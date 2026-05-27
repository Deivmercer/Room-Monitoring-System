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

String sendHttpRequest(const char* url, int port, char* request)
{
    WiFiClient client;

    if (client.connect(url, port))
    {
        client.println(request);
        client.print(F("Host: "));
        client.println(url);
        client.println(F("User-Agent: ArduinoWiFi/1.1"));
        client.println(F("Connection: close"));
        client.println();
    } else {
        Serial.print(F("Connection to "));
        Serial.print(url);
        Serial.println(F(" failed!\n"));
    }

    while (client.connected() && !client.available())
        delay(1); // Waiting for data

    String result;
    while (client.connected() || client.available())
    {   
        // Reading data
        char c = client.read();
        result = result + c;
    }

    client.stop(); // Closing communication

    return result;
}
