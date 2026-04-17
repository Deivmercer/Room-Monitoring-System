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
        case CONFIG_LOW_TEMPERATURE:
            configureLowTemperatureThreshold();
            break;
        case CONFIG_HIGH_TEMPERATURE:
            configureHighTemperatureThreshold();
            break;
        case CONFIG_SIGNAL_STRENGTH:
            configureSignalStrengthThreshold();
            break;
        default:
            Serial.println(F("ERROR: Unknown state"));
    }

    updateLEDs();
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
            state = NORMAL;
            lcd.clear();
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
    lcd.print(" dBm");
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

void printToLCD(const char* displayLine1, const char* displayLine2)
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
