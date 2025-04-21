#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP30.h> // Include SGP30 library
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

// Create sensor objects
Adafruit_SGP40 sgp40;
Adafruit_SGP30 sgp30; // Add SGP30 sensor object

// Function prototypes
void scanI2CBus();
// String detectSensorType(uint8_t address); // Removed unused prototype
void sendSensorData(const char* sensorName, int sensorValue);

// Global variables for sensor control
uint8_t detectedSensorAddress = 0x00; // Store detected address (0 if none)
bool isSGP30 = false; // Flag for detected sensor type
bool isSGP40 = false; // Flag for detected sensor type

// Variables to store sensor readings
uint16_t TVOC = 0; // Total Volatile Organic Compounds
uint16_t eCO2 = 0;
uint32_t lastMeasurement = 0;
uint32_t lastBaseline = 0;
bool readSuccess = false; // Initialize global read success flag

void setup() {
  // Initialize serial communication
  Serial.begin(115200); // Match monitor speed
  delay(1000); // Give serial port time to initialize
  Serial.println("\n\n--- SGP30/SGP40 Gas Sensor Test ---"); // Update title

  // Wait for sensor to power up fully
  delay(1000); // Reduced delay
  Serial.println("Initializing I2C...");
  
  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Set I2C clock to 100kHz
  Serial.println("I2C Initialized (SDA: " + String(SDA_PIN) + ", SCL: " + String(SCL_PIN) + ", Clock: 100kHz)");
  delay(100); // Give I2C time to initialize

  // Scan I2C bus to find the sensor address
  scanI2CBus(); // This will update detectedSensorAddress

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

  // Initialize the detected sensor
  if (detectedSensorAddress == 0x58) {
      Serial.println("Attempting to initialize SGP30 at 0x58...");
      if (!sgp30.begin()){
          Serial.println("SGP30 sensor not found!");
          // Keep trying? Or just stop here? For now, just report.
      } else {
          Serial.println("SGP30 sensor initialized successfully!");
          isSGP30 = true;
          Serial.print("Found SGP30 serial #");
          Serial.print(sgp30.serialnumber[0], HEX);
          Serial.print(sgp30.serialnumber[1], HEX);
          Serial.println(sgp30.serialnumber[2], HEX);
      }
  } else if (detectedSensorAddress == 0x59) {
      Serial.println("Attempting to initialize SGP40 at 0x59...");
      if (!sgp40.begin()){
          Serial.println("SGP40 sensor not found!");
      } else {
          Serial.println("SGP40 sensor initialized successfully!");
          isSGP40 = true;
          // SGP40 doesn't have a readily accessible serial number like SGP30 via library
      }
  } else {
      Serial.println("No supported sensor (SGP30/SGP40) detected at 0x58 or 0x59.");
      Serial.println("Please check wiring and I2C address.");
      // Optional: Enter a loop or halt? For now, let loop run but it won't read.
  }

  Serial.println("Setup complete. Starting measurements...");
}

