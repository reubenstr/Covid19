/* Covid-19
* A Covid-19 interactive display for current global case data.
* 
* Reuben Strangelove
* Spring 2020 
*
* MCU: ESP32 (ESP32 DEV KIT 1.0)
* Extra hardware: TFT tft display, generic SD-Card reader, WS2812b led strips
* 
* Covid-19 API: https://documenter.getpostman.com/view/2568274/SzS8rjbe?version=latest#intro
* Wifi credentials stored on SD card.
*
* License: Have fun, do whatever you want.
*/

// SD card is required for functionality, but may be disabled easier development.
#define USE_SD

#include <Arduino.h>
#include <main.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ST7735.h"
#include "FS.h"
#include <SPI.h>
#include <SD.h>
#include <string.h>

#include "PCA9685.h"       // https://github.com/NachtRaveVL/PCA9685-Arduino
#include <ESP32Encoder.h>  // https://github.com/madhephaestus/ESP32Encoder
#include <TM1637Display.h> // https://github.com/avishorp/TM1637
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // https://arduinojson.org/

#include "FastLED.h"

#include <countries.h>
#include <sdFunctions.h>
#include <utilities.h>

#define TIMEOUT_SELECT 5000                    // ms
#define DELAY_DISPLAYS_UPDATE_AFTER_SELECT 500 // ms

// Start date of filling the SD card and the animate mode.
// Determined by the first available record date of the API minus one day.
#define START_DATE_YEAR 2020
#define START_DATE_MONTH 1
#define START_DATE_DAY 21

#define TFT1_CS 13
#define TFT2_CS 12
#define TFT3_CS 14
#define TFT_DC 25
#define TFT_CLK 27
#define TFT_MOSI 26
#define TFT_MISO -1
#define TFT_RST -1
Adafruit_ST7735 tft1 = Adafruit_ST7735(TFT1_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);
Adafruit_ILI9341 tft2 = Adafruit_ILI9341(TFT2_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
Adafruit_ST7735 tft3 = Adafruit_ST7735(TFT3_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

PCA9685 pwmController1;
PCA9685 pwmController2;

#define PIN_ENCODER_A 36
#define PIN_ENCODER_B 39
ESP32Encoder encoder;

#define LED_DISPLAY_1_DIO 15
#define LED_DISPLAY_CLK 16
#define LED_DISPLAY_2_DIO 4
TM1637Display ledDisplay1(LED_DISPLAY_CLK, LED_DISPLAY_1_DIO);
TM1637Display ledDisplay2(LED_DISPLAY_CLK, LED_DISPLAY_2_DIO);

#define PIN_BUTTON_ANIMATE 35
#define PIN_BUTTON_ENCODER 34

#define PIN_LED_CONNECTION 17
#define PIN_LED_ANIMATE 32

#define NUM_PIXELS 48
#define NUM_PIXELS_AS_INDICATORS 36
#define PIN_NEOPIXEL 33
CRGB leds[NUM_PIXELS];

String ssid;
String password;
const char *dataFilePath = "/covid19data.txt";
const char *wifiFilePath = "/wifi.txt";

#define MAX_COUNTRIES 32
Date dateData;
StatsData globalData;
StatsData countryData[MAX_COUNTRIES];

char eventsData[13];
int selectedCountryID = 14; // USA
bool globalMode = false;
ConnectionStatus connectionStatus = NoConnection;

#define NUM_INDICATORS 12
bool indicators[12];
int pixelToIndicatorMap[12][3] = {
    {0, 1, 2},
    {17, 16, 15},
    {18, 19, 20},
    {35, 34, 33},
    {3, 4, 5},
    {14, 13, 12},
    {21, 22, 23},
    {32, 31, 30},
    {6, 7, 8},
    {11, 10, 9},
    {24, 25, 26},
    {29, 28, 27}};

CRGB indicatorColors[12] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Red, CRGB::Green, CRGB::Blue};

void Failure(String message)
{
  Serial.println(message);

  tft1.fillScreen(ST77XX_BLACK);
  tft2.fillScreen(ILI9341_BLACK);
  tft3.fillScreen(ST77XX_BLACK);
  tft2.setCursor(20, 40);
  tft2.setTextSize(3);
  tft2.println(message);

  Serial.println("Program Halted.");

  while (1)
  {
  } // DO NOTHING ELSE
}

void UpdateConnectionLed(ConnectionStatus status)
{
  static unsigned long connectionMillis;
  static int blinkDelay = 100;

  if ((connectionMillis + blinkDelay) < millis())
  {
    connectionMillis = millis();

    if (status == NoConnection)
    {
      digitalWrite(PIN_LED_CONNECTION, LOW);
    }
    else if (status == Connected)
    {
      digitalWrite(PIN_LED_CONNECTION, HIGH);
    }
    else if (status == DownloadingRecords)
    {
      blinkDelay = 250;
      digitalWrite(PIN_LED_CONNECTION, !digitalRead(PIN_LED_CONNECTION));
    }
    else if (status == DownloadedPaused)
    {
      // blinkDelay = 1000;
      // digitalWrite(PIN_LED_CONNECTION, !digitalRead(PIN_LED_CONNECTION));
      digitalWrite(PIN_LED_CONNECTION, HIGH);
    }
  }
}

void UpdateIndicators(char eventsData[])
{
  for (int i = 0; i < NUM_INDICATORS; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      int pixelIndex = pixelToIndicatorMap[i][j];
      if (eventsData[i] == '0')
      {
        leds[pixelIndex] = CRGB::Black;
      }
      else if (eventsData[i] == '1')
      {
        leds[pixelIndex] = indicatorColors[i];
      }
    }
  }

  for (int i = 36; i < 48; i++)
  {
    leds[i] = CRGB::Green;
  }

  FastLED.show();
}

