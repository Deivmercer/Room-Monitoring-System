#include "secrets.h"

#define POTENTIOMETER A0
#define DISPLAY_SCL D1
#define DISPLAY_SDA D2
#define DHTPIN D3
#define SIGNAL_STRENGTH_LED D5
#define BUTTON D6
#define HIGH_TEMPERATURE_LED D7
#define LOW_TEMPERATURE_LED D8

#define DHTTYPE DHT11

#define DISPLAY_CHARS 16
#define DISPLAY_LINES 2
#define DISPLAY_ADDR 0x27

#define POTENTIOMETER_MIN 1
#define POTENTIOMETER_MAX 1024
#define TEMPERATURE_THRESHOLD_MIN -15.0
#define TEMPERATURE_THRESHOLD_MAX 50.0
#define SIGNAL_STRENGTH_THRESHOLD_MIN 0
#define SIGNAL_STRENGTH_THRESHOLD_MAX -100
#define DEFAULT_ROOM_NUMBER "1"
#define MAX_ROOMS 9

#define MQTT_BUFFER_SIZE 256
#define MQTT_TOPIC_MAIN "rms/%s"
#define MQTT_TOPIC_DEVICES "rms/devices"
#define MQTT_SUBTOPIC_ACK "/ack"
#define MQTT_SUBTOPIC_TEMPERATURE "/sensors/temperature"
#define MQTT_SUBTOPIC_HUMIDITY "/sensors/humidity"
#define MQTT_SUBTOPIC_SIGNAL "/sensors/signal"
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

enum State
{
    NORMAL,
    CONFIG_LOW_TEMPERATURE,
    CONFIG_HIGH_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH,
    CONFIG_ROOM_NUMBER,
    PAUSED
};

void connectToWiFi();
void updateState();
void toggleReadingFlag();
void readDht();
void readSignalStrength();
void updateDisplay();
void configureLowTemperatureThreshold();
void configureHighTemperatureThreshold();
void configureSignalStrengthThreshold();
void configureRoomNumber();
void updateLEDs();
void printToLCD(String displayLine1, String displayLine2);
void writeReadingsToDB();
void connectToMqttBroker();
void onMqttMessageReceived(String& topic, String& payload);
void publishDeviceRoom();
void mqttUpdateRoom(String topic, String room);
void publishRequestResult(const char* topic, String result);
void publishRequestResult(const char* topic, String result, String error);
void mqttUpdateActive(String status);
void publishDeviceActive();
void mqttUpdateTemperatureMinThreshold(String topic, String threshold);
void mqttUpdateTemperatureMaxThreshold(String topic, String threshold);
void mqttUpdateSignalStrengthThreshold(String topic, String threshold);
void publishThresholds();
State getState();
void setState(State newState);
float getLastTemperature();
float getLastHumidity();
int getLastSignalStrength();
float getLowTemperatureThreshold();
void setLowTemperatureThreshold(float newLowTemperatureThreshold);
float getHighTemperatureThreshold();
void setHighTemperatureThreshold(float newHighTemperatureThreshold);
int getSignalStrengthThreshold();
void setSignalStrengthThreshold(int newSignalStrengthThreshold);
String getRoomNumber();
void setRoomNumber(int newRoomNumber);
void setPaused(bool paused);
