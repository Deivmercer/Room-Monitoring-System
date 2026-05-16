#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <InfluxDbClient.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include "assignment_1.h"
#include "webserver.h"

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pointDevice("room_status");

MQTTClient mqttClient(MQTT_BUFFER_SIZE);
WiFiClient networkClient;
String mqttMainTopic;

DHT dht = DHT(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

Ticker updateDataTicker;
bool updateData = false;

State state;

bool isActive = true;
String roomNumber = DEFAULT_ROOM_NUMBER;
float lowTemperatureThreshold = -1.0;
float highTemperatureThreshold = 27.0;
int signalStrengthThreshold = -65;
float lastHumidity = 0;
float lastTemperature = 0;
int lastSignalStrength = 0;

void setup()
{
    dht.begin();

    WiFi.mode(WIFI_STA);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);

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

    pointDevice.addTag("room_number", roomNumber);

    setupWebserver();

    // setup MQTT
    mqttClient.begin(MQTT_BROKER_IP, 1883, networkClient);
    mqttClient.onMessage(onMqttMessageReceived);

    Serial.begin(115200);
}

void loop()
{
    // Will be used to prevent the the system state to keep on changing when the button is being hold pressed
    static bool waitButtonReleased = false;

    // Checking if WiFi is connected
    if (WiFi.status() != WL_CONNECTED)
        connectToWiFi();
    if (!mqttClient.connected())
        connectToMqttBroker();

    byte buttonState = digitalRead(BUTTON);
    if (buttonState == LOW && !waitButtonReleased)
    {
        updateState();
        waitButtonReleased = true;
    } else if (buttonState == HIGH && waitButtonReleased)
        waitButtonReleased = false;

    switch (state)
    {
        case NORMAL:
            if (!updateData)
                break;
            readDht();
            readSignalStrength();
            updateDisplay();
            writeReadingsToDB();
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

    updateLEDs();
    loopWebserver();
    mqttClient.loop();
}

void connectToWiFi()
{
    lcd.clear();
    Serial.print(F("\nConnecting to SSID: "));
    Serial.println(SECRET_SSID);

#ifdef IP
    WiFi.config(ip, dns, gateway, subnet);
#endif

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        switch(WiFi.status())
        {
            case WL_NO_SSID_AVAIL:
                printToLCD("SSID not in", "range. Retrying");
                delay(5000);
                lcd.clear();
                break;
            case WL_CONNECT_FAILED:
                printToLCD("WiFi connection", "failed. Retrying");
                delay(5000);
                lcd.clear();
                break;
            case WL_WRONG_PASSWORD:
                printToLCD("WiFi password", "is wrong.");
                while(true)
                    delay(100);
                break;
            case WL_CONNECTION_LOST:
                printToLCD("WiFi connection", "lost");
                delay(250);
                lcd.clear();
                break;
            default:
                printToLCD("Setup in", "progress...");
                Serial.print(F("."));
                delay(250);
                break;
        }
    }

    Serial.println();
    printToLCD("Connected. IP:", WiFi.localIP().toString());
    delay(5000);
    lcd.clear();
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
            publishThresholds();
            break;
        case CONFIG_ROOM_NUMBER:
            state = NORMAL;
            pointDevice.clearTags();
            pointDevice.addTag("room_number", roomNumber);
            lcd.clear();
            publishDeviceRoom();
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

    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_TEMPERATURE).c_str(), String(lastTemperature), false, 0);
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_HUMIDITY).c_str(), String(lastHumidity), false, 0);
}

void readSignalStrength()
{
    lastSignalStrength = WiFi.RSSI();
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_SIGNAL).c_str(), String(lastSignalStrength), false, 0);
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

void updateLEDs()
{
    if (lastTemperature <= lowTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, HIGH);
    }
    else if (lastTemperature >= highTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, HIGH);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
    }
    else
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
    }

    if (lastSignalStrength < signalStrengthThreshold)
        digitalWrite(SIGNAL_STRENGTH_LED, LOW);
    else
        digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
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

void writeReadingsToDB()
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
    pointDevice.addField("temperature", lastTemperature);
    pointDevice.addField("humidity", lastHumidity);
    pointDevice.addField("signal_strength", lastSignalStrength);
    if (!client_idb.writePoint(pointDevice))
    {
        printToLCD("Failed to write", "to InfluxDB");
        delay(2000);
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb.getLastErrorMessage());
    }
}

