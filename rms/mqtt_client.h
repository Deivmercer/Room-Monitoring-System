#ifndef RMS_MQTT_CLIENT_H
#define RMS_MQTT_CLIENT_H

#define MQTT_BUFFER_SIZE 256
#define MQTT_TOPIC_MAIN "rms/%s"
#define MQTT_TOPIC_DEVICES "rms/devices"
#define MQTT_SUBTOPIC_ACK "/ack"
#define MQTT_SUBTOPIC_TEMPERATURE "/sensors/temperature"
#define MQTT_SUBTOPIC_HUMIDITY "/sensors/humidity"
#define MQTT_SUBTOPIC_SIGNAL "/sensors/signal_strength"
#define MQTT_SUBTOPIC_ROOM "/room"
#define MQTT_SUBTOPIC_ROOM_UPDATE "/room/update"
#define MQTT_SUBTOPIC_ACTIVE "/active"
#define MQTT_SUBTOPIC_ACTIVE_UPDATE "/active/update"
#define MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP "/thresholds/min_temperature"
#define MQTT_SUBTOPIC_THRESHOLDS_MIN_TEMP_UPDATE "/thresholds/min_temperature/update"
#define MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP "/thresholds/max_temperature"
#define MQTT_SUBTOPIC_THRESHOLDS_MAX_TEMP_UPDATE "/thresholds/max_temperature/update"
#define MQTT_SUBTOPIC_THRESHOLDS_SIGNAL "/thresholds/signal_strength"
#define MQTT_SUBTOPIC_THRESHOLDS_SIGNAL_UPDATE "/thresholds/signal_strength/update"
#define MQTT_SUBTOPIC_ALARMS_TEMPERATURE "/alarms/temperature"
#define MQTT_SUBTOPIC_ALARMS_SIGNAL "/alarms/signal_strength"
#define MQTT_SUBTOPIC_POWER_SAVING "/power_saving_mode"
#define MQTT_SUBTOPIC_POWER_SAVING_UPDATE "/power_saving_mode/update"
#define MQTT_SUBTOPIC_NIGHT_MODE_CONFIG "/night_mode_config"
#define MQTT_SUBTOPIC_NIGHT_MODE_CONFIG_UPDATE "/night_mode_config/update"

void setupMqtt();
void loopMqttClient();
void connectToMqttBroker();
void onMqttMessageReceived(String& topic, String& payload);
void mqttPublishData(const char* subTopic, String data);
void mqttPublishDeviceRoom();
void mqttUpdateRoom(String topic, String room);
void mqttPublishRequestResult(const char* topic, String result);
void mqttPublishRequestResult(const char* topic, String result, String error);
void mqttUpdateActive(String status);
void mqttPublishDeviceActive();
void mqttUpdateTemperatureMinThreshold(String topic, String threshold);
void mqttUpdateTemperatureMaxThreshold(String topic, String threshold);
void mqttUpdateSignalStrengthThreshold(String topic, String threshold);
void mqttPublishThresholds();
void mqttPublishAlarm(const char* topic, String message);
void mqttFirstSetup();
void mqttUpdatePowerSavingMode(String mode);
void mqttPublishPowerSavingMode();
void mqttUpdateNightModeConfig(String payload);
void mqttPublishNightModeConfig();

#endif // RMS_MQTT_CLIENT_H
