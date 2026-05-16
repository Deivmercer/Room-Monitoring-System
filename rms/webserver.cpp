#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "webserver.h"
#include "rms.h"
#include "mqtt_client.h"

ESP8266WebServer server(80);

void setupWebserver()
{
    // Configuring server routes
    server.on("/", handleRoot);
    server.on("/set_thresholds", handleSetThresholds);
    server.on("/pause", handlePause);
    server.on("/resume", handleResume);
    server.on("/change_room", handleChangeRoom);
    server.onNotFound(handleNotFound);
    server.begin();
}

void loopWebserver()
{
    // Listening for clients on port 80
    server.handleClient();
}

void handleRoot()
{
    String warningIconHtml = F("<img style='vertical-align: middle; padding-right: 0.5em' width='25' height='25' src='https://git.davemercer.moe/Davemercer/iot_assignment_1/raw/branch/master/icons/warning_50dp.png'/>");
    State state = getState();
    String roomNumber = getRoomNumber();
    int lastTemperature = getLastTemperature();
    int lastHumidity = getLastHumidity();
    int lastSignalStrength = getLastSignalStrength();
    int lowTemperatureThreshold = getLowTemperatureThreshold();
    int highTemperatureThreshold = getHighTemperatureThreshold();
    int signalStrengthThreshold = getSignalStrengthThreshold();


    String html = F(
        "<!DOCTYPE html>\n"
        "<html>\n"
            "<head>\n"
                "<meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
                "<title>Dashboard</title>\n"
                "<style>\n"
                    "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n"
                    "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n"
                    ".button {display: block;width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n"
                    ".button:active {background-color: #158c75;}\n"
                    "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n"
                "</style>\n"
            "</head>\n"
            "<body>\n"
                "<h1>Last readings:</h1>\n");

    if (state == PAUSED)
        html += F("<p>Status: paused</p>\n");
    else
        html += F("<p>Status: active</p>\n");

    html += F(  "<p>Room: #") + roomNumber + F("</p>\n");

    html += F(  "<p>");
    if (lastTemperature <= lowTemperatureThreshold || lastTemperature >= highTemperatureThreshold)
        html += warningIconHtml;
    html += F(      "Temperature: ") + String(lastTemperature) + F(" &deg;C</p>\n");

    html += F(  "<p>Humidity: ") + String(lastHumidity) + F("%</p>\n");

    html += F(  "<p>");
    if (lastSignalStrength < signalStrengthThreshold)
        html += warningIconHtml;
    html += F(      "WiFi signal strength: ") + String(lastSignalStrength) + F(" dBm</p>\n"
                "<br>\n");

    if (state == PAUSED)
    {
        html += F(
                "<form action='/resume' method='post'>\n"
                    "<input class='button' type='submit' value='Resume'/>\n"
                "</form>\n"
        );
    }
    else
    {
        html += F(
                "<form action='/pause' method='post'>\n"
                    "<input class='button' type='submit' value='Pause'/>\n"
                "</form>\n"
        );
    }

    html += F(  "<br>\n"
                "<h1>Update room ID:</h1>\n"
                "<form action='/change_room' method='post'>\n"
                    "<p>\n"
                        "<label for='room_number'>Room: #</label>\n");
    html += F(          "<input type='number' id='room_number' name='room_number' step='1' min='1' max='") + String(MAX_ROOMS) + F("' value='") + roomNumber + F("'/>\n");
    html += F(      "</p>\n"
                    "<br>\n"
                    "<input class='button' type='submit' value='Update'/>\n"
                "</form>\n"
                "<br>\n"
                "<h1>Configure thresholds:</h1>\n"
                "<form action='/set_thresholds' method='post'>\n"
                    "<p>\n"
                        "<label for='low_temperature'>Low temperature: </label>\n");
    html += F(          "<input type='number' id='low_temperature' name='low_temperature' step='0.01' min='") + String(TEMPERATURE_THRESHOLD_MIN) + F("' max='") + String(TEMPERATURE_THRESHOLD_MAX) + F("' value='") + String(lowTemperatureThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<p>\n"
                        "<label for='high_temperature'>High temperature: </label>\n");
    html += F(          "<input type='number' id='high_temperature' name='high_temperature' step='0.01' min='") + String(TEMPERATURE_THRESHOLD_MIN) + F("' max='") + String(TEMPERATURE_THRESHOLD_MAX) + F("' value='") + String(highTemperatureThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<p>\n"
                        "<label for='signal_strength'>Signal stength: </label>\n");
    html += F(          "<input type='number' id='signal_strength' name='signal_strength' step='1' min='") + String(SIGNAL_STRENGTH_THRESHOLD_MAX) + F("' max='") + String(SIGNAL_STRENGTH_THRESHOLD_MIN) + F("' value='") + String(signalStrengthThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<br>\n"
                    "<input class='button' type='submit' value='Set'/>\n"
                "</form>\n"
            "</body>\n"
        "</html>\n");

    server.send(200, F("text/html"), html);
}

