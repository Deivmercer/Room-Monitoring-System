#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include "mqtt_client.h"
#include "rms.h"
#include "wifi_client.h"
#include "secrets.h"

WiFiClient networkClient;
MQTTClient mqttClient(MQTT_BUFFER_SIZE);

String mqttMainTopic;

void setupMqtt()
{
    // Setup MQTT
    mqttClient.begin(MQTT_BROKER_IP, 1883, networkClient);
    mqttClient.onMessage(onMqttMessageReceived);
}

void loopMqttClient()
{
    if (!mqttClient.connected())
        connectToMqttBroker();

    int err = mqttClient.lastError();
    if (err < 0)
    {
        Serial.print("mqtt error: ");
        Serial.println(err);
    }

    mqttClient.loop();
    delay(10); // According to the docs, an additional delay(10) after client.loop() fixes many stability issues with WiFi connections
}

void connectToMqttBroker()
{
    char macAddress[18];
    strcpy(macAddress, getMacAddress().c_str());
    char mainTopic[23];
    snprintf(mainTopic, sizeof(mainTopic), MQTT_TOPIC_MAIN, macAddress);
    mqttMainTopic = mainTopic;

    JsonDocument lwt;
    lwt["status"] = F("disconnected");
    lwt["mac_address"] = macAddress;
    char lwt_buffer[MQTT_BUFFER_SIZE];
    serializeJson(lwt, lwt_buffer);
    mqttClient.setWill(MQTT_TOPIC_DEVICES, lwt_buffer, false, 2);

    Serial.print(F("Connecting to MQTT broker..."));
    while(!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
    {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("\nConnected"));

    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ROOM_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ACTIVE_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_POWER_SAVING_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_NIGHT_MODE_CONFIG_UPDATE);

    JsonDocument json;
    json["status"] = F("connected");
    json["mac_address"] = macAddress;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish(MQTT_TOPIC_DEVICES, buffer, length, false, 2);
    mqttPublishDeviceActive();
    mqttPublishPowerSavingMode();
}

void onMqttMessageReceived(String& topic, String& payload)
{
    Serial.println("Received MQTT message: " + topic + " - " + payload);

    if (topic.endsWith(MQTT_SUBTOPIC_ROOM_UPDATE))
        mqttUpdateRoom(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_ACTIVE_UPDATE))
        mqttUpdateActive(payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE))
        mqttUpdateTemperatureMinThreshold(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE))
        mqttUpdateTemperatureMaxThreshold(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE))
        mqttUpdateSignalStrengthThreshold(topic, payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_POWER_SAVING_UPDATE))
        mqttUpdatePowerSavingMode(payload);
    else if (topic.endsWith(MQTT_SUBTOPIC_NIGHT_MODE_CONFIG_UPDATE))
        mqttUpdateNightModeConfig(payload);
    else
        Serial.println("Ignored");
}

void mqttPublishData(const char* subTopic, String data)
{
    mqttClient.publish((mqttMainTopic + subTopic).c_str(), data, false, 0);
}

void mqttPublishDeviceRoom()
{
    String roomNumber = getRoomNumber();
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_ROOM).c_str(), roomNumber, true, 2);
}

void mqttUpdateRoom(String topic, String room)
{
    int newRoomNumber = room.toInt();
    if (newRoomNumber < 1 || newRoomNumber > MAX_ROOMS)
    {
        mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "error", F("Room ID out of range"));
        return;
    }

    setRoomNumber(newRoomNumber);
    mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "ok");
    mqttPublishDeviceRoom();
}

void mqttPublishRequestResult(const char* topic, String result)
{
    mqttPublishRequestResult(topic, result, "");
}

void mqttPublishRequestResult(const char* topic, String result, String error)
{
    JsonDocument json;
    json["result"] = result;
    if (result == F("error"))
        json["error"] = error;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish(topic, buffer, length, false, 0);
}

void mqttUpdateActive(String active)
{
    bool isActive = active.equals("true");
    setPaused(!isActive);
    mqttPublishDeviceActive();
}

void mqttPublishDeviceActive()
{
    State state = getState();

    String active;
    if (state != PAUSED)
        active = F("true");
    else
        active = F("false");
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_ACTIVE).c_str(), active, true, 2);
}

