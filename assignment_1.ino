#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "secrets.h"

#define POTENTIOMETER A0
#define DHTPIN D1
#define TEMPERATURE_LED D2
#define BUTTON D3
#define DHTTYPE DHT11

void toggleDhtReadFlag();
void readDht();
void printWiFiSignalStrength();

enum State
{
    NORMAL,
    CONFIG_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH
};

DHT dht = DHT(DHTPIN, DHTTYPE);

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

Ticker updateDataTicker;
bool updateData = false;

State state;
float temperatureThreshold = 27.0;
int signalStrengthThreshold = 65;

void setup()
{
    dht.begin();

    WiFi.mode(WIFI_STA);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);

    updateDataTicker.attach(5.0, toggleDhtReadFlag);

    pinMode(TEMPERATURE_LED, OUTPUT);
    digitalWrite(TEMPERATURE_LED, HIGH);

    pinMode(BUTTON, INPUT_PULLUP);

    state = NORMAL;

    Serial.begin(115200);
}

void loop()
{
    static unsigned int potentiometerValue;
    static bool waitButtonReleased = false;

    if (WiFi.status() != WL_CONNECTED)
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

    byte buttonState = digitalRead(BUTTON);
    if (buttonState == LOW && !waitButtonReleased)
    {
        switch (state)
        {
            case NORMAL:
                state = CONFIG_TEMPERATURE;
                Serial.println(F("New state: CONFIG_TEMPERATURE"));
                digitalWrite(TEMPERATURE_LED, HIGH);
                break;
            case CONFIG_TEMPERATURE:
                state = CONFIG_SIGNAL_STRENGTH;
                Serial.println(F("New state: CONFIG_SIGNAL_STRENGTH"));
                break;
            case CONFIG_SIGNAL_STRENGTH:
                state = NORMAL;
                Serial.println(F("New state: NORMAL"));
                break;
            default:
                Serial.println(F("Unknown state"));
        }
        waitButtonReleased = true;
    } else if (buttonState == HIGH && waitButtonReleased)
        waitButtonReleased = false;

    switch (state)
    {
        case NORMAL:
            if (!updateData)
                break;
            readDht();
            printWiFiSignalStrength();
            updateData = false;
            break;
        case CONFIG_TEMPERATURE:
            potentiometerValue = analogRead(POTENTIOMETER);
            temperatureThreshold = linearMapPotentiometerf(potentiometerValue, -15.0, 50.0);
            Serial.print(F("New temperature threshold: "));
            Serial.print(temperatureThreshold);
            Serial.println(F("°C"));
            break;
        case CONFIG_SIGNAL_STRENGTH:
            potentiometerValue = analogRead(POTENTIOMETER);
            signalStrengthThreshold = linearMapPotentiometer(potentiometerValue, 0, -100);
            Serial.print(F("New signal strength threshold: "));
            Serial.print(signalStrengthThreshold);
            Serial.println(F("dbm"));
            break;
        default:
            Serial.println(F("Unknown state"));
    }
}

void toggleDhtReadFlag()
{
    updateData = true;
}

void readDht()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println(F("Failed to read from DHT sensor"));
        return;
    }

    float heatIndex = dht.computeHeatIndex(t, h, false);
    Serial.print(F("Perceived temperature: "));
    Serial.print(heatIndex);
    Serial.println(F("°C"));

    if (heatIndex > temperatureThreshold)
    {
        Serial.print(F("LED on; threshold: "));
        Serial.println(temperatureThreshold);
        digitalWrite(TEMPERATURE_LED, LOW);
    }
    else
    {
        Serial.println(F("LED off"));
        digitalWrite(TEMPERATURE_LED, HIGH);
    }
        
}

void printWiFiSignalStrength()
{
    Serial.print(F("Signal strength (RSSI): "));
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

int linearMapPotentiometer(int value, int min, int max)
{
    return (int) linearMapPotentiometerf(value, min, max);
}

float linearMapPotentiometerf(int value, float min, float max)
{
    return min + (value - 1) * (max - min) / (1024 - 1);
}