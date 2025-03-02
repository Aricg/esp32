#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

// WiFi connection details will be retrieved from environment variables
const char* ssid;
const char* password;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP8266 WiFi Connectivity Test");
  Serial.println("-------------------------------");

  // Get WiFi credentials from environment variables
  ssid = getenv("WIFI_SSID");
  password = getenv("WIFI_PASSWORD");

  if (!ssid || !password) {
    Serial.println("Error: WiFi credentials not found!");
    Serial.println("Make sure WIFI_SSID and WIFI_PASSWORD environment variables are set.");
    return;
  }

  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi network!");
    Serial.print("Status code: ");
    Serial.println(WiFi.status());
  }
}

void loop() {
  // Check WiFi connection status periodically
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to WiFi. Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("WiFi connection lost!");
  }
  
  delay(10000); // Check every 10 seconds
}
