#include <DHT.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WebServer.h>
#include <InfluxDbClient.h>
#include "assignment_1.h"

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

ESP8266WebServer server(80);

InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pointDevice("room_status");

DHT dht = DHT(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

Ticker updateDataTicker;
bool updateData = false;

State state;

bool isActive = true;
String roomNumber = DEFAULT_ROOM_NUMBER;
float lowTemperatureThreshold = -1.0;
float highTemperatureThreshold = 27.0;
int signalStrengthThreshold = -65;
float lastHumidity = 0;
float lastTemperature = 0;
int lastSignalStrength = 0;

void setup()
{
    dht.begin();

    WiFi.mode(WIFI_STA);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);

    // Initializing the LCD display
    Wire.begin();
    Wire.beginTransmission(DISPLAY_ADDR);
    byte error = Wire.endTransmission();
    if (error == 0) {
        Serial.println(F("LCD found."));
        lcd.begin(DISPLAY_CHARS, 2);
        lcd.setBacklight(255);
        lcd.home();
    } else {
        Serial.print(F("LCD not found. Error "));
        Serial.println(error);
    }

    // Initializing ticker
    updateDataTicker.attach(5.0, toggleReadingFlag);

    // Setting pins
    pinMode(SIGNAL_STRENGTH_LED, OUTPUT);
    pinMode(HIGH_TEMPERATURE_LED, OUTPUT);
    pinMode(LOW_TEMPERATURE_LED, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);

    // Turning LED off
    digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
    digitalWrite(HIGH_TEMPERATURE_LED, LOW);
    digitalWrite(LOW_TEMPERATURE_LED, LOW);

    // Init system state
    state = NORMAL;

    pointDevice.addTag("room_number", roomNumber);

    // Configuring server routes
    server.on("/", handleRoot);
    server.on("/set_thresholds", handleSetThresholds);
    server.on("/pause", handlePause);
    server.on("/resume", handleResume);
    server.on("/change_room", handleChangeRoom);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.begin(115200);
}

void loop()
{
    // Will be used to prevent the the system state to keep on changing when the button is being hold pressed
    static bool waitButtonReleased = false;

    // Checking if WiFi is connected
    if (WiFi.status() != WL_CONNECTED)
        connectToWiFi();

    byte buttonState = digitalRead(BUTTON);
    if (buttonState == LOW && !waitButtonReleased)
    {
        updateState();
        waitButtonReleased = true;
    } else if (buttonState == HIGH && waitButtonReleased)
        waitButtonReleased = false;

    switch (state)
    {
        case NORMAL:
            if (!updateData)
                break;
            readDht();
            lastSignalStrength = WiFi.RSSI();
            updateDisplay();
            writeReadingsToDB();
            updateData = false;
            break;
        case CONFIG_LOW_TEMPERATURE:
            configureLowTemperatureThreshold();
            break;
        case CONFIG_HIGH_TEMPERATURE:
            configureHighTemperatureThreshold();
            break;
        case CONFIG_SIGNAL_STRENGTH:
            configureSignalStrengthThreshold();
            break;
        case CONFIG_ROOM_NUMBER:
            configureRoomNumber();
            break;
        case PAUSED:
            lcd.clear();
            printToLCD("Paused", "");
            break;
        default:
            Serial.println(F("ERROR: Unknown state"));
    }

    updateLEDs();

    // Listening for clients on port 80
    server.handleClient();
}

void connectToWiFi()
{
    lcd.clear();
    Serial.print(F("\nConnecting to SSID: "));
    Serial.println(SECRET_SSID);

#ifdef IP
    WiFi.config(ip, dns, gateway, subnet);
#endif

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        switch(WiFi.status())
        {
            case WL_NO_SSID_AVAIL:
                printToLCD("SSID not in", "range. Retrying");
                delay(5000);
                lcd.clear();
                break;
            case WL_CONNECT_FAILED:
                printToLCD("WiFi connection", "failed. Retrying");
                delay(5000);
                lcd.clear();
                break;
            case WL_WRONG_PASSWORD:
                printToLCD("WiFi password", "is wrong.");
                while(true)
                    delay(100);
                break;
            case WL_CONNECTION_LOST:
                printToLCD("WiFi connection", "lost");
                delay(250);
                lcd.clear();
                break;
            default:
                printToLCD("Setup in", "progress...");
                Serial.print(F("."));
                delay(250);
                break;
        }
    }

    Serial.println();
    printToLCD("Connected. IP:", WiFi.localIP().toString());
    delay(5000);
    lcd.clear();
}

