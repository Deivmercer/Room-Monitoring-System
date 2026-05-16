#ifndef ASSIGNMENT_WIFI_CLIENT_H
#define ASSIGNMENT_WIFI_CLIENT_H

void setupWifi();
void connectToWiFi();
bool isWifiConnected();
int getSignalStrength();
String getMacAddress();
String getIpAddress();

#endif //ASSIGNMENT_WIFI_CLIENT_H