void UpdateLedDisplays(Date date)
{
  ledDisplay1.showNumberDecEx(date.month * 100 + date.day, 0b01000000, true); // Show colon.
  ledDisplay2.showNumberDec(date.year, true);
}

void UpdateTFTDisplays(int countryID, StatsData globalData, StatsData countryData[], bool globalMode)
{
  char buffer[20];
  const int x13Offset = 20;
  const int x2Offset = 30;
  const int tft1OffsetTop = 29;
  const int tft1OffsetBottom = 107;
  const int tft2OffsetTop = 47;
  const int tft2OffsetBottom = 187;
  const int tft3OffsetTop = 27;
  const int tft3OffsetBottom = 105;

  // TFT 1: top data.
  tft1.setCursor(x13Offset, tft1OffsetTop);
  tft1.setTextSize(4);
  if (globalMode)
  {
    FormatNumber(globalData.confirmed, buffer);
  }
  else
  {
    FormatNumber(countryData[countryID].confirmed, buffer);
  }
  tft1.printf("%s%*s", buffer, 8, "");

  //TFT 1: bottom data
  tft1.setCursor(x13Offset, tft1OffsetBottom);
  tft1.setTextSize(2);
  if (globalMode)
  {
    FormatNumber(countryData[countryID].confirmed, buffer);
    tft1.printf("%s: %s%*s", countryCodes[countryID], buffer, 8, "");
  }
  else
  {
    tft1.printf("%*s", 12, "");
  }

  // TFT 2: top data.
  tft2.setCursor(x2Offset, tft2OffsetTop);
  tft2.setTextSize(8);
  if (globalMode)
  {
    FormatNumber(globalData.deaths, buffer);
  }
  else
  {
    FormatNumber(countryData[countryID].deaths, buffer);
  }
  tft2.printf("%s%*s", buffer, 8, "");

  // TFT 2: bottom data.
  tft2.setCursor(x2Offset, tft2OffsetBottom);
  tft2.setTextSize(4);
  if (globalMode)
  {
    FormatNumber(countryData[countryID].deaths, buffer);
    tft2.printf("%s: %s%*s", countryCodes[countryID], buffer, 6, "");
  }
  else
  {
    // Special cases to fit longer text into the window.
    if (strlen(countryNames[countryID]) > 12)
    {
      tft2.setTextSize(3);
    }

    tft2.printf("%s%*s", countryNames[countryID], 12, "");
  }

  // TFT 3: top data.
  tft3.setCursor(x13Offset, tft3OffsetTop);
  tft3.setTextSize(4);
  if (globalMode)
  {
    FormatNumber(globalData.recovered, buffer);
  }
  else
  {
    FormatNumber(countryData[countryID].recovered, buffer);
  }
  tft3.printf("%s%*s", buffer, 8, "");

  //TFT 3: bottom data
  tft3.setCursor(x13Offset, tft3OffsetBottom);
  tft3.setTextSize(2);
  if (globalMode)
  {
    FormatNumber(countryData[countryID].recovered, buffer);
    tft3.printf("%s: %s%*s", countryCodes[countryID], buffer, 6, "");
  }
  else
  {
    tft3.printf("%*s", 12, "");
  }
}