void updateState()
{
    switch (state)
    {
        case NORMAL:
            state = CONFIG_LOW_TEMPERATURE;
            digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
            lcd.clear();
            break;
        case CONFIG_LOW_TEMPERATURE:
            state = CONFIG_HIGH_TEMPERATURE;
            break;
        case CONFIG_HIGH_TEMPERATURE:
            if (lowTemperatureThreshold >= highTemperatureThreshold)
            {
                printToLCD("ERROR:low thres", "higher than high");
                delay(2000);
                lcd.clear();
                state = CONFIG_LOW_TEMPERATURE;
            }
            else
            {
                state = CONFIG_SIGNAL_STRENGTH;
                lcd.clear();
            }
            break;
        case CONFIG_SIGNAL_STRENGTH:
            state = CONFIG_ROOM_NUMBER;
            lcd.clear();
            break;
        case CONFIG_ROOM_NUMBER:
            state = NORMAL;
            pointDevice.clearTags();
            pointDevice.addTag("room_number", roomNumber);
            lcd.clear();
            break;
        default:
            Serial.println(F("ERROR: Unknown state"));
    }
}

void toggleReadingFlag()
{
    updateData = true;
}

void readDht()
{
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature))
    {
        Serial.println(F("Failed to read from DHT sensor"));
        return;
    }

    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    lastTemperature = heatIndex;
    lastHumidity = humidity;
}

void updateDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("T:");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", lastTemperature);
    lcd.print(buffer);
    lcd.print(" C  H:");
    lcd.print(int(lastHumidity));
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("WiFi:");
    lcd.print(lastSignalStrength);
    lcd.print(" dBm  #");
    lcd.print(roomNumber);
}

void configureLowTemperatureThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between TEMPERATURE_THRESHOLD_MIN and TEMPERATURE_THRESHOLD_MAX
    lowTemperatureThreshold = linearMapPotentiometerf(potentiometerValue,
                                                      TEMPERATURE_THRESHOLD_MIN,
                                                      TEMPERATURE_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("Low temperature");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", lowTemperatureThreshold);
    lcd.print(buffer);
    lcd.print(" C ");
}

void configureHighTemperatureThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between TEMPERATURE_THRESHOLD_MIN and TEMPERATURE_THRESHOLD_MAX
    highTemperatureThreshold = linearMapPotentiometerf(potentiometerValue,
                                                       TEMPERATURE_THRESHOLD_MIN,
                                                       TEMPERATURE_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("High temperature");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%.1f", highTemperatureThreshold);
    lcd.print(buffer);
    lcd.print(" C ");
}

void configureSignalStrengthThreshold()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between SIGNAL_STRENGTH_THRESHOLD_MIN and SIGNAL_STRENGTH_THRESHOLD_MAX
    signalStrengthThreshold = linearMapPotentiometer(potentiometerValue,
                                                     SIGNAL_STRENGTH_THRESHOLD_MIN,
                                                     SIGNAL_STRENGTH_THRESHOLD_MAX);
    lcd.setCursor(0, 0);
    lcd.print("WiFi signal");
    lcd.setCursor(0, 1);
    lcd.print("thres.: ");
    lcd.print(signalStrengthThreshold);
    lcd.print(" dBm");
}

void configureRoomNumber()
{
    int potentiometerValue = analogRead(POTENTIOMETER);

    // Mapping the potentiometer value between 1 and MAX_ROOMS
    roomNumber = String(linearMapPotentiometer(potentiometerValue, 1, MAX_ROOMS));
    lcd.setCursor(0, 0);
    lcd.print("Room number: ");
    lcd.setCursor(0, 1);
    lcd.print("#");
    lcd.print(roomNumber);
}