void connectToMqttBroker()
{
    char macAddress[18];
    strcpy(macAddress, WiFi.macAddress().c_str());
    char mainTopic[23];
    snprintf(mainTopic, sizeof(mainTopic), MQTT_TOPIC_MAIN, macAddress);
    mqttMainTopic = mainTopic;

    JsonDocument lwt;
    lwt["status"] = F("disconnected");
    lwt["mac_address"] = macAddress;
    char lwt_buffer[MQTT_BUFFER_SIZE];
    serializeJson(lwt, lwt_buffer);
    mqttClient.setWill(MQTT_TOPIC_DEVICES, lwt_buffer, false, 2);

    Serial.print(F("Connecting to MQTT broker..."));
    while(!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
    {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("\nConnected"));
    JsonDocument json;
    json["status"] = F("connected");
    json["mac_address"] = macAddress;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish(MQTT_TOPIC_DEVICES, buffer, length, false, 2);
    publishDeviceActive();

    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ROOM_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ACTIVE_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE);
}

void onMqttMessageReceived(String& topic, String& payload)
{
    JsonDocument json;
    deserializeJson(json, payload);
    Serial.println("Received MQTT message: " + topic + " - " + payload);

    if (topic.endsWith(MQTT_SUBTOPIC_ROOM_UPDATE))
        mqttUpdateRoom(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_ACTIVE_UPDATE))
        mqttUpdateActive(payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE))
        mqttUpdateTemperatureMinThreshold(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE))
        mqttUpdateTemperatureMaxThreshold(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE))
        mqttUpdateSignalStrengthThreshold(topic, payload);
    else
        Serial.println("Ignored");
}

void publishDeviceRoom()
{
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_ROOM).c_str(), String(roomNumber), true, 2);
}

void mqttUpdateRoom(String topic, String room)
{
    int newRoomNumber = room.charAt(0) - 48;
    if (newRoomNumber < 1 || newRoomNumber > MAX_ROOMS)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "error", F("Room ID out of range"));
        return;
    }

    roomNumber = newRoomNumber;

    pointDevice.clearTags();
    pointDevice.addTag("room_number", roomNumber);
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "ok");
    publishDeviceRoom();
}

void publishRequestResult(const char* topic, String result)
{
    publishRequestResult(topic, result, "");
}

void publishRequestResult(const char* topic, String result, String error)
{
    JsonDocument json;
    json["result"] = result;
    if (result == F("error"))
        json["error"] = error;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish(topic, buffer, length, false, 0);
}

void mqttUpdateActive(String active)
{
    bool isActive = active.equals("true");
    if (isActive)
        state = NORMAL;
    else
        state = PAUSED;
    publishDeviceActive();
}

void publishDeviceActive()
{
    String active;
    if (state != PAUSED)
        active = F("true");
    else
        active = F("false");
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_ACTIVE).c_str(), active, true, 2);
}

void mqttUpdateTemperatureMinThreshold(String topic, String threshold)
{
    float newLowTemperature = threshold.toFloat();
    if (newLowTemperature >= highTemperatureThreshold)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("The low temperature threshold must be lower than the high temperature threshold"));
        return;
    }
    if (newLowTemperature < TEMPERATURE_THRESHOLD_MIN || newLowTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Low temperature threshold out of range"));
        return;
    }

    lowTemperatureThreshold = newLowTemperature;
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void mqttUpdateTemperatureMaxThreshold(String topic, String threshold)
{
    float newHighTemperature = threshold.toFloat();
    if (newHighTemperature < TEMPERATURE_THRESHOLD_MIN || newHighTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("High temperature threshold out of range"));
        return;
    }

    highTemperatureThreshold = newHighTemperature;
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void mqttUpdateSignalStrengthThreshold(String topic, String threshold)
{
    int newSignalStrength = threshold.toInt();
    if (newSignalStrength < SIGNAL_STRENGTH_THRESHOLD_MAX || newSignalStrength > SIGNAL_STRENGTH_THRESHOLD_MIN)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Signal strength threshold out of range"));
        return;
    }

    signalStrengthThreshold = newSignalStrength;
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void publishThresholds()
{
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP).c_str(), String(lowTemperatureThreshold), true, 0);
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP).c_str(), String(highTemperatureThreshold), true, 0);
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL).c_str(), String(signalStrengthThreshold), true, 0);
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
