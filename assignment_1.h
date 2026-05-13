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
#define MQTT_TOPIC_DEVICES "rms/devices"
#define MQTT_TOPIC_SENSORS "rms/%s/sensors"

void connectToWiFi();
void updateState();
void toggleReadingFlag();
void readDht();
void updateDisplay();
void configureLowTemperatureThreshold();
void configureHighTemperatureThreshold();
void configureSignalStrengthThreshold();
void configureRoomNumber();
void updateLEDs();
void printToLCD(String displayLine1, String displayLine2);
void handleRoot();
void handleNotFound();
void handleSetThresholds();
void handlePause();
void handleResume();
void handleChangeRoom();
void writeReadingsToDB();
String getErrorPage(String errorMessage);
void connectToMqttBroker();

enum State
{
    NORMAL,
    CONFIG_LOW_TEMPERATURE,
    CONFIG_HIGH_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH,
    CONFIG_ROOM_NUMBER,
    PAUSED
};
