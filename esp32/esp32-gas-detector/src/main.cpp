#include <Arduino.h>
#include "network_utils.h"

// Define MQ135 sensor pin
#define MQ135_PIN_AO 34

// WiFi and server configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverUrl = "http://your-aggregator-server.com/post";

// Create network utilities instance
NetworkUtils network(ssid, password, serverUrl);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("Serial connection established!");

  // Initialize analog pin
  pinMode(MQ135_PIN_AO, INPUT);
  
  // Connect to WiFi
  if (!network.connectToWiFi()) {
    Serial.println("Failed to connect to WiFi. Continuing in offline mode.");
  }
  
  Serial.println("MQ135 sensor initialized!");
  Serial.println("Waiting 5 seconds for sensor warm-up...");
  delay(5000);
  Serial.println("Starting sensor readings...");
}

void loop() {
  // Read raw analog value
  int rawAnalog = analogRead(MQ135_PIN_AO);

  // Convert value to string
  char dataString[10];
  snprintf(dataString, sizeof(dataString), "%d", rawAnalog);

  // Post data to server
  if (!network.postSensorData("mq135_sensor_1", dataString)) {
    Serial.println("Failed to post sensor data");
  }

  // Wait 5 seconds before next reading
  delay(5000);
}
