#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "time.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define LED_PIN 8
#define I2C_SDA 4
#define I2C_SCL 3
#define BUTTON_PIN 1        // Button on GPIO 1

Adafruit_MPU6050 mpu;
bool ledState = false;

const char* ssid       = "yashmitbum";
const char* password   = "yashmitisabum";

const long  gmtOffset_sec = -28800;
const int   daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SECRET_X_MIN  6.0
#define SECRET_X_MAX  8.5
#define SECRET_Y_MAX  2.0
#define SECRET_Z_MAX  10.0

bool isSecretAngle(float ax, float ay, float az) {
  return (ax > SECRET_X_MIN && ax < SECRET_X_MAX &&
          abs(ay) < SECRET_Y_MAX &&
          abs(az) < SECRET_Z_MAX);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Internal pull-up, button connects pin to GND

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }


  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(50);
      digitalWrite(LED_PIN, LOW);  delay(50);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(100);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Connecting to:");
  display.println(ssid);
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  display.println("Connected!");
  display.display();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000);
}

void loop() {
  // --- Button Check ---
  if (digitalRead(BUTTON_PIN) == LOW) {   // LOW = pressed (pull-up wiring)
    Serial.println("Button pressed!");
    delay(200);                            // Basic debounce
  }

  // --- MPU6050 ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  Serial.print("ACCEL [m/s^2]  ");
  Serial.printf("X: %.2f\tY: %.2f\tZ: %.2f\n", ax, ay, az);

  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  if (isSecretAngle(ax, ay, az)) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("* TOP SECRET *");
    display.println("---------------------");
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("   You found the");
    display.println("   hidden message!");
    display.println(" President Prasham :)");
    display.setTextSize(1);
    display.setCursor(20, 48);
    display.print("Go ALL IN");
  } else {
    display.println("SYSTEM STATUS: ONLINE");
    display.println("---------------------");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      display.setTextSize(2);
      display.setCursor(0, 25);
      display.println("Time Sync");
      display.println("Error");
    } else {
      display.setTextSize(2);
      display.setCursor(15, 20);
      display.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      display.setTextSize(1);
      display.setCursor(20, 48);
      display.print(&timeinfo, "%A, %b %d");
    }
  }

  display.display();
  delay(1000);
}