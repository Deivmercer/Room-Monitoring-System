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

#define OWM_URL "api.openweathermap.org"
#define OWM_PORT 80
#define OWM_QUERY "GET /data/2.5/weather?q=%s,%s&units=metric&APPID=%s"
#define WEATHER_DEFAULT_COUNTRY "it" // ISO3166 country code 
#define WEATHER_DEFAULT_CITY "Milano"

#define DEFAULT_SENSOR_TIMER 5.0
#define NIGHT_MODE_TIMER 1800.0

enum State
{
    NORMAL,
    WEATHER,
    AWAY_MODE,
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

struct NightModeConfig
{
    int startHour;
    int startMinute;
    int endHour;
    int endMinute;
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
void showWeatherInfo();
void setPowerSavingMode(State mode);
void setNightModeConfig(NightModeConfig newConfig);
NightModeConfig getNightModeConfig();