void updateLEDs()
{
    if (lastTemperature <= lowTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, HIGH);
    }
    else if (lastTemperature >= highTemperatureThreshold)
    {
        digitalWrite(HIGH_TEMPERATURE_LED, HIGH);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
    }
    else
    {
        digitalWrite(HIGH_TEMPERATURE_LED, LOW);
        digitalWrite(LOW_TEMPERATURE_LED, LOW);
    }

    if (lastSignalStrength < signalStrengthThreshold)
        digitalWrite(SIGNAL_STRENGTH_LED, LOW);
    else
        digitalWrite(SIGNAL_STRENGTH_LED, HIGH);
}

void printToLCD(String displayLine1, String displayLine2)
{
    lcd.setCursor(0, 0);
    lcd.print(displayLine1);
    lcd.setCursor(0, 1);
    lcd.print(displayLine2);
}

int linearMapPotentiometer(int value, int min, int max)
{
    return (int) linearMapPotentiometerf(value, min, max);
}

float linearMapPotentiometerf(int value, float min, float max)
{
    return min + (value - POTENTIOMETER_MIN) * (max - min) / (POTENTIOMETER_MAX - POTENTIOMETER_MIN);
}

void handleRoot()
{
    String html = F(
        "<!DOCTYPE html>\n"
        "<html>\n"
            "<head>\n"
                "<meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
                "<title>Dashboard</title>\n"
                "<style>\n"
                    "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n"
                    "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n"
                    ".button {display: block;width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n"
                    ".button:active {background-color: #158c75;}\n"
                    "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n"
                "</style>\n"
            "</head>\n"
            "<body>\n"
                "<h1>Last readings:</h1>\n");

    if (state == PAUSED)
        html += F("<p>Status: paused</p>\n");
    else
        html += F("<p>Status: active</p>\n");

    html += F(  "<p>Room: #") + roomNumber + F("</p>\n");
    html += F(  "<p>Temperature: ") + String(lastTemperature) + F(" &deg;C</p>\n");
    html += F(  "<p>Humidity: ") + String(lastHumidity) + F("%</p>\n");
    html += F(  "<p>WiFi signal strength: ") + String(lastSignalStrength) + F(" dBm</p>\n"
                "<br>\n");

    if (state == PAUSED)
        html += F("<a class='button' href='/resume'>Resume</a>\n");
    else
        html += F("<a class='button' href='/pause'>Pause</a>\n");

    html += F(  "<br>\n"
                "<h1>Update room ID:</h1>\n"
                "<form action='/change_room' method='post'>\n"
                    "<p>\n"
                        "<label for='room_number'>Room: #</label>\n");
    html += F(          "<input type='number' id='room_number' name='room_number' step='1' min='1' max='") + String(MAX_ROOMS) + F("' value='") + roomNumber + F("'/>\n");
    html += F(      "</p>\n"
                    "<br>\n"
                    "<input class='button' type='submit' value='Update'/>\n"
                "</form>\n"
                "<br>\n"
                "<h1>Configure thresholds:</h1>\n"
                "<form action='/set_thresholds' method='post'>\n"
                    "<p>\n"
                        "<label for='low_temperature'>Low temperature: </label>\n");
    html += F(          "<input type='number' id='low_temperature' name='low_temperature' step='0.01' min='") + String(TEMPERATURE_THRESHOLD_MIN) + F("' max='") + String(TEMPERATURE_THRESHOLD_MAX) + F("' value='") + String(lowTemperatureThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<p>\n"
                        "<label for='high_temperature'>High temperature: </label>\n");
    html += F(          "<input type='number' id='high_temperature' name='high_temperature' step='0.01' min='") + String(TEMPERATURE_THRESHOLD_MIN) + F("' max='") + String(TEMPERATURE_THRESHOLD_MAX) + F("' value='") + String(highTemperatureThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<p>\n"
                        "<label for='signal_strength'>Signal stength: </label>\n");
    html += F(          "<input type='number' id='signal_strength' name='signal_strength' step='1' min='") + String(SIGNAL_STRENGTH_THRESHOLD_MAX) + F("' max='") + String(SIGNAL_STRENGTH_THRESHOLD_MIN) + F("' value='") + String(signalStrengthThreshold) + F("'/>\n");
    html += F(      "</p>\n"
                    "<br>\n"
                    "<input class='button' type='submit' value='Set'/>\n"
                "</form>\n"
            "</body>\n"
        "</html>\n");

    server.send(200, F("text/html"), html);
}

