#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
// #include "ntp.h"
#include "secrets.h"


#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <time.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <WiFiClientSecure.h>

// Timezone string for your region, example: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* timezone = "GMT0BST,M3.5.0/1,M10.5.0";  // GMT0BST,M3.5.0/1,M10.5.0 = Europe/London

// Time that the daily task runs in 24 hour format
const int taskHour = 9;
const int taskMinute = 0;

// Store the day when the task last ran to ensure it only runs once per day
int lastRunDay = -1;

unsigned long lastNTPUpdate = 0;
const unsigned long ntpSyncInterval = 30 * 60 * 1000;  // Sync every 1 minutes (in ms)
const unsigned long httpTimeout = 10 * 1000;           // 10 seconds

WiFiClientSecure client;
// WiFiClient client;  // or WiFiClientSecure for HTTPS
// WiFiClientSecure client;
HTTPClient http;






const int ledPin = 22;

// Set up WeActStudio 2.13" Epaper module
GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
  GxEPD2_213_Z98c(/*CS=*/5, /*DC=*/0, /*RST=*/2, /*BUSY=*/15));

// Set up U8G2 fonts
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

const uint8_t* mainWordFont = u8g2_font_ncenB12_te;
const uint8_t* subtitleFont = u8g2_font_helvB08_tf;
const uint8_t* definitionFont = u8g2_font_helvR08_tf;

const int leftMargin = 5;


struct Entry {
  String type;
  String gender;
  String definitions[10];
};

struct WordStruct {
  String name;
  Entry entries[10];
};


void syncTime() {
  Serial.print("Synchronizing time with NTP server...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC offset set to 0
  time_t now = time(nullptr);
  while (now < 24 * 3600) {  // Wait until time is valid
    delay(100);
    now = time(nullptr);
  }
  Serial.println(" Time synchronized!");

  // Set timezone
  setenv("TZ", timezone, 1);
  tzset();

  lastNTPUpdate = millis();  // Record the time of the last sync
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Sync NTP time
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  syncTime();

  display.init(115200, true, 2, false);  // Initialize display
  display.setRotation(3);                // Flipped landscape
  display.setFullWindow();
  display.firstPage();

  u8g2_for_adafruit_gfx.begin(display);  // connect u8g2 procedures to Adafruit GFX

  JsonDocument doc = contactProxyAPI();
  WordStruct word = parseWordFromDoc(doc);
  Serial.println(word.name);

  drawDisplay(timeinfo, word);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
}

String getMotDuJourString(const tm& timeinfo) {
  const char* mois[] = {
    "janv", "févr", "mars", "avri", "mai", "juin",
    "juil", "août", "sept", "octo", "nove", "déc."
  };

  const char* moisStr = "inco";
  if (timeinfo.tm_mon >= 0 && timeinfo.tm_mon <= 11) {
    moisStr = mois[timeinfo.tm_mon];
  }

  char buff[40];
  snprintf(buff, sizeof(buff), "mot du jour - %02d %s à %02d:%02d",
           timeinfo.tm_mday, moisStr, timeinfo.tm_hour, timeinfo.tm_min);

  return String(buff);
}

void drawDisplay(struct tm timeinfo, struct WordStruct word) {
  Serial.println("#########\nDrawing display...\n#########");

  do {
    display.fillScreen(GxEPD_WHITE);

    u8g2_for_adafruit_gfx.setFontMode(1);       // use u8g2 transparent mode (this is default)
    u8g2_for_adafruit_gfx.setFontDirection(0);  // left to right (this is default)
    // u8g2_for_adafruit_gfx.setFont(mainWordFont);
    u8g2_for_adafruit_gfx.setBackgroundColor(GxEPD_WHITE);

    // Title
    u8g2_for_adafruit_gfx.setCursor(90, 15);
    String mdjString = getMotDuJourString(timeinfo);
    Serial.println(mdjString);
    /// mot du jour
    u8g2_for_adafruit_gfx.setFont(subtitleFont);
    u8g2_for_adafruit_gfx.setForegroundColor(GxEPD_RED);
    u8g2_for_adafruit_gfx.print(mdjString);
    u8g2_for_adafruit_gfx.print(F(" "));
    /// Icon
    u8g2_for_adafruit_gfx.setFont(u8g2_font_iconquadpix_m_all);
    u8g2_for_adafruit_gfx.setForegroundColor(GxEPD_BLACK);
    u8g2_for_adafruit_gfx.print(F("l"));  // Double arrow circle icon (https://github.com/olikraus/u8g2/wiki/fntgrpbitfontmaker2#iconquadpix)

    // Word - French
    u8g2_for_adafruit_gfx.setCursor(leftMargin, 40);
    u8g2_for_adafruit_gfx.setFont(mainWordFont);
    u8g2_for_adafruit_gfx.setForegroundColor(GxEPD_BLACK);
    u8g2_for_adafruit_gfx.print(word.name);
    u8g2_for_adafruit_gfx.print("  ");

    int verticalCursor = 60;
    for (Entry e : word.entries) {
      /// Type of word
      int16_t horizontal_cursor = u8g2_for_adafruit_gfx.tx;
      u8g2_for_adafruit_gfx.setFont(subtitleFont);
      u8g2_for_adafruit_gfx.setForegroundColor(GxEPD_RED);
      u8g2_for_adafruit_gfx.print(e.type);
      u8g2_for_adafruit_gfx.print(" ");
      u8g2_for_adafruit_gfx.print(e.gender);

      int definitionCount = 0;
      for (String d : e.definitions) {
        if (d != "") {
          // Definition - English
          u8g2_for_adafruit_gfx.setFont(definitionFont);
          u8g2_for_adafruit_gfx.setForegroundColor(GxEPD_BLACK);
          u8g2_for_adafruit_gfx.setCursor(leftMargin, verticalCursor);
          // u8g2_for_adafruit_gfx.print(F(d));
          u8g2_for_adafruit_gfx.print(definitionCount + 1);
          u8g2_for_adafruit_gfx.print(". ");
          u8g2_for_adafruit_gfx.print(d);

          verticalCursor += 15;
          definitionCount++;
        }
      }
      verticalCursor += 15;
    }
  } while (display.nextPage());
}


JsonDocument contactProxyAPI() {
  const char* url = "https://mot-du-jour-api.onrender.com/mot_spontane";

  JsonDocument doc;

  // Ask HTTPClient to collect the Transfer-Encoding header
  // (by default, it discards all headers)
  const char* keys[] = { "Transfer-Encoding" };
  http.collectHeaders(keys, 1);

  // Send request
  client.setInsecure();
  http.setTimeout(httpTimeout);
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32Client");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET request to %s failed with status code %d\n", url, httpCode);
    http.end();

    return doc;
  }

  // Get the raw and the decoded stream
  Stream& rawStream = http.getStream();
  ChunkDecodingStream decodedStream(http.getStream());

  // Choose the right stream depending on the Transfer-Encoding header
  Stream& response = http.header("Transfer-Encoding") == "chunked" ? decodedStream : rawStream;

  // Parse response
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.print("Deserialization failed: ");
    Serial.println(err.c_str());
    http.end();

    return doc;
  }

  // Disconnect
  http.end();

  return doc;
}