bool GetWifiCredentialsFromSDCard()
{

  File file = SD.open(wifiFilePath);
  if (!file)
  {
    Serial.printf("Failed to open file: %s\n", dataFilePath);
    Serial.printf("Creating file with path: %s\n", dataFilePath);
    writeFile(SD, wifiFilePath, "SSID: \"your ssid inside quotations\"\nPassword: \"your password inside quotations\"");
    return false;
  }

  if (file.find("SSID: \""))
  {
    ssid = file.readStringUntil('"');
    if (file.find("Password: \""))
    {
      password = file.readStringUntil('"');
      return true;
    }
  }

  return false;
}

HttpStatus GetGlobalByDate(Date *date, StatsData *globalData)
{
  char path[200];
  char dateAsText[11];
  HTTPClient http;

  sprintf(dateAsText, "%02i-%02i-%02i", date->year, date->month, date->day);
  sprintf(path, "https://covidapi.info/api/v1/global/%s", dateAsText);

  Serial.print("Connecting to API: ");
  Serial.println(path);

  http.useHTTP10(true);
  http.begin(path);

  int httpCode = http.GET();

  if (httpCode != 200)
  {
    Serial.printf("HTTP error code: %i\n", httpCode);
    http.end();
    return HttpError;
  }

  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  serializeJson(doc, Serial);
  Serial.println();

  if (error)
  {
    Serial.print(F("JSON: deserializeJson() failed: "));
    Serial.println(error.c_str());
    return JsonError;
  }

  globalData->confirmed = doc["result"]["confirmed"];
  globalData->deaths = doc["result"]["deaths"];
  globalData->recovered = doc["result"]["recovered"];

  http.end();
  return Success;
}

HttpStatus GetCountryByDate(int countryId, Date date, StatsData *statsData)
{
  char path[200];
  char dateAsText[11];
  HTTPClient http;

  sprintf(dateAsText, "%02i-%02i-%02i", date.year, date.month, date.day);
  sprintf(path, "https://covidapi.info/api/v1/country/%s/%s", countryCodes[countryId], dateAsText);

  Serial.print("Connecting to API: ");
  Serial.println(path);

  http.useHTTP10(true);
  http.begin(path);

  int httpCode = http.GET();

  if (httpCode != 200)
  {
    Serial.printf("HTTP error code: %i\n", httpCode);
    http.end();
    return HttpError;
  }

  // Serial.println(http.getString());

  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  serializeJson(doc, Serial);
  Serial.println();

  if (error)
  {
    Serial.print(F("JSON: deserializeJson() failed: "));
    Serial.println(error.c_str());
    return JsonError;
  }
  statsData->confirmed = doc["result"][dateAsText]["confirmed"];
  statsData->deaths = doc["result"][dateAsText]["deaths"];
  statsData->recovered = doc["result"][dateAsText]["recovered"];

  Serial.printf("GetCountryByDate: %u %u %u\n", statsData->confirmed, statsData->deaths, statsData->recovered); // TMEP

  http.end();
  return Success;
}