void handleNotFound()
{
    server.send(404, F("text/html"), getErrorPage(F("Page not found")));
}

void handleSetThresholds()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, F("text/html"), getErrorPage(F("Method not allowed")));
        return;
    }
    if (!server.hasArg(F("low_temperature")) || !server.hasArg(F("high_temperature")) || !server.hasArg(F("signal_strength")))
    {
        server.send(400, F("text/html"), getErrorPage(F("Missing arguments")));
        return;
    }

    float newLowTemperature = server.arg(F("low_temperature")).toFloat();
    float newHighTemperature = server.arg(F("high_temperature")).toFloat();
    if (newLowTemperature >= newHighTemperature)
    {
        server.send(400, F("text/html"), getErrorPage(F("The low temperature threshold must be lower than the high temperature threshold")));
        return;
    }
    if (newLowTemperature < TEMPERATURE_THRESHOLD_MIN || newLowTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        server.send(400, F("text/html"), getErrorPage(F("Low temperature threshold out of range")));
        return;
    }
    if (newHighTemperature < TEMPERATURE_THRESHOLD_MIN || newHighTemperature > TEMPERATURE_THRESHOLD_MAX)
    {
        server.send(400, F("text/html"), getErrorPage(F("High temperature threshold out of range")));
        return;
    }

    int newSignalStrength = server.arg(F("signal_strength")).toInt();
    if (newSignalStrength < SIGNAL_STRENGTH_THRESHOLD_MAX || newSignalStrength > SIGNAL_STRENGTH_THRESHOLD_MIN)
    {
        server.send(400, F("text/html"), getErrorPage(F("Signal strength threshold out of range")));
        return;
    }

    lowTemperatureThreshold = newLowTemperature;
    highTemperatureThreshold = newHighTemperature;
    signalStrengthThreshold = newSignalStrength;

    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
}

void handleChangeRoom()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, F("text/html"), getErrorPage(F("Method not allowed")));
        return;
    }
    if (!server.hasArg(F("room_number")))
    {
        server.send(400, F("text/html"), getErrorPage(F("Missing arguments")));
        return;
    }

    float newRoomNumber = server.arg(F("room_number")).toInt();
    if (newRoomNumber < 1 || newRoomNumber > MAX_ROOMS)
    {
        server.send(400, F("text/html"), getErrorPage(F("Room ID out of range")));
        return;
    }

    roomNumber = newRoomNumber;

    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
}

void handlePause()
{
    state = PAUSED;
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
}

void handleResume()
{
    state = NORMAL;
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(301);
}

String getErrorPage(String errorMessage)
{
    String html = F(
        "<!DOCTYPE html>\n"
        "<html>\n"
            "<head>\n"
                "<meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
                "<title>Dashboard</title>\n"
                "<style>\n"
                    "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n"
                    "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n"
                    ".button {display: block;width: 120px;background-color: #1abc9c;border: none;color: white;padding: 13px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n"
                    ".button:active {background-color: #158c75;}\n"
                    "h1 {color: #FF0000}\n"
                "</style>\n"
            "</head>\n"
            "<body>\n"
                "<h1>ERROR</h1>\n");
    html += F(  "<h2>") + errorMessage + F("</h2>\n");
    html += F(  "<br>\n"
                "<a class='button' href='/'>Home</a>\n"
            "</body>\n"
        "</html>\n");
    
    return html;
}

void writeReadingsToDB()
{
    // Checking server connection
    if (!client_idb.validateConnection())
    {
        printToLCD("InfluxDB", "not connected");
        delay(2000);
        return;
    }

    pointDevice.clearFields();
    pointDevice.addField("temperature", lastTemperature);
    pointDevice.addField("humidity", lastHumidity);
    pointDevice.addField("signal_strength", lastSignalStrength);
    if (!client_idb.writePoint(pointDevice))
    {
        printToLCD("Failed to write", "to InfluxDB");
        delay(2000);
        Serial.print(F("InfluxDB write failed: "));
        Serial.println(client_idb.getLastErrorMessage());
    }
}
