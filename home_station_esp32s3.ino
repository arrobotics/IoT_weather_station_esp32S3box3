#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define LGFX_ESP32_S3_BOX_V3
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

static LGFX lcd;

// ================= WiFi =================
const char* ssid = "AR";
const char* password = "12345678";


// ================= ThingSpeak =================
String channelID = "3248919";
String readAPIKey = "5KX0C4F7HRSMDR4B";

// ================= SCREEN =================
#define SCREEN_W 320
#define SCREEN_H 240

// ================= COLORS =================
#define BG_COLOR      TFT_BLACK
#define HEADER_COLOR  TFT_DARKCYAN
#define TEXT_COLOR    TFT_WHITE

#define CARD_TEMP     TFT_RED
#define CARD_HUM      TFT_BLUE
#define CARD_WIND     TFT_DARKCYAN
#define CARD_PRESS    TFT_MAGENTA
#define CARD_DIR      TFT_PURPLE
#define CARD_RAIN     TFT_NAVY
#define CARD_GAS      TFT_DARKGREEN
#define CARD_ALT      TFT_BROWN

// ================= DATA =================
float temperature = 0;
float humidity = 0;
float pressure = 0;
float windSpeed = 0;
float rain = 0;
float gas = 0;
float altitude = 0;
int windDir = 0;

// ================= TIMER =================
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 15000;

// ================= PAGE CONTROL =================
int currentPage = 0;   // 0 = Page1, 1 = Page2
int touchStartX = -1;
int touchEndX = -1;

// ================= HELPERS =================
const char* windDirText(int deg) {
  if (deg < 22) return "N";
  if (deg < 67) return "NE";
  if (deg < 112) return "E";
  if (deg < 157) return "SE";
  if (deg < 202) return "S";
  if (deg < 247) return "SW";
  if (deg < 292) return "W";
  if (deg < 337) return "NW";
  return "N";
}

void drawCard(int x, int y, int w, int h,
              uint16_t bgColor,
              const char* title,
              const String& value,
              const char* unit) {

  lcd.fillRoundRect(x, y, w, h, 10, bgColor);
  lcd.drawRoundRect(x, y, w, h, 10, TFT_WHITE);

  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(TEXT_COLOR);
  lcd.setTextDatum(top_left);
  lcd.drawString(title, x + 8, y + 6);

  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextDatum(middle_left);
  lcd.drawString(value, x + 8, y + h / 2 + 8);

  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(bottom_right);
  lcd.drawString(unit, x + w - 8, y + h - 6);
}

// ================= UI PAGES =================
void drawPage1() {
  drawCard(10, 50, 145, 70, CARD_TEMP,
           "Temperature", String(temperature,1), "C");

  drawCard(165, 50, 145, 70, CARD_HUM,
           "Humidity", String(humidity,1), "%");

  drawCard(10, 130, 145, 70, CARD_WIND,
           "Wind Speed", String(windSpeed,1), "m/s");

  drawCard(165, 130, 145, 70, CARD_PRESS,
           "Pressure", String(pressure,0), "hPa");
}

void drawPage2() {
  drawCard(10, 50, 145, 70, CARD_DIR,
           "Wind Dir",
           String(windDir) + " " + windDirText(windDir),
           "");

  drawCard(165, 50, 145, 70, CARD_RAIN,
           "Rain", String(rain,1), "mm");

  drawCard(10, 130, 145, 70, CARD_GAS,
           "Gas", String(gas,1), "ppm");

  drawCard(165, 130, 145, 70, CARD_ALT,
           "Altitude", String(altitude,1), "m");
}

// ================= DRAW UI =================
void drawUI() {
  lcd.fillScreen(BG_COLOR);

  // Header
  lcd.fillRect(0, 0, SCREEN_W, 40, HEADER_COLOR);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextColor(TEXT_COLOR);
  lcd.setTextDatum(middle_center);
  lcd.drawString("WEATHER STATION", SCREEN_W / 2, 22);

  // Page dots
  lcd.fillCircle(150, 235, 4, currentPage == 0 ? TFT_WHITE : TFT_DARKGREY);
  lcd.fillCircle(170, 235, 4, currentPage == 1 ? TFT_WHITE : TFT_DARKGREY);

  if (currentPage == 0) drawPage1();
  else drawPage2();
}

// ================= TOUCH SWIPE =================
void handleSwipe() {
  uint16_t x, y;

  if (lcd.getTouch(&x, &y)) {
    if (touchStartX == -1) touchStartX = x;
    touchEndX = x;
  } else {
    if (touchStartX != -1 && touchEndX != -1) {
      int diff = touchEndX - touchStartX;

      if (diff > 80) currentPage = 0;
      else if (diff < -80) currentPage = 1;

      drawUI();
    }
    touchStartX = -1;
    touchEndX = -1;
  }
}

// ================= THINGSPEAK =================
void readThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://api.thingspeak.com/channels/";
  url += channelID;
  url += "/feeds/last.json?api_key=";
  url += readAPIKey;

  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, http.getString());

    temperature = doc["field1"].as<float>();
    humidity    = doc["field2"].as<float>();
    pressure    = doc["field3"].as<float>();
    windSpeed   = doc["field4"].as<float>();
    windDir     = doc["field5"].as<int>();
    rain        = doc["field6"].as<float>();
    gas         = doc["field7"].as<float>();
    altitude    = doc["field8"].as<float>();

    drawUI();
  }
  http.end();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);

  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextDatum(middle_center);
  lcd.drawString("Connecting WiFi...", SCREEN_W/2, SCREEN_H/2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  drawUI();
  readThingSpeak();
}

// ================= LOOP =================
void loop() {
  handleSwipe();

  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();
    readThingSpeak();
  }
}
