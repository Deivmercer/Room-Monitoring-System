#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "secrets.h"

#define DHTPIN D1
#define DHTTYPE DHT11
#define TEMPERATURE_LED D2

void toggleDhtReadFlag();
void readDht();
void printWiFiSignalStrength();

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

void setup()
{
    dht.begin();

    WiFi.mode(WIFI_STA);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);

    Serial.begin(115200);

    updateDataTicker.attach(5.0, toggleDhtReadFlag);

    pinMode(TEMPERATURE_LED, OUTPUT);
    digitalWrite(TEMPERATURE_LED, HIGH);
}

void loop()
{
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
        }g
        Serial.println();
    }

    if (!updateData)
        return;

    readDht();
    printWiFiSignalStrength();
    updateData = false;
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

    if (heatIndex > 24.0)
        digitalWrite(TEMPERATURE_LED, LOW);
    else
        digitalWrite(TEMPERATURE_LED, HIGH);
}

void printWiFiSignalStrength()
{
    Serial.print(F("Signal strength (RSSI): "));
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}
