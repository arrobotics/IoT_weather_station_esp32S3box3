#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// LovyanGFX Setup
#define LGFX_ESP32_S3_BOX_V3
#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

static LGFX lcd;

// wifi
const char* ssid = "AR";
const char* password = "12345678";

// ThingSpeak API
const String ts_channel_id = "3248919";
const String ts_read_api_key = "5KX0C4F7HRSMDR4B";

// OpenWeatherMap API
const String owm_api_key = "baeb9924257c681ed2541da71c96fc60";
const String city = "Delhi,IN";

unsigned long lastTime = 0;
unsigned long timerDelay = 30000; // 30 seconds delay

// UI colour
#define BG_COLOR        lcd.color565(15, 15, 20)
#define HEADER_COLOR    lcd.color565(0, 100, 200)

// 8 box colour for Page 1
#define BOX_TEMP_COLOR  lcd.color565(200, 60, 60)   // red
#define BOX_HUM_COLOR   lcd.color565(40, 140, 200)  // blue
#define BOX_PRES_COLOR  lcd.color565(140, 80, 200)  // purple
#define BOX_WIND_COLOR  lcd.color565(220, 120, 40)  // orange
#define BOX_DIR_COLOR   lcd.color565(80, 160, 120)  // green
#define BOX_RAIN_COLOR  lcd.color565(60, 100, 160)  // dark yellow
#define BOX_GAS_COLOR   lcd.color565(180, 180, 50)  // yellow
#define BOX_ALT_COLOR   lcd.color565(100, 100, 100) // grey

// Forecast Box Color (Page 2)
#define FC_CARD_COLOR   lcd.color565(40, 50, 70) 
#define FC_TOP_COLOR    lcd.color565(30, 40, 60)

// page
int currentPage = 1; // 1 = Live Data, 2 = Forecast Data
int touchX, touchY;
bool isTouched = false;

// data store and update after touch
String t_temp = "--", t_hum = "--", t_pres = "--", t_wind = "--";
String t_dir = "--", t_rain = "--", t_gas = "--", t_alt = "--";

// Forecast variables
float fc_temp = 0.0, fc_feels_like = 0.0, fc_hum = 0.0, fc_pressure = 0.0, fc_dew_point = 0.0;
float fc_wind_speed = 0.0, fc_visibility_km = 0.0;
String fc_wind_dir = "--";
String fc_desc = "Fetching...";

// ===================== HELPERS =====================
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

void setup() {
  Serial.begin(115200);

  // display int
  lcd.init();
  lcd.setRotation(1); // landscape mode

  if (lcd.touch() != nullptr) {
    lcd.touch()->init();
  }

  // boot screen
  lcd.fillScreen(BG_COLOR);
  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(2);
  lcd.setCursor(60, 110);
  lcd.println("Connecting WiFi...");

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  lcd.fillScreen(BG_COLOR);
  lcd.setTextColor(TFT_GREEN);
  lcd.setCursor(75, 110);
  lcd.println("WiFi Connected!");
  delay(1000);

  // first fetch
  fetchThingSpeakData();
  fetchOpenWeatherData();
  drawCurrentPage();
}

void loop() {
  // check input after touch
  if (lcd.getTouch(&touchX, &touchY)) {
    if (!isTouched) {
      isTouched = true;
      // change page
      currentPage = (currentPage == 1) ? 2 : 1;
      drawCurrentPage();
    }
  } else {
    isTouched = false;
  }

  // data update after 30 sec
  if ((millis() - lastTime) > timerDelay || lastTime == 0) {
    if(WiFi.status() == WL_CONNECTED){
      fetchThingSpeakData();
      fetchOpenWeatherData();
      drawCurrentPage(); // refresh new data
    }
    lastTime = millis();
  }
}

