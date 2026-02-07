// 🌦️ ESP32 Weather Station + ThingSpeak
// Date: 03/02/26

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "DFRobot_RainfallSensor.h"

// ===================== WiFi & ThingSpeak =====================
const char* ssid = "AR";
const char* password = "12345678";

String server = "http://api.thingspeak.com/update";
String apiKey = "BDZPSI4MF4S7FKF2";

// ===================== Anemometer =====================
#define ANEMOMETER_PIN 27
volatile int windCount = 0;

unsigned long lastMeasureTime = 0;
const unsigned long measureInterval = 15000;  // ThingSpeak needs ≥15s

void IRAM_ATTR onWindPulse() {
  windCount++;
}

// ===================== Wind Direction =====================
#define WIND_DIR_PIN 34

// ===================== BME680 =====================
Adafruit_BME680 bme;

// ===================== Rainfall Sensor =====================
HardwareSerial RainSerial(2);
DFRobot_RainfallSensor_UART RainSensor(&RainSerial);

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("🌦️ ESP32 Weather Station Starting...\n");

  // -------- WiFi --------
  WiFi.begin(ssid, password);
  Serial.print("📡 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // -------- Anemometer --------
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), onWindPulse, FALLING);

  // -------- Wind Direction --------
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // -------- I2C --------
  Wire.begin(21, 22);

  if (!bme.begin()) {
    Serial.println("❌ BME680 not detected!");
    while (1);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  // -------- Rainfall --------
  RainSerial.begin(9600, SERIAL_8N1, 16, 17);
  delay(2000);

  while (!RainSensor.begin()) {
    Serial.println("❌ Rain sensor error! Retrying...");
    delay(1000);
  }

  Serial.println("🚀 Weather Station Ready!\n");
}

// ===================== Loop =====================
void loop() {

  if (millis() - lastMeasureTime >= measureInterval) {
    lastMeasureTime = millis();

    // -------- Wind Speed --------
    float windSpeed = (windCount * 8.75) / 100.0;
    windCount = 0;

    // -------- Wind Direction --------
    int windADC = analogRead(WIND_DIR_PIN);
    int windDirection = map(windADC, 0, 4095, 0, 360);

    // -------- BME680 --------
    if (!bme.performReading()) {
      Serial.println("❌ BME680 read error");
      return;
    }

    // -------- Rainfall --------
    float totalRain = RainSensor.getRainfall();

    // -------- Print --------
    Serial.println("========== 🌦️ WEATHER DATA ==========");
    Serial.print("Temp: "); Serial.println(bme.temperature);
    Serial.print("Humidity: "); Serial.println(bme.humidity);
    Serial.print("Pressure: "); Serial.println(bme.pressure / 100.0);
    Serial.print("Wind Speed: "); Serial.println(windSpeed);
    Serial.print("Wind Direction: "); Serial.println(windDirection);
    Serial.print("Rain: "); Serial.println(totalRain);
    Serial.println("=====================================");

    // -------- ThingSpeak Upload --------
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      String url = server + "?api_key=" + apiKey +
                   "&field1=" + String(bme.temperature) +
                   "&field2=" + String(bme.humidity) +
                   "&field3=" + String(bme.pressure / 100.0) +
                   "&field4=" + String(windSpeed) +
                   "&field5=" + String(windDirection) +
                   "&field6=" + String(totalRain) +
                   "&field7=" + String(bme.gas_resistance / 1000.0) +
                   "&field8=" + String(bme.readAltitude(1013.25));

      http.begin(url);
      int httpCode = http.GET();
      http.end();

      if (httpCode > 0) {
        Serial.println("✅ Data sent to ThingSpeak");
      } else {
        Serial.println("❌ ThingSpeak error");
      }
    }

    Serial.println();
  }
}
