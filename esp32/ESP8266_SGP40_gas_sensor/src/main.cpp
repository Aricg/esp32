#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
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
Adafruit_SGP40 sgp40;

// Function prototypes
void scanI2CBus();
String detectSensorType(uint8_t address);
void sendSensorData(const char* sensorName, int sensorValue);

// Global variables for sensor control
uint8_t sensorAddress = 0x59; // SGP40 address
String sensorType = "SGP40"; // We're focusing on SGP40 only

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

  // Initialize SGP40 sensor
  Serial.println("Initializing SGP40 sensor...");
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
  
  // Initialize SGP40
  bool sensorFound = false;
  
  for (int attempt = 1; attempt <= 3 && !sensorFound; attempt++) {
    Serial.print("SGP40 init attempt ");
    Serial.print(attempt);
    Serial.println("/3");
    delay(50);
    
    if (sgp40.begin()) {
      Serial.println("SGP40 sensor initialized successfully!");
      sensorFound = true;
      sensorType = "SGP40";
      delay(50);
    } else {
      Serial.println("SGP40 init failed");
      delay(500);
    }
  }
  
  if (!sensorFound) {
    Serial.println("Failed to find SGP40 sensor after multiple attempts.");
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
      
      // SGP40 library reading - get raw value and convert to VOC index
      int32_t raw_reading = sgp40.measureRaw();
      if (raw_reading >= 0) {
        // Process the raw reading to get a VOC index
        // SGP40 raw values are typically in the 20,000-40,000 range
        int voc_index;
        if (raw_reading > 40000) {
          voc_index = 500; // Very high VOC
        } else if (raw_reading < 20000) {
          voc_index = 0; // Very low VOC
        } else {
          // Map 20000-40000 to 0-500
          voc_index = map(raw_reading, 20000, 40000, 0, 500);
        }
        
        TVOC = voc_index;
        
        // Convert VOC index to approximate eCO2 (very rough estimate)
        // VOC index of 100 is "normal", higher is worse air quality
        if (voc_index < 100) {
          eCO2 = 400 + voc_index; // Minimal CO2 increase for good air
        } else {
          eCO2 = 500 + (voc_index - 100) * 5; // More aggressive scaling for poor air
        }
        
        readSuccess = true;
        
        // Debug info
        Serial.print("SGP40 Raw: ");
        Serial.print(raw_reading);
        Serial.print(", VOC Index: ");
        Serial.println(voc_index);
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
          
          sensorWorking = sgp40.begin();
          
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
        sensorWorking = sgp40.begin();
        if (sensorWorking) {
          Serial.println("Sensor reconnected");
          delay(10);
          failCount = 0;
        }
      }
    }
    
    // Optional: Set absolute humidity to improve accuracy (uncomment if you have temp/humidity sensor)
    // float temperature = 22.1; // Replace with actual temperature reading
    // float humidity = 45.2;    // Replace with actual humidity reading
    // sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));
  }
  
  // Perform periodic maintenance every hour
  if (millis() - lastBaseline > 3600000) {
    lastBaseline = millis();
    
    // SGP40 doesn't have the same baseline concept as SGP30
    Serial.println("SGP40 hourly maintenance checkpoint");
  }
  
  // Send data to metrics server every 5 seconds
  if (millis() - lastPostTime > postInterval) {
    lastPostTime = millis();
    
    // Only send if we have valid readings
    if (sensorWorking) {
      // Send TVOC data
      sendSensorData("TVOC", TVOC);
      
      // Send eCO2 data
      sendSensorData("eCO2", eCO2);
      
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
