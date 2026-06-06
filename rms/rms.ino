#include <DHT.h>
#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <InfluxDbClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "rms.h"
#include "wifi_client.h"
#include "webserver.h"
#include "mqtt_client.h"
#include "secrets.h"

InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pointDevice("room_status");

DHT dht = DHT(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

Ticker updateDataTicker;
bool updateData = false;

State state;

char weatherCountry[] = WEATHER_DEFAULT_COUNTRY;
char weatherCity[] = WEATHER_DEFAULT_CITY;

String roomNumber = DEFAULT_ROOM_NUMBER;
float lowTemperatureThreshold = -1.0;
float highTemperatureThreshold = 27.0;
int signalStrengthThreshold = -65;
float lastHumidity = 0;
float lastTemperature = 0;
int lastSignalStrength = 0;
int savedData = 0;

void setup()
{
    dht.begin();

    // Initializing the LCD display
    Wire.begin();
    Wire.beginTransmission(DISPLAY_ADDR);
    byte error = Wire.endTransmission();
    if (error == 0) {
        Serial.println(F("LCD found."));
        lcd.begin(DISPLAY_CHARS, 2);
        lcd.setBacklight(255);
        lcd.home();
    } else {
        Serial.print(F("LCD not found. Error "));
        Serial.println(error);
    }

    setupWifi();

    // Initializing ticker
    updateDataTicker.attach(5.0, toggleReadingFlag);

    // Setting pins
    pinMode(SIGNAL_STRENGTH_LED, OUTPUT);
    pinMode(HIGH_TEMPERATURE_LED, OUTPUT);
    pinMode(LOW_TEMPERATURE_LED, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);

    // Turning LED off
    digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
    digitalWrite(HIGH_TEMPERATURE_LED, LOW);
    digitalWrite(LOW_TEMPERATURE_LED, LOW);

    // Init system state
    state = NORMAL;

    // Setting up InfluxDB client
    client_idb.setWriteOptions(WriteOptions().writePrecision(WritePrecision::S)); // Setting timestamp format
    pointDevice.addTag("room_number", roomNumber);

    setupWebserver();
    setupMqtt(isFirstSetup());

    // Retrieving UTC time via NTP
    configTime(0, 0, "it.pool.ntp.org"); 

    // Init EEPROM
    EEPROM.begin(512);

    Serial.begin(115200);
}

void loop()
{
    // Will be used to prevent the the system state to keep on changing when the button is being hold pressed
    static bool waitButtonReleased = false;

    byte buttonState = digitalRead(BUTTON);
    if (buttonState == LOW && !waitButtonReleased)
    {
        updateState();
        waitButtonReleased = true;
    } else if (buttonState == HIGH && waitButtonReleased)
        waitButtonReleased = false;

    int32_t now;
    switch (state)
    {
        case NORMAL:
            if (!updateData)
                break;
            readDht();
            readSignalStrength();
            updateDisplay();
            writeReadingsToDB(lastTemperature, lastHumidity, lastSignalStrength, -1);
            updateData = false;
            state = WEATHER;
            break;
        case WEATHER:
            if (!updateData)
                break;
            showWeatherInfo();
            readDht();
            readSignalStrength();
            writeReadingsToDB(lastTemperature, lastHumidity, lastSignalStrength, -1);
            updateData = false;
            state = NORMAL;
            break;
        case NIGHT_MODE:
            if (!updateData)
                break;
            now = time(nullptr);
            readDht();
            readSignalStrength();
            writeDataToEEPROM(now);
            updateData = false;
            break;
        case CONFIG_LOW_TEMPERATURE:
            configureLowTemperatureThreshold();
            break;
        case CONFIG_HIGH_TEMPERATURE:
            configureHighTemperatureThreshold();
            break;
        case CONFIG_SIGNAL_STRENGTH:
            configureSignalStrengthThreshold();
            break;
        case CONFIG_ROOM_NUMBER:
            configureRoomNumber();
            break;
        case PAUSED:
            lcd.clear();
            printToLCD("Paused", "");
            break;
        default:
            Serial.println(F("ERROR: Unknown state"));
    }

    if (state == NIGHT_MODE)
        return;

    loopWebserver();
    loopMqttClient();
}

void updateState()
{
    switch (state)
    {
        case NORMAL:
            state = CONFIG_LOW_TEMPERATURE;
            digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
            lcd.clear();
            break;
        case CONFIG_LOW_TEMPERATURE:
            state = CONFIG_HIGH_TEMPERATURE;
            break;
        case CONFIG_HIGH_TEMPERATURE:
            if (lowTemperatureThreshold >= highTemperatureThreshold)
            {
                printToLCD("ERROR:low thres", "higher than high");
                delay(2000);
                lcd.clear();
                state = CONFIG_LOW_TEMPERATURE;
            }
            else
            {
                state = CONFIG_SIGNAL_STRENGTH;
                lcd.clear();
            }
            break;
        case CONFIG_SIGNAL_STRENGTH:
            state = CONFIG_ROOM_NUMBER;
            lcd.clear();
            mqttPublishThresholds();
            break;
        case CONFIG_ROOM_NUMBER:
            state = NORMAL;
            pointDevice.clearTags();
            pointDevice.addTag("room_number", roomNumber);
            lcd.clear();
            mqttPublishDeviceRoom();
            break;
        case NIGHT_MODE:
            enterSleep();
            digitalWrite(HIGH_TEMPERATURE_LED, LOW);
            digitalWrite(LOW_TEMPERATURE_LED, LOW);
            digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
            lcd.clear();
            lcd.setBacklight(0);
            updateDataTicker.detach();
            updateDataTicker.attach(60.0, toggleReadingFlag);
            break;
        case NIGHT_MODE_RESTORE:
            exitSleep();
            readEEPROM();
            state = NORMAL;
            updateDataTicker.detach();
            updateDataTicker.attach(5.0, toggleReadingFlag);
            break;
        case AWAY_MODE:
            digitalWrite(HIGH_TEMPERATURE_LED, LOW);
            digitalWrite(LOW_TEMPERATURE_LED, LOW);
            digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
            lcd.clear();
            lcd.setBacklight(0);
            ESP.deepSleep(0);
            break;
        default:
            Serial.println(F("ERROR: Unknown state"));
    }
}

void toggleReadingFlag()
{
    updateData = true;
}

void readDht()
{
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature))
    {
        Serial.println(F("Failed to read from DHT sensor"));
        return;
    }

    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    lastTemperature = heatIndex;
    lastHumidity = humidity;

    if (state == NIGHT_MODE)
        return;

    if (lastTemperature <= lowTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, HIGH);
        if (state == NORMAL)
            mqttPublishAlarm(MQTT_SUBTOPIC_ALARMS_TEMPERATURE, F("Temperature is too low!"));
    }
    else if (lastTemperature >= highTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, HIGH);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
        if (state == NORMAL)
            mqttPublishAlarm(MQTT_SUBTOPIC_ALARMS_TEMPERATURE, F("Temperature is too high!"));
    }
    else
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
    }

    mqttPublishData(MQTT_SUBTOPIC_TEMPERATURE, String(lastTemperature));
    mqttPublishData(MQTT_SUBTOPIC_HUMIDITY, String(lastTemperature));
}