void mqttUpdateTemperatureMinThreshold(String topic, String threshold)
{
    float newLowTemperature = threshold.toFloat();
    float highTemperatureThreshold = getHighTemperatureThreshold();
    if (newLowTemperature >= highTemperatureThreshold)
    {
        mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("The low temperature threshold must be lower than the high temperature threshold"));
        return;
    }
    if (newLowTemperature < TEMPERATURE_THRESHOLD_MIN || newLowTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Low temperature threshold out of range"));
        return;
    }

    setLowTemperatureThreshold(newLowTemperature);
    mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    mqttPublishThresholds();
}

void mqttUpdateTemperatureMaxThreshold(String topic, String threshold)
{
    float newHighTemperature = threshold.toFloat();
    if (newHighTemperature < TEMPERATURE_THRESHOLD_MIN || newHighTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("High temperature threshold out of range"));
        return;
    }

    setHighTemperatureThreshold(newHighTemperature);
    mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    mqttPublishThresholds();
}

void mqttUpdateSignalStrengthThreshold(String topic, String threshold)
{
    int newSignalStrength = threshold.toInt();
    if (newSignalStrength < SIGNAL_STRENGTH_THRESHOLD_MAX || newSignalStrength > SIGNAL_STRENGTH_THRESHOLD_MIN)
    {
        mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Signal strength threshold out of range"));
        return;
    }

    setSignalStrengthThreshold(newSignalStrength);
    mqttPublishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    mqttPublishThresholds();
}

void mqttPublishThresholds()
{
    String lowTemperatureThreshold = String(getLowTemperatureThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP).c_str(), lowTemperatureThreshold, true, 2);

    String highTemperatureThreshold = String(getHighTemperatureThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP).c_str(), highTemperatureThreshold, true, 2);

    String signalStrengthThreshold = String(getSignalStrengthThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL).c_str(), signalStrengthThreshold, true, 2);
}

void mqttPublishAlarm(const char* topic, String message)
{
    mqttClient.publish((mqttMainTopic + topic).c_str(), message, false, 0);
}

void mqttUpdatePowerSavingMode(String mode)
{
    State newState;
    if (mode == F("normal"))
        newState = NORMAL;
    else if (mode == "away")
        newState = AWAY_MODE;
    else if (mode == "night")
        newState = NIGHT_MODE;
    else
    {
        Serial.print("Unrecognized mode ");
        Serial.println(mode);
    }

    setPowerSavingMode(newState);
}

void mqttPublishPowerSavingMode()
{
    State state = getState();
    String stateString;
    switch (state)
    {
        case NORMAL:
        case WEATHER:
        case NIGHT_MODE_RESTORE:
        case CONFIG_LOW_TEMPERATURE:
        case CONFIG_HIGH_TEMPERATURE:
        case CONFIG_SIGNAL_STRENGTH:
        case CONFIG_ROOM_NUMBER:
        case PAUSED:
            stateString = F("none");
            break;
        case AWAY_MODE:
            stateString = F("away");
            break;
        case NIGHT_MODE:
            stateString = F("night");
            break;
        default:
            Serial.print("Unrecognizeda mode ");
            Serial.println(state);
    }

    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_POWER_SAVING).c_str(), stateString, true, 2);
}

void mqttUpdateNightModeConfig(String payload)
{
    JsonDocument json;
    deserializeJson(json, payload);

    if (!json.containsKey("startHour") || !json.containsKey("startMinute") || !json.containsKey("endHour") || !json.containsKey("endMinute"))
    {
        Serial.println("Missing data");
        return;
    }
    
    int startHour = json["startHour"];
    if (startHour < 0 || startHour > 23)
    {
        Serial.println("Invalid start hour");
        return;
    }
    
    int startMinute = json["startMinute"];
    if (startMinute < 0 || startMinute > 59)
    {
        Serial.println("Invalid start minute");
        return;
    }
    
    int endHour = json["endHour"];
    if (endHour < 0 || endHour > 23)
    {
        Serial.println("Invalid end hour");
        return;
    }
    
    int endMinute = json["endMinute"];
    if (endMinute < 0 || endMinute > 59)
    {
        Serial.println("Invalid end minute");
        return;
    }

    NightModeConfig newConfig = {startHour, startMinute, endHour, endMinute};
    setNightModeConfig(newConfig);
    mqttPublishNightModeConfig();
}

void mqttPublishNightModeConfig()
{
    NightModeConfig nightModeConfig = getNightModeConfig();
    JsonDocument json;
    json["startHour"] = nightModeConfig.startHour;
    json["startMinute"] = nightModeConfig.startMinute;
    json["endHour"] = nightModeConfig.endHour;
    json["endMinute"] = nightModeConfig.endMinute;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_NIGHT_MODE_CONFIG).c_str(), buffer, length, true, 2);
}
