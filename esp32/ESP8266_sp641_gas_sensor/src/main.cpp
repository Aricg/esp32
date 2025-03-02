#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>
#include <Wire.h>
#include <SensirionI2CSgp40.h>
#include <VOCGasIndexAlgorithm.h>

// WiFi connection details from build flags
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID" // Fallback if not defined
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" // Fallback if not defined
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// SGP40 sensor setup
SensirionI2CSgp40 sgp40;
VOCGasIndexAlgorithm vocAlgorithm;

// Conditioning duration in seconds
const uint16_t CONDITIONING_DURATION_S = 10;

// Variables to store sensor readings
int32_t vocIndex = 0;
uint16_t srawVoc = 0;

// Function to initialize the SGP40 sensor
void initSGP40() {
  uint16_t error;
  char errorMessage[256];
  
  // Initialize I2C bus for SGP40 sensor
  Wire.begin(4, 5); // SDA on GPIO4 (D2), SCL on GPIO5 (D1)
  
  // Initialize SGP40 sensor
  sgp40.begin(Wire);
  
  // VOCGasIndexAlgorithm is initialized automatically
  // No explicit initialization needed
  
  // Check if sensor is responding
  uint16_t serialNumber[3];
  error = sgp40.getSerialNumber(serialNumber, 3);
  
  if (error) {
    Serial.print("Error getting serial number: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("SGP40 Serial Number: ");
    Serial.print(serialNumber[0], HEX);
    Serial.print(serialNumber[1], HEX);
    Serial.println(serialNumber[2], HEX);
    Serial.println("SGP40 sensor detected!");
  }
  
  // Self-test
  uint16_t testResult;
  error = sgp40.executeSelfTest(testResult);
  if (error) {
    Serial.print("Error executing self-test: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (testResult != 0xD400) {
    Serial.print("Self-test failed, expected: 0xD400, got: 0x");
    Serial.println(testResult, HEX);
  } else {
    Serial.println("SGP40 self-test successful!");
  }
  
  // Conditioning phase
  Serial.println("Starting SGP40 conditioning phase...");
  uint16_t defaultRh = 0x8000; // 50% relative humidity
  uint16_t defaultT = 0x6666;  // 25°C
  
  for (uint16_t i = 0; i < CONDITIONING_DURATION_S; i++) {
    error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
    if (error) {
      Serial.print("Error during conditioning: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      break;
    }
    Serial.print("Conditioning: ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(CONDITIONING_DURATION_S);
    Serial.print(", SRAW_VOC: ");
    Serial.println(srawVoc);
    delay(1000); // Wait 1 second
  }
  
  Serial.println("SGP40 conditioning completed!");
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP8266 SGP40 Gas Sensor Test");
  
  // Check for default credentials
  if (strcmp(ssid, "YOUR_WIFI_SSID") == 0 || strcmp(password, "YOUR_WIFI_PASSWORD") == 0) {
    Serial.println("Error: Default WiFi credentials detected!");
    Serial.println("Set WIFI_SSID and WIFI_PASSWORD environment variables");
    return;
  }

  // Initialize SGP40 sensor
  Serial.println("Initializing SGP40 sensor...");
  initSGP40();
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
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
  uint16_t error;
  char errorMessage[256];
  static unsigned long lastSensorRead = 0;
  static unsigned long lastWifiCheck = 0;
  
  // Read sensor data every 1 second
  if (millis() - lastSensorRead >= 1000) {
    lastSensorRead = millis();
    
    // Default humidity and temperature values (can be replaced with actual sensor values)
    uint16_t defaultRh = 0x8000; // 50% relative humidity
    uint16_t defaultT = 0x6666;  // 25°C
    
    // Measure VOC raw signal
    error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
    
    if (error) {
      Serial.print("Error measuring raw signal: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      // Process raw signal with VOC Gas Index Algorithm
      vocIndex = vocAlgorithm.process(srawVoc);
      
      // Print sensor readings
      Serial.println("SGP40 Measurements:");
      Serial.print("SRAW_VOC: ");
      Serial.print(srawVoc);
      Serial.print(" | VOC Index: ");
      Serial.println(vocIndex);
      
      Serial.println("------------------------------");
    }
  }
  
  // Check WiFi connection status every 30 seconds
  if (millis() - lastWifiCheck >= 30000) {
    lastWifiCheck = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected. RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }
  }
  
  delay(100); // Small delay to prevent watchdog timer issues
}