void handleNotFound()
{
    server.send(404, F("text/html"), getErrorPage(F("Page not found")));
}

void handleSetThresholds()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, F("text/html"), getErrorPage(F("Method not allowed")));
        return;
    }
    if (!server.hasArg(F("low_temperature")) || !server.hasArg(F("high_temperature")) || !server.hasArg(F("signal_strength")))
    {
        server.send(400, F("text/html"), getErrorPage(F("Missing arguments")));
        return;
    }

    float newLowTemperature = server.arg(F("low_temperature")).toFloat();
    float newHighTemperature = server.arg(F("high_temperature")).toFloat();
    if (newLowTemperature >= newHighTemperature)
    {
        server.send(400, F("text/html"), getErrorPage(F("The low temperature threshold must be lower than the high temperature threshold")));
        return;
    }
    if (newLowTemperature < TEMPERATURE_THRESHOLD_MIN || newLowTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        server.send(400, F("text/html"), getErrorPage(F("Low temperature threshold out of range")));
        return;
    }
    if (newHighTemperature < TEMPERATURE_THRESHOLD_MIN || newHighTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        server.send(400, F("text/html"), getErrorPage(F("High temperature threshold out of range")));
        return;
    }

    int newSignalStrength = server.arg(F("signal_strength")).toInt();
    if (newSignalStrength < SIGNAL_STRENGTH_THRESHOLD_MAX || newSignalStrength > SIGNAL_STRENGTH_THRESHOLD_MIN)
    {
        server.send(400, F("text/html"), getErrorPage(F("Signal strength threshold out of range")));
        return;
    }

    setLowTemperatureThreshold(newLowTemperature);
    setHighTemperatureThreshold(newHighTemperature);
    setSignalStrengthThreshold(newSignalStrength);

    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    publishThresholds();
}

void handleChangeRoom()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, F("text/html"), getErrorPage(F("Method not allowed")));
        return;
    }
    if (!server.hasArg(F("room_number")))
    {
        server.send(400, F("text/html"), getErrorPage(F("Missing arguments")));
        return;
    }

    int newRoomNumber = server.arg(F("room_number")).toInt();
    if (newRoomNumber < 1 || newRoomNumber > MAX_ROOMS)
    {
        server.send(400, F("text/html"), getErrorPage(F("Room ID out of range")));
        return;
    }

    setRoomNumber(newRoomNumber);

    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    publishDeviceRoom();
}

void handlePause()
{
    setPaused(true);
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    publishDeviceActive();
}

void handleResume()
{
    setPaused(false);
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    publishDeviceActive();
}

String getErrorPage(String errorMessage)
{
    String html = F(
        "<!DOCTYPE html>\n"
        "<html>\n"
            "<head>\n"
                "<meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
                "<title>Dashboard</title>\n"
                "<style>\n"
                    "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n"
                    "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n"
                    ".button {display: block;width: 120px;background-color: #1abc9c;border: none;color: white;padding: 13px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n"
                    ".button:active {background-color: #158c75;}\n"
                    "h1 {color: #FF0000}\n"
                "</style>\n"
            "</head>\n"
            "<body>\n"
                "<h1>ERROR</h1>\n");
    html += F(  "<h2>") + errorMessage + F("</h2>\n");
    html += F(  "<br>\n"
                "<a class='button' href='/'>Home</a>\n"
            "</body>\n"
        "</html>\n");
    
    return html;
}