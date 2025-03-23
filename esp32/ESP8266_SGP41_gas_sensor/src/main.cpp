#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSgp41.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Define pins for I2C
#define SDA_PIN 4
#define SCL_PIN 5

// WiFi credentials from environment variables
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Metrics server configuration
const char* serverUrl = "http://192.168.88.126:5000/data";
const unsigned long postInterval = 10000; // Post data every 10 seconds
unsigned long lastPostTime = 0;

// Create sensor object
SensirionI2CSgp41 sgp41;

// Function prototypes
void scanI2CBus();
String detectSensorType(uint8_t address);
void sendSensorData(const char* sensorName, int sensorValue);

// Global variables for sensor control
uint8_t sensorAddress = 0x59; // SGP41 address
String sensorType = "SGP41"; // We're focusing on SGP41 only

// Variables to store sensor readings
uint16_t TVOC = 0;
uint16_t eCO2 = 0;
uint32_t lastMeasurement = 0;
uint32_t lastBaseline = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  delay(1000); // Give serial port time to initialize
  Serial.println("\n\n--- SGP40 Gas Sensor Test ---");

  // Wait for sensor to power up fully
  delay(2000);
  Serial.println("Initializing I2C...");
  
  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(500); // Give I2C time to initialize
  
  // Scan I2C bus to see what devices are connected
  scanI2CBus();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize SGP41 sensor
  Serial.println("Initializing SGP41 sensor...");
  delay(50);
  
  // Set I2C to a slower speed for better reliability
  Wire.setClock(10000); // 10 kHz
  Serial.println("I2C clock set to 10 kHz for stability");
  delay(50);
  
  Serial.print("Using I2C pins - SDA: ");
  Serial.print(SDA_PIN);
  Serial.print(", SCL: ");
  Serial.println(SCL_PIN);
  delay(50);
  
  // End any pending transmission
  Wire.endTransmission(true);
  delay(100);
  
  // Re-initialize I2C with pullups enabled
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);
  
  // Initialize SGP41
  bool sensorFound = false;
  
  for (int attempt = 1; attempt <= 3 && !sensorFound; attempt++) {
    Serial.print("SGP41 init attempt ");
    Serial.print(attempt);
    Serial.println("/3");
    delay(50);
    
    Wire.beginTransmission(0x59);
    if (Wire.endTransmission() == 0) {
      sgp41.begin(Wire);
      Serial.println("SGP41 sensor initialized successfully!");
      sensorFound = true;
      sensorType = "SGP41";
      delay(50);
    } else {
      Serial.println("SGP41 init failed");
      delay(500);
    }
  }
  
  if (!sensorFound) {
    Serial.println("Failed to find SGP41 sensor after multiple attempts.");
    Serial.println("The program will continue but sensor readings will be invalid.");
  }

  // Set up initial baseline after 12 hours
  Serial.println("Waiting for sensor to warm up...");
}

void loop() {
  static uint8_t failCount = 0;
  static bool sensorWorking = true;
  static uint32_t printInterval = 0;
  
  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    
    if (sensorWorking) {
      bool readSuccess = false;
      
      // SGP41 measurement variables
      uint16_t error;
      uint16_t srawVoc = 0;
      uint16_t srawNox = 0;
      int32_t vocIndex = 0;
      int32_t noxIndex = 0;
      
      // Use default values for humidity and temperature (50% RH, 25°C)
      uint16_t defaultRh = 0x8000; // 50% RH in fixed point format
      uint16_t defaultT = 0x6666;  // 25°C in fixed point format
      
      // Measure raw signals with default compensation
      error = sgp41.measureRawSignals(defaultRh, defaultT, srawVoc, srawNox);
      
      if (error == 0) {
        // Calculate VOC and NOx indices
        vocIndex = srawVoc; // SGP41 raw VOC is already in index format
        noxIndex = srawNox; // SGP41 raw NOx is already in index format
        
        // Update our global variables
        TVOC = vocIndex;
        eCO2 = noxIndex;
        
        readSuccess = true;
        
        // Debug info
        Serial.print("SGP41 VOC Index: ");
        Serial.print(vocIndex);
        Serial.print(", NOx Index: ");
        Serial.println(noxIndex);
      }
      
      if (!readSuccess) {
        failCount++;
        
        // Only print every 5 seconds to reduce serial traffic
        if (millis() - printInterval > 5000) {
          printInterval = millis();
          Serial.print("Measurement failed (");
          Serial.print(failCount);
          Serial.println("/5)");
          delay(10);
        }
        
        // After 5 consecutive failures, try to reinitialize
        if (failCount >= 5) {
          Serial.println("Reinitializing sensor...");
          delay(10);
          
          Wire.beginTransmission(0x59);
          sensorWorking = (Wire.endTransmission() == 0);
          if (sensorWorking) {
            sgp41.begin(Wire);
          }
          
          if (sensorWorking) {
            Serial.println("Sensor reinitialized OK");
            delay(10);
            failCount = 0;
          }
        }
      } else {
        // Reset fail counter on successful reading
        failCount = 0;
        
        // Print readings every 5 seconds to reduce serial traffic
        if (millis() - printInterval > 5000) {
          printInterval = millis();
          Serial.print("TVOC: "); 
          Serial.print(TVOC); 
          Serial.print(" ppb, eCO2: "); 
          Serial.print(eCO2); 
          Serial.println(" ppm");
          delay(10);
        }
      }
    } else {
      // Try to reinitialize the sensor periodically
      static uint32_t lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 30000) { // Try every 30 seconds
        lastReconnectAttempt = millis();
        Serial.println("Reconnecting to sensor...");
        delay(10);
        Wire.beginTransmission(0x59);
        sensorWorking = (Wire.endTransmission() == 0);
        if (sensorWorking) {
          sgp41.begin(Wire);
        }
        if (sensorWorking) {
          Serial.println("Sensor reconnected");
          delay(10);
          failCount = 0;
        }
      }
    }
    
    // Optional: Replace default humidity and temperature values with actual sensor readings
    // To convert float values to fixed-point format for the SGP41:
    // uint16_t rhForSgp41 = uint16_t(humidity * 65535 / 100); // Convert from % to ticks
    // uint16_t tempForSgp41 = uint16_t((temperature + 45) * 65535 / 175); // Convert from °C to ticks
  }
  
  // Perform periodic maintenance every hour
  if (millis() - lastBaseline > 3600000) {
    lastBaseline = millis();
    
    // SGP41 also doesn't have the same baseline concept as SGP30
    Serial.println("SGP41 hourly maintenance checkpoint");
  }
  
  // Send data to metrics server every 5 seconds
  if (millis() - lastPostTime > postInterval) {
    lastPostTime = millis();
    
    // Only send if we have valid readings
    if (sensorWorking) {
      // Send VOC Index data
      sendSensorData("VOC", TVOC);
      
      // Send NOx Index data
      sendSensorData("NOx", eCO2);
      
      Serial.println("Data sent to metrics server");
    }
  }
  
  // Yield to prevent watchdog timer from triggering
  yield();
}

