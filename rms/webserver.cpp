#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "webserver.h"
#include "rms.h"
#include "wifi_client.h"
#include "mqtt_client.h"
#include "secrets.h"

ESP8266WebServer server(80);

void setupWebserver()
{
    // Configuring server routes
    server.on("/", handleRoot);
    server.on("/set_thresholds", handleSetThresholds);
    server.on("/pause", handlePause);
    server.on("/resume", handleResume);
    server.on("/change_room", handleChangeRoom);
    server.on("/wot", handleWot);
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
    mqttPublishThresholds();
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
    mqttPublishDeviceRoom();
}

void handlePause()
{
    setPaused(true);
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    mqttPublishDeviceActive();
}

void handleResume()
{
    setPaused(false);
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
    mqttPublishDeviceActive();
}

void handleWot()
{
    String macAddress = getMacAddress();
    String mqttBroker = String(MQTT_BROKER_IP);
    String ipAddress = getIpAddress();

    String json = F(
        "{\n"
            "\"@context\": \"https://www.w3.org/2019/wot/td/v1.1\",\n"
            "\"title\": \"Room Monitoring System\",\n");
    json += F("\"id\": \"") + macAddress + F("\",\n");
    json += F("\"properties\": {\n"
                "\"temperature\": {\n"
                    "\"description\": \"Latest temperature value\",\n"
                    "\"type\": \"float\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_TEMPERATURE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"humidity\": {\n"
                    "\"description\": \"Latest humidity value\",\n"
                    "\"type\": \"float\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_HUMIDITY + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"signalStrength\": {\n"
                    "\"description\": \"Latest WiFi signal strength value\",\n"
                    "\"type\": \"int\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_SIGNAL + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"room\": {\n"
                    "\"description\": \"The room where this device is located (1 - 9)\",\n"
                    "\"type\": \"int\",\n"
                    "\"readOnly\": false,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ROOM + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"minTemperatureThreshold\": {\n"
                    "\"description\": \"The lowest acceptable temperature value (-15 - 50)\",\n"
                    "\"type\": \"float\",\n"
                    "\"readOnly\": false,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"maxTemperatureThreshold\": {\n"
                    "\"description\": \"The highest acceptable temperature value (-15 - 50)\",\n"
                    "\"type\": \"float\",\n"
                    "\"readOnly\": false,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"signalStrengthThreshold\": {\n"
                    "\"description\": \"The lower acceptable signal strength value (-100 - 0)\",\n"
                    "\"type\": \"int\",\n"
                    "\"readOnly\": false,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"isActive\": {\n"
                    "\"description\": \"The current status of the device (true/false)\",\n"
                    "\"type\": \"bool\",\n"
                    "\"readOnly\": false,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ACTIVE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"temperatureAlarms\": {\n"
                    "\"description\": \"The temperature-related alarms\",\n"
                    "\"type\": \"string\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ALARMS_TEMPERATURE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"signalStrengthAlarms\": {\n"
                    "\"description\": \"The signal strength-related alarms\",\n"
                    "\"type\": \"string\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ALARMS_SIGNAL + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"minTemperatureUpdateAck\": {\n"
                    "\"description\": \"Acks related to min temperature threshold update requests\",\n"
                    "\"type\": \"object\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE + MQTT_SUBTOPIC_ACK + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"maxTemperatureUpdateAck\": {\n"
                    "\"description\": \"Acks related to max temperature threshold update requests\",\n"
                    "\"type\": \"object\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE + MQTT_SUBTOPIC_ACK + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"signalStrengthUpdateAck\": {\n"
                    "\"description\": \"Acks related to signal strength threshold update requests\",\n"
                    "\"type\": \"object\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE + MQTT_SUBTOPIC_ACK + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"roomUpdateAck\": {\n"
                    "\"description\": \"Acks related to room update requests\",\n"
                    "\"type\": \"object\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ROOM_UPDATE + MQTT_SUBTOPIC_ACK + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"webOfThings\": {\n"
                    "\"description\": \"Returns the Web of Things definition of this device (i.e. this resource)\",\n"
                    "\"type\": \"object\",\n"
                    "\"readOnly\": true,\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"http://") + ipAddress + F("/wot\",\n");
    json += F(              "\"contentType\": \"application/json\"\n"
                        "}\n"
                    "]\n"
                "}\n"                
            "},\n"
            "\"actions\": {\n"
                "\"updateStatus\": {\n"
                    "\"input\": {\n"
                        "\"type\": \"bool\",\n"
                        "\"description\": \"The new status of the device\"\n"
                    "},\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ACTIVE_UPDATE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"updateMinTemperature\": {\n"
                    "\"input\": {\n"
                        "\"type\": \"float\",\n"
                        "\"description\": \"The new lower temperature threshold. Must be lower than maxTemperatureThreshold\"\n"
                    "},\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"updateMaxTemperature\": {\n"
                    "\"input\": {\n"
                        "\"type\": \"float\",\n"
                        "\"description\": \"The new higher temperature threshold. Must be higher than minTemperatureThreshold\"\n"
                    "},\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"updateSignalStrength\": {\n"
                    "\"input\": {\n"
                        "\"type\": \"int\",\n"
                        "\"description\": \"The new signal strength threshold\"\n"
                    "},\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"updateRoom\": {\n"
                    "\"input\": {\n"
                        "\"type\": \"int\",\n"
                        "\"description\": \"The new room\"\n"
                    "},\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"mqtt://") + mqttBroker + F("/rms/") + macAddress + MQTT_SUBTOPIC_ROOM_UPDATE + F("\",\n");
    json += F(              "\"contentType\": \"text/plain\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"pause\": {\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"http://") + ipAddress + F("/pause\",\n");
    json += F(              "\"htv:methodName\": \"GET\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"resume\": {\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"http://") + ipAddress + F("/resume\",\n");
    json += F(              "\"htv:methodName\": \"GET\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"setThresholds\": {\n"
                    "\"input\": {\n"
                        "\"low_temperature\": {\n"
                            "\"type\": \"float\"\n"
                        "},\n"
                        "\"high_temperature\": {\n"
                            "\"type\": \"float\"\n"
                        "},\n"
                        "\"signal_strength\": {\n"
                            "\"type\": \"int\"\n"
                        "}\n"
                    "},\n"
                    "\"required\": [\"low_temperature\", \"high_temperature\", \"signal_strength\"],\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"http://") + ipAddress + F("/set_thresholds\",\n");
    json += F(              "\"contentType\": \"application/x-www-form-urlencoded\",\n"
                            "\"htv:methodName\": \"POST\"\n"
                        "}\n"
                    "]\n"
                "},\n"
                "\"changeRoom\": {\n"
                    "\"input\": {\n"
                        "\"room_number\": {\n"
                            "\"type\": \"int\"\n"
                        "}\n"
                    "},\n"
                    "\"required\": [\"room_number\"],\n"
                    "\"forms\": [\n"
                        "{\n");
    json +=             F(  "\"href\": \"http://") + ipAddress + F("/change_room\",\n");
    json += F(              "\"contentType\": \"application/x-www-form-urlencoded\",\n"
                            "\"htv:methodName\": \"POST\"\n"
                        "}\n"
                    "]\n"
                "}\n"
            "}"
        "}\n"
    );

    server.send(200, F("application/json"), json);
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