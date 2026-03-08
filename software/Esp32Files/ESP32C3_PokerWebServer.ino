#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include "time.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define LED_PIN 8
#define I2C_SDA 4
#define I2C_SCL 3
#define BUTTON_PIN 1

Adafruit_MPU6050 mpu;
WebServer server(80);
bool ledState = false;

// --- Globals ---
String lastAction       = "No action yet";
String replyMessage     = "PRESSED";
IPAddress senderIP;
bool hasSender          = false;
volatile bool buttonPressed      = false;
volatile bool buttonResetPressed = false;
unsigned long buttonHoldStart    = 0;
bool buttonHeld                  = false;
unsigned long lastDisplayUpdate = 0;

const char* ssid     = "yashmitbum";
const char* password = "yashmitisabum";

const long  gmtOffset_sec     = -28800;
const int   daylightOffset_sec = 0;
const char* ntpServer          = "pool.ntp.org";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Secret angle: triggered when ay is between 3 and 9 ---
#define SECRET_Y_MIN  3
#define SECRET_Y_MAX  9

bool isSecretAngle(float ax, float ay, float az) {
  return (ay > SECRET_Y_MIN && ay < SECRET_Y_MAX);
}

// --- Web Server Handlers ---
void handleAction() {
  if (server.hasArg("val")) {
    lastAction = server.arg("val");
    senderIP   = server.client().remoteIP();
    hasSender  = true;
    Serial.println("Received Action: " + lastAction);
    Serial.print("From IP: ");
    Serial.println(senderIP);
  }
  server.send(200, "text/plain", "OK");
}

void handleGetButton() {
  if (buttonResetPressed) {
    buttonResetPressed = false;
    server.send(200, "text/plain", "RESET");
    Serial.println("Button state sent: RESET");
  } else if (buttonPressed) {
    buttonPressed = false;
    server.send(200, "text/plain", "PRESSED");
    Serial.println("Button state sent: PRESSED");
  } else {
    server.send(200, "text/plain", "OK");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  display.println("Connected!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);

  server.on("/action",    handleAction);
  server.on("/getButton", handleGetButton);
  server.begin();
  Serial.println("Web server started");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000);
}

void loop() {
  server.handleClient();   // This now runs freely, not blocked by delay

// --- Button Check ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonHeld) {
      buttonHoldStart = millis();   // Start timing when first pressed
      buttonHeld = true;
    }

    // Check if held for 3 seconds
    if (millis() - buttonHoldStart >= 3000) {
      buttonPressed = false;        // Cancel normal press
      buttonHeld = false;           // Reset so it doesn't fire repeatedly
      Serial.println("Button held 3s - sending RESET");
      // Set a special flag so /getButton returns RESET
      buttonResetPressed = true;
      delay(200);
    }
  } else {
    // Button released
    if (buttonHeld) {
      // Only register as normal press if not a long hold
      if (millis() - buttonHoldStart < 3000) {
        buttonPressed = true;
        Serial.println("Button pressed!");
      }
      buttonHeld = false;
    }
  }

  // Only update display once per second, non-blocking
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();

    // --- MPU6050 ---
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;

    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);

    if (isSecretAngle(ax, ay, az)) {
      display.setCursor(0, 0);
      display.println("  *** TOP SECRET ***");
      display.println("---------------------");
      display.setCursor(0, 20);
      display.println(" President Prasham :)");
      display.println("");
      display.println("Action received:");
      display.setTextSize(2);
      display.setCursor(0, 48);
      display.print(lastAction);
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
  }
}