void readSignalStrength()
{
    lastSignalStrength = getSignalStrength();

    if (state == NIGHT_MODE)
        return;

    if (lastSignalStrength < signalStrengthThreshold)
    {
        digitalWrite(SIGNAL_STRENGTH_LED, LOW);
        if (state == NORMAL)
            mqttPublishAlarm(MQTT_SUBTOPIC_ALARMS_SIGNAL, F("WiFi signal is too weak!"));
    }
    else
        digitalWrite(SIGNAL_STRENGTH_LED, HIGH);

    mqttPublishData(MQTT_SUBTOPIC_SIGNAL, String(lastSignalStrength));
}

void updateDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("T:");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", lastTemperature);
    lcd.print(buffer);
    lcd.print(" C  H:");
    lcd.print(int(lastHumidity));
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("WiFi:");
    lcd.print(lastSignalStrength);
    lcd.print(" dBm  #");
    lcd.print(roomNumber);
}

void configureLowTemperatureThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between TEMPERATURE_THRESHOLD_MIN and TEMPERATURE_THRESHOLD_MAX
    lowTemperatureThreshold = linearMapPotentiometerf(potentiometerValue,
                                                      TEMPERATURE_THRESHOLD_MIN,
                                                      TEMPERATURE_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("Low temperature");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", lowTemperatureThreshold);
    lcd.print(buffer);
    lcd.print(" C ");
}

void configureHighTemperatureThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between TEMPERATURE_THRESHOLD_MIN and TEMPERATURE_THRESHOLD_MAX
    highTemperatureThreshold = linearMapPotentiometerf(potentiometerValue,
                                                       TEMPERATURE_THRESHOLD_MIN,
                                                       TEMPERATURE_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("High temperature");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", highTemperatureThreshold);
    lcd.print(buffer);
    lcd.print(" C ");
}

void configureSignalStrengthThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between SIGNAL_STRENGTH_THRESHOLD_MIN and SIGNAL_STRENGTH_THRESHOLD_MAX
    signalStrengthThreshold = linearMapPotentiometer(potentiometerValue,
                                                     SIGNAL_STRENGTH_THRESHOLD_MIN,
                                                     SIGNAL_STRENGTH_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("WiFi signal");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    lcd.print(signalStrengthThreshold);
    lcd.print(" dBm");
}

