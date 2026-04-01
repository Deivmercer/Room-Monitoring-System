#include <DHT.h>
#include <Ticker.h>

#define DHTPIN D1
#define DHTTYPE DHT11

void toggleDhtReadFlag();

DHT dht = DHT(DHTPIN, DHTTYPE);
Ticker dhtTicker;
bool dhtRead = false;

void setup()
{
    dht.begin();
    Serial.begin(115200);
    dhtTicker.attach(2.0, toggleDhtReadFlag);
}

void loop()
{
    if (!dhtRead)
        return;

    readDht();
    dhtRead = false;
}

void toggleDhtReadFlag()
{
    dhtRead = true;
}

void readDht()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println(F("Failed to read from DHT sensor"));
        return;
    }

    float heatIndex = dht.computeHeatIndex(t, h, false);
    Serial.print(F("Perceived temperature: "));   // the temperature perceived by humans (takes into account humidity)
    Serial.print(heatIndex);
    Serial.println(F("°C"));
}
