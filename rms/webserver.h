#ifndef RMS_WEBSERVER_H
#define RMS_WEBSERVER_H

void setupWebserver();
void loopWebserver();
void handleRoot();
void handleNotFound();
void handleSetThresholds();
void handleChangeRoom();
void handlePause();
void handleResume();
void handleWot();
String getErrorPage(String errorMessage);

#endif // RMS_WEBSERVER_H