// Draw the current active page
void drawCurrentPage() {
  lcd.fillScreen(BG_COLOR);

  if (currentPage == 1) {
    // --- PAGE 1: LIVE DATA ---
    lcd.fillRect(0, 0, 320, 35, HEADER_COLOR);
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.setCursor(10, 10);
    lcd.print("Live Sensor Data");

    String windDirDisplay = t_dir;
    if (t_dir != "--" && t_dir != "null" && t_dir != "") {
      int deg = t_dir.toInt();
      windDirDisplay = t_dir + " " + String(windDirText(deg));
    }

    int startY = 45;  
    int boxW = 145;  
    int boxH = 40;  
    int col1 = 10;  
    int col2 = 165;  
    int gap = 48; 

    drawBox(col1, startY,           boxW, boxH, BOX_TEMP_COLOR, "Temp (C)", t_temp);  
    drawBox(col1, startY + gap,     boxW, boxH, BOX_HUM_COLOR,  "Humidity (%)", t_hum);  
    drawBox(col1, startY + gap * 2, boxW, boxH, BOX_PRES_COLOR, "Pres (hPa)", t_pres);  
    drawBox(col1, startY + gap * 3, boxW, boxH, BOX_WIND_COLOR, "Wind (m/s)", t_wind);  

    drawBox(col2, startY,           boxW, boxH, BOX_DIR_COLOR,  "Wind Dir", windDirDisplay);  
    drawBox(col2, startY + gap,     boxW, boxH, BOX_RAIN_COLOR, "Rain (mm)", t_rain);  
    drawBox(col2, startY + gap * 2, boxW, boxH, BOX_GAS_COLOR,  "Gas", t_gas);  
    drawBox(col2, startY + gap * 3, boxW, boxH, BOX_ALT_COLOR,  "Altitude(m)", t_alt);

  } else {
    // --- PAGE 2: FORECAST DATA ---
    lcd.fillRect(0, 0, 320, 35, HEADER_COLOR);
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.setCursor(10, 10);
    lcd.print("Forecast Data");

    // Top Weather Info Card (Temp, Desc, Feels Like)
    lcd.fillRoundRect(10, 45, 300, 60, 8, FC_TOP_COLOR);  
    
    // Main Temp
    lcd.setTextSize(3);  
    lcd.setTextColor(TFT_WHITE);  
    lcd.setCursor(20, 55);  
    lcd.print(fc_temp, 0);  
    lcd.print(" C");  

    // Description
    lcd.setTextSize(1);  
    lcd.setTextColor(TFT_WHITE);  
    lcd.setCursor(150, 55);  
    lcd.print(fc_desc);

    // Feels Like
    lcd.setTextColor(TFT_LIGHTGREY);
    lcd.setCursor(150, 75);
    lcd.print("Feels like ");
    lcd.print(fc_feels_like, 0);
    lcd.print(" C");

    // 6-Grid Layout for extra data
    int startY = 115;
    int gapY = 55;
    int col1 = 10, col2 = 113, col3 = 216;
    int boxW = 94, boxH = 50;

    String windStr = String(fc_wind_speed, 1) + "m/s " + fc_wind_dir;
    String humStr = String(fc_hum, 0) + "%";
    String visStr = String(fc_visibility_km, 1) + "km";
    String presStr = String(fc_pressure, 0) + "hPa";
    String uvStr = "-- UV"; 
    String dewStr = String(fc_dew_point, 1) + " C";

    // Row 1
    drawForecastBox(col1, startY, boxW, boxH, FC_CARD_COLOR, "Wind", windStr);
    drawForecastBox(col2, startY, boxW, boxH, FC_CARD_COLOR, "Humidity", humStr);
    drawForecastBox(col3, startY, boxW, boxH, FC_CARD_COLOR, "Visibility", visStr);
    
    // Row 2
    drawForecastBox(col1, startY + gapY, boxW, boxH, FC_CARD_COLOR, "Pressure", presStr);
    drawForecastBox(col2, startY + gapY, boxW, boxH, FC_CARD_COLOR, "UV Index", uvStr);
    drawForecastBox(col3, startY + gapY, boxW, boxH, FC_CARD_COLOR, "Dew Point", dewStr);
  }
}

// Helper to draw Page 1 Boxes
void drawBox(int x, int y, int w, int h, uint16_t color, String label, String value) {
  lcd.fillRoundRect(x, y, w, h, 6, color);
  lcd.setTextColor(TFT_LIGHTGREY);
  lcd.setTextSize(1);
  lcd.setCursor(x + 5, y + 5);
  lcd.print(label);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(x + 5, y + 18);
  if (value == "null" || value == "") {
    lcd.print("--");
  } else {
    lcd.print(value);
  }
}

