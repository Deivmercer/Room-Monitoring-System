#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include "mqtt_client.h"
#include "rms.h"
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
    mqttClient.loop();
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
    JsonDocument json;
    json["status"] = F("connected");
    json["mac_address"] = macAddress;
    char buffer[MQTT_BUFFER_SIZE];
    size_t length = serializeJson(json, buffer);
    mqttClient.publish(MQTT_TOPIC_DEVICES, buffer, length, false, 2);
    publishDeviceActive();

    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ROOM_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_ACTIVE_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE);
    mqttClient.subscribe(mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE);
}

void onMqttMessageReceived(String& topic, String& payload)
{
    JsonDocument json;
    deserializeJson(json, payload);
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
    else
        Serial.println("Ignored");
}

void mqttPublishData(const char* subTopic, String data)
{
    mqttClient.publish((mqttMainTopic + subTopic).c_str(), data, false, 0);
}

void publishDeviceRoom()
{
    String roomNumber = getRoomNumber();
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_ROOM).c_str(), roomNumber, true, 2);
}

void mqttUpdateRoom(String topic, String room)
{
    int newRoomNumber = room.charAt(0) - 48;
    if (newRoomNumber < 1 || newRoomNumber > MAX_ROOMS)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "error", F("Room ID out of range"));
        return;
    }

    setRoomNumber(newRoomNumber);
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), "ok");
    publishDeviceRoom();
}

void publishRequestResult(const char* topic, String result)
{
    publishRequestResult(topic, result, "");
}

void publishRequestResult(const char* topic, String result, String error)
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
    setPaused(isActive);
    publishDeviceActive();
}

void publishDeviceActive()
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
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("The low temperature threshold must be lower than the high temperature threshold"));
        return;
    }
    if (newLowTemperature < TEMPERATURE_THRESHOLD_MIN || newLowTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Low temperature threshold out of range"));
        return;
    }

    setLowTemperatureThreshold(newLowTemperature);
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void mqttUpdateTemperatureMaxThreshold(String topic, String threshold)
{
    float newHighTemperature = threshold.toFloat();
    if (newHighTemperature < TEMPERATURE_THRESHOLD_MIN || newHighTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("High temperature threshold out of range"));
        return;
    }

    setHighTemperatureThreshold(newHighTemperature);
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void mqttUpdateSignalStrengthThreshold(String topic, String threshold)
{
    int newSignalStrength = threshold.toInt();
    if (newSignalStrength < SIGNAL_STRENGTH_THRESHOLD_MAX || newSignalStrength > SIGNAL_STRENGTH_THRESHOLD_MIN)
    {
        publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("error"), F("Signal strength threshold out of range"));
        return;
    }

    setSignalStrengthThreshold(newSignalStrength);
    publishRequestResult((topic + MQTT_SUBTOPIC_ACK).c_str(), F("ok"), "");
    publishThresholds();
}

void publishThresholds()
{
    String lowTemperatureThreshold = String(getLowTemperatureThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP).c_str(), lowTemperatureThreshold, true, 0);

    String highTemperatureThreshold = String(getHighTemperatureThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP).c_str(), highTemperatureThreshold, true, 0);

    String signalStrengthThreshold = String(getSignalStrengthThreshold());
    mqttClient.publish((mqttMainTopic + MQTT_SUBTOPIC_THRESHOLDS_SIGNAL).c_str(), signalStrengthThreshold, true, 0);
}
