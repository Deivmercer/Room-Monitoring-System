#ifndef WEBSERVER_H
#define WEBSERVER_H

void setupWebserver();
void loopWebserver();
void handleRoot();
void handleNotFound();
void handleSetThresholds();
void handleChangeRoom();
void handlePause();
void handleResume();
String getErrorPage(String errorMessage);

#endif // WEBSERVER_H