// Get's the most recent record which is assumed to be the last record of the file.
bool GetMostRecentRecord(Date *date, StatsData *globalData, StatsData countryData[], char eventsData[])
{
  char buffer[2048];

  File file = SD.open(dataFilePath);
  if (!file)
  {
    Serial.printf("Failed to open file: %s\n", dataFilePath);
    Serial.printf("Creating file with path: %s\n", dataFilePath);
    writeFile(SD, dataFilePath, "");
  }

  bool recordFoundFlag = false;
  while (file.readBytesUntil('\n', buffer, sizeof(buffer) - 1))
  {
    recordFoundFlag = true;

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, buffer);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }

    int year, month, day;
    const char *dateString = doc["Date"];
    sscanf(dateString, "%u-%u-%u", &year, &month, &day);
    date->year = year;
    date->month = month;
    date->day = day;

    globalData->confirmed = doc["GlobalConfirmed"];
    globalData->deaths = doc["GlobalDeaths"];
    globalData->recovered = doc["GlobalRecovered"];

    const char *eventString = doc["Events"];
    strcpy(eventsData, eventString);

    int i = 0;
    JsonArray arrayCountriesConfirmed = doc["CountriesConfirmed"].as<JsonArray>();
    JsonArray arrayCountriesDeaths = doc["CountriesDeaths"].as<JsonArray>();
    JsonArray arrayCountriesRecovered = doc["CountriesRecovered"].as<JsonArray>();
    for (JsonVariant v : arrayCountriesConfirmed)
    {
      countryData[i].confirmed = v.as<unsigned int>();
      i++;
    }
    i = 0;
    for (JsonVariant v : arrayCountriesDeaths)
    {
      countryData[i].deaths = v.as<unsigned int>();
      i++;
    }
    i = 0;
    for (JsonVariant v : arrayCountriesRecovered)
    {
      countryData[i].recovered = v.as<unsigned int>();
      i++;
    }
  }
  file.close();

  return recordFoundFlag;
}

bool AppendDataToSd(Date date, StatsData globalData, StatsData countryData[], char eventsData[])
{
  // Generate JSON string from collected data.
  DynamicJsonDocument doc(2048);
  char dateAsText[11];
  sprintf(dateAsText, "%02i-%02i-%02i", date.year, date.month, date.day);
  doc["Date"] = dateAsText;
  doc["Events"] = eventsData;
  doc["GlobalConfirmed"] = globalData.confirmed;
  doc["GlobalDeaths"] = globalData.deaths;
  doc["GlobalRecovered"] = globalData.recovered;

  JsonArray confirmedJsonData = doc.createNestedArray("CountriesConfirmed");
  JsonArray deathsJsonData = doc.createNestedArray("CountriesDeaths");
  JsonArray recoveredJsonData = doc.createNestedArray("CountriesRecovered");

  for (int i = 0; i < MAX_COUNTRIES; i++)
  {
    confirmedJsonData.add(countryData[i].confirmed);
    deathsJsonData.add(countryData[i].deaths);
    recoveredJsonData.add(countryData[i].recovered);
  }

  Serial.println("Appending data to SD card:");
  serializeJson(doc, Serial);
  Serial.println();
  File file = SD.open(dataFilePath, FILE_APPEND);
  serializeJson(doc, file);
  file.print("\n");
  file.close();
  // TODO: check for failure.

  return true;
}

// Update SD card data by using the API to get data.
// When an update for a complete record is finished update current working record (that is displayed).
// Non-blocking.
HttpStatus UpdateCardData(Date *date, StatsData *globalData, StatsData countryData[], char eventsData[])
{
  static int countryId = 0;
  static StatsData updateCountryData[MAX_COUNTRIES];
  Date updateDate;
  StatsData updateGlobalData;
  StatsData statsData;

  // Deep copy of date to increment for updating record.
  updateDate.year = date->year;
  updateDate.month = date->month;
  updateDate.day = date->day;
  IncrementDate(updateDate);

  HttpStatus status = GetCountryByDate(countryId, updateDate, &statsData);
  if (status != Success)
  {
    return status;
  }

  updateCountryData[countryId].confirmed = statsData.confirmed;
  updateCountryData[countryId].deaths = statsData.deaths;
  updateCountryData[countryId].recovered = statsData.recovered;

  // Check if all the data is collected for this date.
  if (++countryId == MAX_COUNTRIES)
  {
    // Get global data before appending the record to the SD.
    HttpStatus status = GetGlobalByDate(&updateDate, &updateGlobalData);
    if (status != Success)
    {
      countryId--;
      return status;
    }

    if (!AppendDataToSd(updateDate, updateGlobalData, updateCountryData, eventsData))
    {
      return JsonError;
    }

    countryId = 0;

    // Update current record.
    date->year = updateDate.year;
    date->month = updateDate.month;
    date->day = updateDate.day;
    for (int i = 0; i < MAX_COUNTRIES; i++)
    {
      countryData[i].confirmed = updateCountryData[i].confirmed;
      countryData[i].deaths = updateCountryData[i].deaths;
      countryData[i].recovered = updateCountryData[i].recovered;
    }
    globalData->confirmed = updateGlobalData.confirmed;
    globalData->deaths = updateGlobalData.deaths;
    globalData->recovered = updateGlobalData.recovered;

    return Success;
  }

  return NotReady;
}