// Function to scan I2C bus for devices
void scanI2CBus() {
  Serial.println("Scanning I2C bus...");
  delay(50);
  
  byte error, address;
  int deviceCount = 0;
  
  // The SGP30 should be at address 0x58
  for(address = 1; address < 127; address++) {
    delay(10); // Small delay between transmissions for stability
    
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      
      if (address == 0x58) {
        Serial.println(" (SGP30 sensor)");
      } else if (address == 0x59) {
        Serial.println(" (Possible SGP30 alternate address)");
      } else {
        Serial.println(" (Unknown device)");
      }
      
      deviceCount++;
      delay(50); // Give serial time to send
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found!");
    delay(50);
  } else {
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" device(s)");
    delay(50);
  }
}

// Simplified function to detect SGP40/41 sensor
String detectSensorType(uint8_t address) {
  byte error;
  String type = "Unknown";
  
  Serial.print("Attempting to identify sensor at address 0x");
  Serial.println(address, HEX);
  delay(50);
  
  // Try SGP40-specific command (Measure Raw Signal)
  Wire.beginTransmission(address);
  Wire.write(0x26); // Measure Raw Signal command
  Wire.write(0x0F);
  error = Wire.endTransmission();
  
  if (error == 0) {
    delay(30); // SGP40 needs time to process
    if (Wire.requestFrom((uint8_t)address, (uint8_t)3) == 3) {
      byte data[3];
      for (int i = 0; i < 3; i++) {
        data[i] = Wire.read();
      }
      
      // If we got a valid response (not all 0xFF), it's likely an SGP40
      if (data[0] != 0xFF || data[1] != 0xFF) {
        type = "SGP40";
        Serial.println("SGP40 identified by raw signal response");
        return type;
      }
    }
  }
  
  // Try SGP41-specific command (Execute Conditioning)
  Wire.beginTransmission(address);
  Wire.write(0x26); // Execute Conditioning command
  Wire.write(0x12);
  error = Wire.endTransmission();
  
  if (error == 0) {
    delay(50); // SGP41 needs time to process
    if (Wire.requestFrom((uint8_t)address, (uint8_t)3) == 3) {
      byte data[3];
      for (int i = 0; i < 3; i++) {
        data[i] = Wire.read();
      }
      
      // If we got a valid response (not all 0xFF), it's likely an SGP41
      if (data[0] != 0xFF || data[1] != 0xFF) {
        type = "SGP41";
        Serial.println("SGP41 identified by conditioning response");
        return type;
      }
    }
  }
  
  Serial.println("Could not identify specific sensor type");
  return type;
}

// Function to send sensor data to the metrics server
void sendSensorData(const char* sensorName, int sensorValue) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    // Configure the request
    http.begin(client, serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    String payload = "{\"sensor_name\": \"";
    payload += sensorName;
    payload += "\", \"sensor_value\": ";
    payload += sensorValue;
    payload += "}";
    
    // Send the request
    int httpResponseCode = http.POST(payload);
    
    // Check response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      // Serial.println(response); // Uncomment to see full response
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// The getAbsoluteHumidity function was removed as it's not needed for SGP40
