#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "wifi_client.h"
#include "rms.h"

WiFiServer wifiServer(80);

void setupWifi()
{
    WiFiManager wifiManager;

    // Erase WiFi settings
    // wifiManager.resetSettings();

    wifiManager.autoConnect();

    Serial.println("Connected.");
    printToLCD("Connected. IP:", WiFi.localIP().toString());
    delay(5000);
    clearLcd();
}

void connectToWiFi()
{
}

bool isWifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

int getSignalStrength()
{
    return WiFi.RSSI();
}

String getMacAddress()
{
    return WiFi.macAddress();
}
