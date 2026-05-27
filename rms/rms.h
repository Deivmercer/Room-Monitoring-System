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

enum State
{
    NORMAL,
    NIGHT_MODE,
    NIGHT_MODE_RESTORE,
    CONFIG_LOW_TEMPERATURE,
    CONFIG_HIGH_TEMPERATURE,
    CONFIG_SIGNAL_STRENGTH,
    CONFIG_ROOM_NUMBER,
    PAUSED
};

struct RomData
{
    float temperature;
    float humidity;
    int32_t timestamp;
    int8_t signalStrength;
};

void updateState();
void toggleReadingFlag();
void readDht();
void readSignalStrength();
void updateDisplay();
void configureLowTemperatureThreshold();
void configureHighTemperatureThreshold();
void configureSignalStrengthThreshold();
void configureRoomNumber();
void printToLCD(String displayLine1, String displayLine2);
void clearLcd();
void writeReadingsToDB(float temperature, float humidity, int singalStrength, time_t timestamp);
State getState();
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
void writeDataToEEPROM(time_t timestamp);
void readEEPROM();