// Helper to draw Page 2 Grid Boxes
void drawForecastBox(int x, int y, int w, int h, uint16_t color, String label, String value) {
  lcd.fillRoundRect(x, y, w, h, 6, color);
  
  lcd.setTextColor(TFT_LIGHTGREY);
  lcd.setTextSize(1);
  int labelWidth = label.length() * 6; 
  lcd.setCursor(x + (w - labelWidth) / 2, y + 8);
  lcd.print(label);
  
  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(1);
  int valWidth = value.length() * 6;
  lcd.setCursor(x + (w - valWidth) / 2, y + 30);
  lcd.print(value);
}

// ThingSpeak Data Fetch
void fetchThingSpeakData() {
  HTTPClient http;
  String ts_url = "http://api.thingspeak.com/channels/" + ts_channel_id + "/feeds/last.json?api_key=" + ts_read_api_key;
  http.begin(ts_url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      t_temp = doc["field1"].as<String>();
      t_hum  = doc["field2"].as<String>();
      t_pres = doc["field3"].as<String>();
      t_wind = doc["field4"].as<String>();
      t_dir  = doc["field5"].as<String>();
      t_rain = doc["field6"].as<String>();
      t_gas  = doc["field7"].as<String>();
      t_alt  = doc["field8"].as<String>();
      
      // Print ThingSpeak data to Serial Monitor
      Serial.println("\n--- ThingSpeak Live Data ---");
      Serial.println("Temp: " + t_temp + " C");
      Serial.println("Humidity: " + t_hum + " %");
      Serial.println("Pressure: " + t_pres + " hPa");
      Serial.println("Wind Speed: " + t_wind + " m/s");
      Serial.println("Wind Dir: " + t_dir + " deg");
      Serial.println("Rain: " + t_rain + " mm");
      Serial.println("Gas: " + t_gas);
      Serial.println("Altitude: " + t_alt + " m");
      Serial.println("----------------------------");
    }
  } else {
    Serial.println("Error fetching ThingSpeak data. HTTP Code: " + String(httpResponseCode));
  }
  http.end();
}

// OpenWeatherMap Data Fetch
void fetchOpenWeatherData() {
  HTTPClient http;
  String owm_url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + owm_api_key + "&units=metric";
  http.begin(owm_url);
  int httpResponseCode = http.GET();

  // ----------------------------------------------------
  // DEBUGGING SECTION ADDED HERE
  // ----------------------------------------------------
  Serial.println("\n--- OpenWeatherMap Debug Info ---");
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("Raw Payload from API:");
    Serial.println(payload); 
    Serial.println("---------------------------------");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      fc_temp = doc["main"]["temp"];
      fc_feels_like = doc["main"]["feels_like"];
      fc_hum = doc["main"]["humidity"];
      fc_pressure = doc["main"]["pressure"];
      
      fc_visibility_km = doc["visibility"].as<int>() / 1000.0;
      
      fc_wind_speed = doc["wind"]["speed"];
      int wdeg = doc["wind"]["deg"];
      fc_wind_dir = String(windDirText(wdeg));

      fc_desc = doc["weather"][0]["description"].as<String>();
      if (fc_desc.length() > 0) {  
        fc_desc[0] = toupper(fc_desc[0]);  
      }  

      fc_dew_point = fc_temp - ((100.0 - fc_hum) / 5.0);

      // Print OpenWeatherMap data to Serial Monitor
      Serial.println("\n--- OpenWeatherMap Forecast Data ---");
      Serial.println("Temp: " + String(fc_temp) + " C");
      Serial.println("Feels Like: " + String(fc_feels_like) + " C");
      Serial.println("Desc: " + fc_desc);
      Serial.println("Humidity: " + String(fc_hum) + " %");
      Serial.println("Pressure: " + String(fc_pressure) + " hPa");
      Serial.println("Visibility: " + String(fc_visibility_km) + " km");
      Serial.println("Wind: " + String(fc_wind_speed) + " m/s " + fc_wind_dir);
      Serial.println("Dew Point: " + String(fc_dew_point) + " C");
      Serial.println("------------------------------------");
    } else {
      Serial.print("JSON Parsing Failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("Error fetching OpenWeather data. Connection Failed.");
  }
  http.end();
}