void loop() {
  // static uint8_t failCount = 0; // Removed unused variable
  // static bool sensorWorking = true; // Replaced by isSGP30/isSGP40 flags
  static uint32_t printInterval = 0;
  // bool readSuccess = false; // Moved to global scope

  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    // readSuccess = false; // Reset success flag for this measurement cycle <-- REMOVED: Only set false on actual failure

    if (isSGP30) {
        // Read SGP30
        if (! sgp30.IAQmeasure()) {
          Serial.println("SGP30 Measurement failed");
          readSuccess = false; // Set false on failure
        } else {
          TVOC = sgp30.TVOC;
          eCO2 = sgp30.eCO2;
          readSuccess = true;
          // Print readings every 5 seconds to reduce serial traffic
          if (millis() - printInterval > 5000) {
              printInterval = millis();
              Serial.print("SGP30 Reading -> TVOC: "); Serial.print(TVOC); Serial.print(" ppb\t");
              Serial.print("eCO2: "); Serial.print(eCO2); Serial.println(" ppm");
          }
        }
    } else if (isSGP40) {
        // Read SGP40
        // Optional: Provide humidity compensation if available
        // uint16_t compRh = 0; // Placeholder: calculate actual RH compensation value
        // uint16_t compT = 0;  // Placeholder: calculate actual Temp compensation value
        // int32_t raw_reading = sgp40.measureRaw(compRh, compT);
        int32_t raw_reading = sgp40.measureRaw(); // Use uncompensated reading

        if (raw_reading == 0x8000) { // Check for SGP40 error code
            Serial.println("SGP40 Measurement failed (error code)");
            readSuccess = false; // Set false on failure
        } else {
            // Process the raw reading to get a VOC index (0-500)
            // The library's example uses a fixed mapping, let's stick to that for consistency
            // but the previous mapping seemed reasonable too. Let's use the previous one.
            int voc_index;
            if (raw_reading > 40000) {
              voc_index = 500; // Very high VOC
            } else if (raw_reading < 20000) {
              voc_index = 0; // Very low VOC
            } else {
              // Map 20000-40000 to 0-500
              voc_index = map(raw_reading, 20000, 40000, 0, 500);
            }
            TVOC = voc_index; // For SGP40, use TVOC variable to store the calculated VOC Index
            // eCO2 = 0; // SGP40 does not measure eCO2, ensure it's not carrying an old value if needed, though sending logic prevents it.
            readSuccess = true;

            // Print readings every 5 seconds to reduce serial traffic
            if (millis() - printInterval > 5000) {
                printInterval = millis();
                Serial.print("SGP40 Reading -> Raw: "); Serial.print(raw_reading);
                Serial.print(", VOC Index: "); Serial.println(TVOC); // TVOC holds voc_index here
            }
        }
    } else {
        // No sensor initialized, print message occasionally
        if (millis() - printInterval > 10000) {
            printInterval = millis();
            Serial.println("No sensor initialized. Waiting...");
        }
    }

    // Optional: Handle persistent read failures (e.g., try re-initializing)
    // This part is removed for simplicity now, relying on initial setup success.
  }

  // Perform periodic maintenance (only relevant for SGP30 baseline)
  if (isSGP30 && (millis() - lastBaseline > 3600000)) { // Every hour
    lastBaseline = millis();
    uint16_t tvoc_base, eco2_base;
    if (! sgp30.getIAQBaseline(&eco2_base, &tvoc_base)) {
      Serial.println("Failed to get SGP30 baseline.");
    } else {
      Serial.print("SGP30 Baseline values: eCO2: 0x"); Serial.print(eco2_base, HEX);
      Serial.print(", TVOC: 0x"); Serial.println(tvoc_base, HEX);
      // Note: Add code here to save baseline to non-volatile memory if needed
    }
  } else if (isSGP40 && (millis() - lastBaseline > 3600000)) {
      lastBaseline = millis();
      Serial.println("SGP40 hourly checkpoint (no baseline operation needed).");
  }

  // Send data to metrics server every postInterval
  if (millis() - lastPostTime > postInterval) {
    lastPostTime = millis();

    // Only send if a sensor is initialized and last read was successful
    if (readSuccess) {
        if (isSGP30) {
            Serial.print("Sending SGP30 data: TVOC=");
            Serial.print(TVOC);
            Serial.print(", eCO2=");
            Serial.println(eCO2);
            // Send SGP30 TVOC data
            sendSensorData("SGP30_TVOC", TVOC);
            // Send SGP30 eCO2 data
            sendSensorData("SGP30_eCO2", eCO2);
        } else if (isSGP40) {
            Serial.print("Sending SGP40 data: TVOC="); // Reverted label for serial output
            Serial.println(TVOC); // Remember TVOC holds VOC Index for SGP40
            // Send SGP40 VOC Index data (using the TVOC variable) with the original name
            sendSensorData("TVOC", TVOC); // Reverted sensor name for data sending
            // Do NOT send eCO2 for SGP40
        }
    } else if (!isSGP30 && !isSGP40) {
        Serial.println("No sensor active, skipping data send.");
    } else { // Sensor is active but last read failed
        Serial.println("Last read failed, skipping data send.");
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
  detectedSensorAddress = 0x00; // Reset detected address before scan

  for(address = 1; address < 127; address++) {
    // delay(10); // Small delay between transmissions for stability - Often not needed
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("Device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      if (address == 0x58) {
        Serial.println(" (SGP30 Address)");
        detectedSensorAddress = address; // Store detected address
      } else if (address == 0x59) {
        Serial.println(" (SGP40 Address)");
        detectedSensorAddress = address; // Store detected address
      } else {
        Serial.println(" (Unknown device)");
      }

      deviceCount++;
      // delay(50); // Give serial time to send - Usually not needed
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

// Removed detectSensorType function as it's replaced by direct initialization attempts

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
      // String response = http.getString(); // Read response only if needed
      Serial.print(" -> ");
      Serial.print(sensorName);
      Serial.print("=");
      Serial.print(sensorValue);
      Serial.print(" | HTTP POST ");
      Serial.print(httpResponseCode);
      // Serial.println(response); // Uncomment to see full response body
      if (httpResponseCode == HTTP_CODE_OK) {
          Serial.println(" OK");
      } else {
          Serial.println(" (Non-OK response)");
      }
    } else {
      Serial.print(" -> ");
      Serial.print(sensorName);
      Serial.print("=");
      Serial.print(sensorValue);
      Serial.print(" | Error sending POST: ");
      Serial.println(httpResponseCode);
    }
    
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// The getAbsoluteHumidity function was removed as it's not needed for SGP40
