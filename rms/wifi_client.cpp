#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "wifi_client.h"
#include "rms.h"

WiFiServer wifiServer(80);

bool firstSetup;

void setupWifi()
{
    printToLCD("Setup in", "progress...");

    WiFiManager wifiManager;

    firstSetup = !wifiManager.getWiFiIsSaved();

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

String getIpAddress()
{
    return WiFi.localIP().toString();
}

bool isFirstSetup()
{
    return isFirstSetup;
}