WordStruct parseWordFromDoc(JsonDocument doc) {
  const char* name = doc["Name"];
  Serial.printf("Name: %s\n", name);

  // Read values
  WordStruct w = {};
  w.name = name;

  JsonArray entries = doc["Entries"];
  int entriesCount = 0;
  for (JsonVariant entry : entries) {
    Entry e = {};

    const char* wordType = entry["Type"];
    e.type = wordType;

    const char* gender = entry["Gender"];
    e.gender = gender;

    JsonArray definitions = entry["Definitions"];
    int definitionsCount = 0;
    for (JsonVariant definition : definitions) {
      String parsedDef = definition.as<String>();

      e.definitions[definitionsCount] = parsedDef;

      definitionsCount++;
    }

    w.entries[entriesCount] = e;

    entriesCount++;
  }

  return w;
}


void loop() {
  delay(1000);  // Run loop every second

  digitalWrite(ledPin, HIGH);

  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Resynchronize with NTP at every interval
  if (millis() - lastNTPUpdate > ntpSyncInterval) {
    syncTime();
  }


  if (timeinfo.tm_sec == 0) {
    // TODO: remove this, it's only for testing to trigger the task more often

    JsonDocument doc = contactProxyAPI();
    WordStruct word = parseWordFromDoc(doc);
    Serial.println(word.name);

    drawDisplay(timeinfo, word);
  }

  // Current time and date
  Serial.printf("Current time: %02d:%02d:%02d, Date: %04d-%02d-%02d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

  if (timeinfo.tm_hour == taskHour && timeinfo.tm_min == taskMinute && lastRunDay != timeinfo.tm_mday) {
    JsonDocument doc = contactProxyAPI();
    WordStruct word = parseWordFromDoc(doc);
    Serial.println(word.name);

    drawDisplay(timeinfo, word);
    // Set the day to ensure it only runs once per day
    lastRunDay = timeinfo.tm_mday;
  }
}
