#ifndef RMS_WIFI_CLIENT_H
#define RMS_WIFI_CLIENT_H

void setupWifi();
void connectToWiFi();
bool isWifiConnected();
int getSignalStrength();
String getMacAddress();
String getIpAddress();
bool isFirstSetup();
String sendHttpRequest(const char* url, int port, char* request);
void enterSleep();
void exitSleep();

#endif // RMS_WIFI_CLIENT_H