// Return true if value has changed.
bool CheckEncoder(int &value)
{
  static int previousValue = 0;

  if (encoder.getCount() == previousValue)
  {
    return false;
  }

  if (encoder.getCount() > MAX_COUNTRIES - 1)
  {
    encoder.setCount(0);
  }

  if (encoder.getCount() < 0)
  {
    encoder.setCount(MAX_COUNTRIES - 1);
  }

  previousValue = encoder.getCount();
  value = encoder.getCount();

  return true;
}

void UpdateCountryIndicator(int countryId, StatsData countryData[], bool globalMode)
{
  uint16_t pwms1[16];
  uint16_t pwms2[16];

  if (globalMode)
  {
    int max = 0;
    int min = INT_MAX;
    for (int i = 0; i < MAX_COUNTRIES; i++)
    {
      if (countryData[i].confirmed > max)
        max = countryData[i].confirmed;
      if (countryData[i].confirmed < min)
        min = countryData[i].confirmed;
    }
    for (int i = 0; i < 16; i++)
    {
      pwms1[i] = countryData[i].confirmed == 0 ? 0 : map(countryData[i].confirmed, min, max, 128, 4095);
      pwms2[i] = countryData[i + MAX_COUNTRIES / 2].confirmed == 0 ? 0 : map(countryData[i + MAX_COUNTRIES / 2].confirmed, min, max, 128, 4095);
    }
  }
  else
  {
    for (int i = 0; i < 16; i++)
    {
      pwms1[i] = 0;
      pwms2[i] = 0;
    }
    if (countryId < 16)
    {
      pwms1[countryId] = 2048;
    }
    else
    {
      pwms2[countryId - 16] = 2048;
    }
  }

  pwmController1.setChannelsPWM(0, 16, pwms1);
  pwmController2.setChannelsPWM(0, 16, pwms2);
}

// Animate is a blocking.
void Animate(int selectedCountryID)
{
  Date dateDataAnimate;
  StatsData globalDataAnimate;
  char eventsDataAnimate[13];
  StatsData countryDataAnimate[MAX_COUNTRIES];
  char buffer[2048];

  File file = SD.open(dataFilePath);
  if (!file)
  {
    Serial.printf("Failed to open file: %s\n", dataFilePath);
    Failure("No SD card.");
  }

  while (file.readBytesUntil('\n', buffer, sizeof(buffer) - 1))
  {

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, buffer);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      Failure("Data corrupt.");
    }

    int year, month, day;
    const char *dateString = doc["Date"];
    sscanf(dateString, "%i-%i-%i", &year, &month, &day);
    dateDataAnimate.year = year;
    dateDataAnimate.month = month;
    dateDataAnimate.day = day;

    globalDataAnimate.confirmed = doc["GlobalConfirmed"];
    globalDataAnimate.deaths = doc["GlobalDeaths"];
    globalDataAnimate.recovered = doc["GlobalRecovered"];

    const char *eventString = doc["Events"];
    strcpy(eventsDataAnimate, eventString);

    int i = 0;
    JsonArray arrayCountriesConfirmed = doc["CountriesConfirmed"].as<JsonArray>();
    JsonArray arrayCountriesDeaths = doc["CountriesDeaths"].as<JsonArray>();
    JsonArray arrayCountriesRecovered = doc["CountriesRecovered"].as<JsonArray>();
    for (JsonVariant v : arrayCountriesConfirmed)
    {
      countryDataAnimate[i].confirmed = v.as<unsigned int>();
      i++;
    }
    i = 0;
    for (JsonVariant v : arrayCountriesDeaths)
    {
      countryDataAnimate[i].deaths = v.as<unsigned int>();
      i++;
    }
    i = 0;
    for (JsonVariant v : arrayCountriesRecovered)
    {
      countryDataAnimate[i].recovered = v.as<unsigned int>();
      i++;
    }

    bool globalModeAnimate = true;
    UpdateTFTDisplays(selectedCountryID, globalDataAnimate, countryDataAnimate, globalModeAnimate);
    UpdateLedDisplays(dateDataAnimate);
    UpdateIndicators(eventsDataAnimate);
    UpdateCountryIndicator(selectedCountryID, countryDataAnimate, globalModeAnimate);

    if (digitalRead(PIN_BUTTON_ANIMATE) == 0)
    {
      return;
    }
  }
}

