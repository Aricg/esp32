#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

// WiFi connection details from build flags
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID" // Fallback if not defined
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" // Fallback if not defined
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP8266 WiFi Connectivity Test");
  
  // Check for default credentials
  if (strcmp(ssid, "YOUR_WIFI_SSID") == 0 || strcmp(password, "YOUR_WIFI_PASSWORD") == 0) {
    Serial.println("Error: Default WiFi credentials detected!");
    Serial.println("Set WIFI_SSID and WIFI_PASSWORD environment variables");
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("Failed to connect to WiFi!");
  }
}

void loop() {
  // Check WiFi connection status periodically
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck >= 30000) { // Check every 30 seconds
    lastCheck = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected. RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }
  }
  
  // Your main code will go here
  
  delay(100); // Small delay to prevent watchdog timer issues
}
