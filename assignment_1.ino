#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "assignment_1.h"

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

DHT dht = DHT(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

Ticker updateDataTicker;
bool updateData = false;

State state;

float temperatureThreshold = 27.0;
int signalStrengthThreshold = 65;
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
        lcd.clear();
        lcd.print("Setup in");
        lcd.setCursor(0, 1);
        lcd.print("progress...");
    } else {
        Serial.print(F("LCD not found. Error "));
        Serial.println(error);
    }

    // Initializing ticker
    updateDataTicker.attach(5.0, toggleReadingFlag);

    // Setting pins
    pinMode(TEMPERATURE_LED, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);

    // Turning LED off
    digitalWrite(TEMPERATURE_LED, HIGH);

    // Init system state
    state = NORMAL;

    Serial.begin(115200);
}

void loop()
{
    // Will be used to prevent the the system state to keep on changing when the button is being hold pressed
    static bool waitButtonReleased = false;

    // Checking if WiFi is connected
    if (WiFi.status() != WL_CONNECTED)
        connectToWiFi();

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
            lastSignalStrength = WiFi.RSSI();
            updateDisplay();
            updateData = false;
            break;
        case CONFIG_TEMPERATURE:
            configureTemperatureThreshold();
            break;
        case CONFIG_SIGNAL_STRENGTH:
            configureSignalStrengthThreshold();
            break;
        default:
            Serial.println(F("Unknown state"));
    }
}

void connectToWiFi()
{
    Serial.print(F("Connecting to SSID: "));
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
                Serial.println(F("\nSSID not in range"));
                delay(5000);
                break;
            case WL_CONNECT_FAILED:
                Serial.println(F("\nWiFi connection failed!"));
                delay(5000);
                break;
            case WL_WRONG_PASSWORD:
                Serial.println(F("WiFi password is wrong!"));
                while(true)
                    delay(100);
                break;
            case WL_CONNECTION_LOST:
                Serial.println(F("\nWiFi connection lost!"));
                delay(250);
                break;
            default:
                Serial.print(F("."));
                delay(250);
                break;
        }
    }
    Serial.println();
}

void updateState()
{
    switch (state)
    {
        case NORMAL:
            state = CONFIG_TEMPERATURE;
            digitalWrite(TEMPERATURE_LED, HIGH);
            lcd.clear();
            break;
        case CONFIG_TEMPERATURE:
            state = CONFIG_SIGNAL_STRENGTH;
            break;
        case CONFIG_SIGNAL_STRENGTH:
            state = NORMAL;
            lcd.clear();
            break;
        default:
            Serial.println(F("Unknown state"));
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

    if (heatIndex > temperatureThreshold)
        digitalWrite(TEMPERATURE_LED, LOW);
    else
        digitalWrite(TEMPERATURE_LED, HIGH);
}

void updateDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", lastTemperature);
    lcd.print(buffer);
    lcd.print(" C ");
    lcd.setCursor(0, 1);
    lcd.print("WiFi: ");
    lcd.print(lastSignalStrength);
    lcd.print(" dBm");
}

void configureTemperatureThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between TEMPERATURE_THRESHOLD_MIN and TEMPERATURE_THRESHOLD_MAX
    temperatureThreshold = linearMapPotentiometerf(potentiometerValue,
                                                    TEMPERATURE_THRESHOLD_MIN,
                                                    TEMPERATURE_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("Temperature");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", temperatureThreshold);
    lcd.print(buffer);
    lcd.print(" C");
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
    lcd.print("thres.: -");
    lcd.print(signalStrengthThreshold);
    lcd.print(" dBm");
}

int linearMapPotentiometer(int value, int min, int max)
{
    return (int) linearMapPotentiometerf(value, min, max);
}

float linearMapPotentiometerf(int value, float min, float max)
{
    return min + (value - POTENTIOMETER_MIN) * (max - min) / (POTENTIOMETER_MAX - POTENTIOMETER_MIN);
}
