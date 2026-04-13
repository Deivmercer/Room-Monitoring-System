#include "secrets.h"

#define POTENTIOMETER A0
#define DISPLAY_SCL D1
#define DISPLAY_SDA D2
#define DHTPIN D3
#define TEMPERATURE_LED D5
#define BUTTON D6

#define DHTTYPE DHT11

#define DISPLAY_CHARS 16
#define DISPLAY_LINES 2
#define DISPLAY_ADDR 0x27

#define POTENTIOMETER_MIN 1
#define POTENTIOMETER_MAX 1024
#define TEMPERATURE_THRESHOLD_MIN -15.0
#define TEMPERATURE_THRESHOLD_MAX 50.0
#define SIGNAL_STRENGTH_THRESHOLD_MIN 0
#define SIGNAL_STRENGTH_THRESHOLD_MAX 100

void connectToWiFi();
void updateState();
void toggleReadingFlag();
void readDht();
void updateDisplay();
void configureTemperatureThreshold();
void configureSignalStrengthThreshold();

enum State
{
    NORMAL,
    CONFIG_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH
};
