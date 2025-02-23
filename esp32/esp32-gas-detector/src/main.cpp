#include <Arduino.h>
#include "network_utils.h"

// Define MQ135 sensor pin
#define MQ135_PIN_AO 34

// WiFi and server configuration
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"  // Fallback if not defined
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Fallback if not defined
#endif

#ifndef SERVER_IP
#define SERVER_IP "192.168.1.100"  // Fallback if not defined
#endif

#ifndef SERVER_PORT
#define SERVER_PORT "5050"  // Fallback if not defined
#endif

// Construct server URL
const String SERVER_URL = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/data";

// Create network utilities instance
NetworkUtils network(WIFI_SSID, WIFI_PASSWORD, SERVER_URL.c_str());


void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("Serial connection established!");

  // Debug: Print SERVER_URL
  Serial.print("SERVER_URL: ");
  Serial.println(SERVER_URL);

  // Initialize analog pin
  pinMode(MQ135_PIN_AO, INPUT);
  
  // Always attempt to connect to WiFi
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

  // Convert to floating-point value
  float sensorValue = rawAnalog / 10.0;  // Adjust this conversion as needed

  // Post data to server only if SERVER_URL is set
  if (SERVER_URL.length() > 0) {
    if (!network.postSensorData("Temperature", sensorValue)) {
      Serial.println("Failed to post sensor data");
    }
  } else {
    Serial.print("Raw Value: ");
    Serial.println(rawAnalog);
  }

  // Wait 5 seconds before next reading
  delay(5000);
}
