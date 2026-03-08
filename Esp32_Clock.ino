#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "time.h"

// 1. Credentials
const char* ssid       = "PrashyWashy";
const char* password   = "rompot2009";

// 2. Time Configuration (GMT+1 Example)
// For California (PST)
const long  gmtOffset_sec = -28800; 
const int   daylightOffset_sec = 0; // Change to 3600 starting tomorrow!
const char* ntpServer = "pool.ntp.org";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);

  // Initialize Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Initial Screen Message
  display.clearDisplay();
  display.setTextSize(1);      
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Connecting to:");
  display.println(ssid);
  display.display();

  // Connect to Hotspot
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected");
  display.println("Connected!");
  display.display();

  // Initialize Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000); // Give it a second to sync
}

void loop() {
  struct tm timeinfo;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("SYSTEM STATUS: ONLINE");
  display.println("---------------------");

  if(!getLocalTime(&timeinfo)){
    display.setCursor(0, 30);
    display.println("Time Sync Error");
  } else {
    // Large font for the actual time
    display.setTextSize(2); 
    display.setCursor(15, 30);
    display.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Small font for the date
    display.setTextSize(1);
    display.setCursor(20, 55);
    display.print(&timeinfo, "%A, %b %d");
  }

  display.display();
  delay(1000); 
}