#include "secrets.h"

#define POTENTIOMETER A0
#define DHTPIN D1
#define TEMPERATURE_LED D2
#define BUTTON D3

#define DHTTYPE DHT11

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
void printWiFiSignalStrength();

enum State
{
    NORMAL,
    CONFIG_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH
};