bool InitSDCard()
{

  int count = 0;
  while (!SD.begin())
  {
    if (++count > 5)
    {
      Serial.println("Card Mount Failed.");
      return false;
    }
    delay(250);
  }

  if (SD.cardType() == CARD_NONE)
  {
    Serial.println("No SD card attached.");
    return false;
  }

  Serial.println("SD card mounted.");
  return true;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Covid-19 Startup.");

  pinMode(PIN_BUTTON_ENCODER, INPUT);
  pinMode(PIN_BUTTON_ANIMATE, INPUT);
  pinMode(PIN_LED_CONNECTION, OUTPUT);
  pinMode(PIN_LED_ANIMATE, OUTPUT);

  FastLED.addLeds<NEOPIXEL, PIN_NEOPIXEL>(leds, NUM_PIXELS);
  FastLED.setBrightness(128);

  ledDisplay1.setBrightness(2);
  ledDisplay2.setBrightness(2);

  Wire.begin();
  pwmController1.resetDevices();
  pwmController1.init(0x01);
  pwmController2.init(0x02);
  pwmController1.setPWMFrequency(1000);
  pwmController2.setPWMFrequency(1000);

  ESP32Encoder::useInternalWeakPullResistors = NONE;
  encoder.attachSingleEdge(PIN_ENCODER_A, PIN_ENCODER_B);
  encoder.clearCount();

  tft1.initR(INITR_GREENTAB);
  tft2.begin();
  tft3.initR(INITR_BLACKTAB);
  tft1.fillScreen(ST77XX_BLACK);
  tft2.fillScreen(ILI9341_BLACK);
  tft3.fillScreen(ST77XX_BLACK);
  tft1.setRotation(3);
  tft2.setRotation(1);
  tft3.setRotation(1);
  tft1.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft2.setTextColor(ILI9341_RED, ILI9341_BLACK);
  tft3.setTextColor(ST77XX_GREEN, ST77XX_BLACK);

// Mount SD card.
// Init file if required.
#if defined(USE_SD)
  if (!InitSDCard())
  {
    Failure("No SD card.");
  }

  if (!GetMostRecentRecord(&dateData, &globalData, countryData, eventsData))
  {
    // File contains no data, create first record.
    // Date based on first record date provided by API.
    dateData.year = START_DATE_YEAR;
    dateData.month = START_DATE_MONTH;
    dateData.day = START_DATE_DAY;
    if (!AppendDataToSd(dateData, globalData, countryData, eventsData))
    {
      Failure("SD card error.");
    }
  }

  Serial.printf("Latest record's date: %i-%i-%i\n", dateData.year, dateData.month, dateData.day);

  // Get WiFi credentials from SD card.
  if (!GetWifiCredentialsFromSDCard())
  {
    Failure("No Wifi Cred.");
  }
  Serial.printf("Wifi SSID: %s\nWifi password: %s\n", ssid.c_str(), password.c_str());
#endif

  UpdateTFTDisplays(selectedCountryID, globalData, countryData, globalMode);

  Serial.println("Entering main loop.");
}

