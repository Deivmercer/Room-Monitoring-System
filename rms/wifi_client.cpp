#include <ESP8266WiFi.h>
#include "wifi_client.h"
#include "rms.h"
#include "secrets.h"

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

void setupWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);
}

void connectToWiFi()
{
    clearLcd();
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
                clearLcd();
                break;
            case WL_CONNECT_FAILED:
                printToLCD("WiFi connection", "failed. Retrying");
                delay(5000);
                clearLcd();
                break;
            case WL_WRONG_PASSWORD:
                printToLCD("WiFi password", "is wrong.");
                while(true)
                    delay(100);
                break;
            case WL_CONNECTION_LOST:
                printToLCD("WiFi connection", "lost");
                delay(250);
                clearLcd();
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
    clearLcd();
}

bool isWifiConnected()
{
    return WiFi.status() != WL_CONNECTED;
}

int getSignalStrength()
{
    return WiFi.RSSI();
}

String getMacAddress()
{
    return WiFi.macAddress();
}