void configureRoomNumber()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between 1 and MAX_ROOMS
    roomNumber = String(linearMapPotentiometer(potentiometerValue, 1, MAX_ROOMS));
    lcd.setCursor(0, 0);
    lcd.print("Room number: ");
    lcd.setCursor(0, 1);
    lcd.print("#");
    lcd.print(roomNumber);
}

void printToLCD(String displayLine1, String displayLine2)
{
    lcd.setCursor(0, 0);
    lcd.print(displayLine1);
    lcd.setCursor(0, 1);
    lcd.print(displayLine2);
}

int linearMapPotentiometer(int value, int min, int max)
{
    return (int) linearMapPotentiometerf(value, min, max);
}

float linearMapPotentiometerf(int value, float min, float max)
{
    return min + (value - POTENTIOMETER_MIN) * (max - min) / (POTENTIOMETER_MAX - POTENTIOMETER_MIN);
}

void clearLcd()
{
    lcd.clear();
}

void writeReadingsToDB(float temperature, float humidity, int singalStrength, time_t timestamp)
{
    // Checking server connection
    if (!client_idb.validateConnection())
    {
        lcd.clear();
        printToLCD("InfluxDB", "not connected");
        delay(2000);
        return;
    }

    pointDevice.clearFields();
    pointDevice.addField("temperature", temperature);
    pointDevice.addField("humidity", humidity);
    pointDevice.addField("signal_strength", singalStrength);
    if (timestamp > 0)
        pointDevice.setTime(timestamp);

    if (!client_idb.writePoint(pointDevice))
    {
        printToLCD("Failed to write", "to InfluxDB");
        delay(2000);
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb.getLastErrorMessage());
    }
}

State getState()
{
    return state;
}

float getLastTemperature()
{
    return lastTemperature;
}

float getLastHumidity()
{
    return lastHumidity;
}

int getLastSignalStrength()
{
    return lastSignalStrength;
}

float getLowTemperatureThreshold()
{
    return lowTemperatureThreshold;
}

void setLowTemperatureThreshold(float newLowTemperatureThreshold)
{
    lowTemperatureThreshold = newLowTemperatureThreshold;
}

float getHighTemperatureThreshold()
{
    return highTemperatureThreshold;
}

void setHighTemperatureThreshold(float newHighTemperatureThreshold)
{
    highTemperatureThreshold = newHighTemperatureThreshold;
}

int getSignalStrengthThreshold()
{
    return signalStrengthThreshold;
}

void setSignalStrengthThreshold(int newSignalStrengthThreshold)
{
    signalStrengthThreshold = newSignalStrengthThreshold;
}

String getRoomNumber()
{
    return roomNumber;
}

void setRoomNumber(int newRoomNumber)
{
    roomNumber = newRoomNumber;
    pointDevice.clearTags();
    pointDevice.addTag("room_number", roomNumber);
}

void setPaused(bool paused)
{
    state = paused ? PAUSED : NORMAL;
}

void writeDataToEEPROM(time_t timestamp)
{
    RomData data = {lastTemperature, lastHumidity, timestamp, lastSignalStrength};
    EEPROM.put(savedData * sizeof(RomData), data);
    ++savedData;
}

void readEEPROM()
{
    if (!client_idb.validateConnection())
    {
        savedData = 0;
        return;
    }

    for (int i = 0; i < savedData; ++i)
    {
        RomData data;
        EEPROM.get(i * sizeof(RomData), data);
        writeReadingsToDB(data.temperature, data.humidity, data.signalStrength, data.timestamp);
    }
    savedData = 0;
}

void showWeatherInfo()
{
    char request[100];
    sprintf(request, OWM_QUERY, weatherCity, weatherCity, OWM_API_KEY);
    String result = sendHttpRequest(OWM_URL, OWM_PORT, request);

    char jsonArray[result.length() + 1];
    result.toCharArray(jsonArray, sizeof(jsonArray));
    jsonArray[result.length() + 1] = '\0';
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonArray);

    if (error)
    {
        Serial.print(F("Failed to deserialize JSON: "));
        Serial.println(error.c_str());
        return;
    }

    char buffer[6];
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Weather:"));
    lcd.print(doc["weather"][0]["main"].as<String>());
    lcd.setCursor(0, 1);
    lcd.print(F("Lo:"));
    snprintf(buffer, sizeof(buffer), "%.1f", (float) doc["main"]["temp_min"]);
    lcd.print(buffer);
    lcd.print(F(" Hi:"));
    snprintf(buffer, sizeof(buffer), "%.1f", (float) doc["main"]["temp_max"]);
    lcd.print(buffer);
}