void loop(void)
{
  static unsigned long selectTimoutMillis;
  static unsigned int updateDisplayDelayMillis = 0;
  static unsigned long WifiTimeoutMillis = 0;
  static bool displayConnectionStatus = false;
  static int updateSdCardMillis = 0;
  static int updateSdCardDelay = 100;

  // TODO: Process millis() overflow.

  // Check for Wifi connection
  if (WiFi.status() != WL_CONNECTED)
  {
    displayConnectionStatus = true;
    if ((WifiTimeoutMillis + 1000) < millis())
    {
      connectionStatus = NoConnection;
      WifiTimeoutMillis = millis();
      Serial.println("WiFi disconnected, attempting to connect...");
      WiFi.begin((const char *)ssid.c_str(), (const char *)password.c_str());
    }
  }
  else
  {
    if (displayConnectionStatus)
    {
      displayConnectionStatus = false;
      connectionStatus = Connected;
      Serial.print("WiFi connected to ");
      Serial.println(WiFi.localIP());
    }
  }

  // Check if display needs updated.
  if ((updateDisplayDelayMillis + DELAY_DISPLAYS_UPDATE_AFTER_SELECT) < millis())
  {
    updateDisplayDelayMillis = LONG_MAX - (DELAY_DISPLAYS_UPDATE_AFTER_SELECT * 2); // Prevent constant updates
    UpdateTFTDisplays(selectedCountryID, globalData, countryData, globalMode);
    UpdateLedDisplays(dateData);
    UpdateIndicators(eventsData);
  }

#if defined(USE_SD)
  // Get missing records and store them to the SD card.
  if ((updateSdCardMillis + updateSdCardDelay) < millis())
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      HttpStatus status = UpdateCardData(&dateData, &globalData, countryData, eventsData);
      if (status == NotReady)
      {
        // Do nothing.
        connectionStatus = DownloadingRecords;
      }
      else if (status == Success)
      {
        connectionStatus = DownloadingRecords;
        updateDisplayDelayMillis = millis() - DELAY_DISPLAYS_UPDATE_AFTER_SELECT;
        updateSdCardDelay = 100;
      }
      else if (status == HttpError)
      {
        connectionStatus = DownloadedPaused;
        updateSdCardDelay = 3600000;
        Serial.printf("Sever error detected, %ims until next API Get attempt.\n", updateSdCardDelay);
      }
    }
  }
#endif

  // Check for encoder value updates.
  int encoderValue;
  if (CheckEncoder(encoderValue))
  {
    selectedCountryID = encoderValue;
    selectTimoutMillis = millis();
    updateDisplayDelayMillis = millis();
    globalMode = false;
    UpdateCountryIndicator(selectedCountryID, countryData, globalMode);
  }

  // Check for select mode timeout.
  if ((selectTimoutMillis + TIMEOUT_SELECT) < millis())
  {
    selectTimoutMillis = LONG_MAX - TIMEOUT_SELECT;
    globalMode = true;
    UpdateCountryIndicator(selectedCountryID, countryData, globalMode);
    UpdateTFTDisplays(selectedCountryID, globalData, countryData, globalMode);
  }

  // Check encoder button.
  if (digitalRead(PIN_BUTTON_ENCODER) == 0)
  {
    if (globalMode == false)
    {
      globalMode = true;
      UpdateCountryIndicator(selectedCountryID, countryData, globalMode);
      UpdateTFTDisplays(selectedCountryID, globalData, countryData, globalMode);
    }
  }

  // Process Animate mode.
  static unsigned long animateMillis;
  if (digitalRead(PIN_BUTTON_ANIMATE) == 0)
  {
    if ((animateMillis + 200) < millis()) // Prevent animate from starting over if animate is canceled during animation.
    {
      digitalWrite(PIN_LED_ANIMATE, HIGH);

      if (connectionStatus != NoConnection)
      {
        connectionStatus = Connected;
      }

      Serial.println("Animate started.");

      Animate(selectedCountryID); // BLOCKING

      Serial.println("Animate finished.");

      digitalWrite(PIN_LED_ANIMATE, LOW);
      updateDisplayDelayMillis = millis() - DELAY_DISPLAYS_UPDATE_AFTER_SELECT;
      animateMillis = millis();
    }
  }

  UpdateConnectionLed(connectionStatus);
}