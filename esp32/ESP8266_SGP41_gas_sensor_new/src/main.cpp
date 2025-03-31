#include <Arduino.h>
#include <SensirionI2CScd4x.h> // Use SCD4x library
#include <Wire.h>
#include <ESP8266WiFi.h>       // For WiFi connectivity
#include <ESP8266HTTPClient.h> // For making HTTP requests
#include <WiFiClient.h>        // Required for HTTPClient

// Define pins for ESP8266 I2C
#define SDA_PIN D2  // GPIO4
#define SCL_PIN D1  // GPIO5

SensirionI2cScd4x scd4x; // Create an SCD4x sensor object (Corrected class name)
uint16_t error;         // Variable to store errors
char errorMessage[256]; // Buffer for error messages

// WiFi credentials (set via build flags)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Web server configuration
const char* serverUrl = "http://192.168.88.126:5000/data";
const unsigned long postInterval = 10000; // Post data every 10 seconds
unsigned long lastPostTime = 0;

// Function prototypes
void connectToWiFi();
void sendSensorData(const char* sensorName, float sensorValue); // Updated prototype

// Flag to track sensor stabilization
bool sensorStabilized = false;

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      if (address == 0x62) { // SCD4x address
        Serial.println("Expected SCD4x)");
      } else if (address == 0x59) {
        Serial.println("Detected 0x59 - THIS IS NOT THE EXPECTED SCD4x ADDRESS!)");
      } else {
         Serial.println("Unknown device)");
      }
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
    // Ignore other error codes (2:NACK addr, 3:NACK data) as they mean no device responded
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.print("Found ");
    Serial.print(nDevices);
    Serial.println(" device(s)");
  }
  Serial.println("I2C scan complete");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Give time for the serial monitor to connect

  Serial.println("\nESP8266 SCD4x (SCD40/SCD41) CO2 Sensor Test"); // Updated title

  // Initialize I2C with proper pins for ESP8266
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Lower I2C clock speed to 100kHz for stability

  // Scan I2C bus
  scanI2C();

  // Give sensor extra time to power up
  Serial.println("Waiting for sensor to initialize...");
  delay(1000); 

  // Initialize SCD4x library, providing the I2C address
  scd4x.begin(Wire, 0x62); // Pass Wire object and the I2C address

  // Note: softReset() call removed as it's not available in this library version per compiler error.
  // Reset might occur implicitly during begin() or stopPeriodicMeasurement().

  // Check if we can communicate with the SCD4x at its expected address
  bool scd4x_found = false;
  Wire.beginTransmission(0x62); // SCD4x Address
  if (Wire.endTransmission() == 0) {
    Serial.println("Communication successful with device at expected SCD4x address 0x62.");
    scd4x_found = true;
  } else {
    Serial.println("ERROR: Failed to communicate with device at expected SCD4x address 0x62!");
    Serial.println("-> Please RE-VERIFY the physical sensor type and ALL wiring connections:");
    Serial.println("   - SENSOR TYPE: Ensure it is an SCD40 or SCD41.");
    Serial.println("   - SDA: Sensor SDA to ESP8266 D2 (GPIO4)");
    Serial.println("   - SCL: Sensor SCL to ESP8266 D1 (GPIO5)");
    Serial.println("   - VCC: Sensor VCC to ESP8266 3.3V (Check sensor datasheet, SCD4x often supports 2.4-5.5V)");
    Serial.println("   - GND: Sensor GND to ESP8266 GND");
    Serial.println("   - PULL-UPS: Ensure 4.7kOhm pull-up resistors are present on SDA and SCL lines to 3.3V.");
    Serial.println("-> The I2C scan might have detected other devices if present.");
    Serial.println("   Continuing initialization attempt, but errors are expected if 0x62 is not the SCD4x.");
    delay(5000); // Pause to allow reading the error
  }

  // Stop potentially previously running measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    Serial.print("Error stopping periodic measurement: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  delay(500); // Wait for sensor command processing

  // Only proceed if communication at 0x62 was initially successful
  if (scd4x_found) {
    // Get and print serial number
    uint64_t serialNumber; // Use uint64_t for the serial number
    error = scd4x.getSerialNumber(serialNumber); // Pass the single variable
    if (error) {
      Serial.print("Error getting serial number: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      Serial.print("SerialNumber: 0x");
      // Print the 64-bit hex value. Need to print high and low parts separately for ESP8266 Serial.print
      uint32_t high = (uint32_t)(serialNumber >> 32);
      uint32_t low = (uint32_t)(serialNumber & 0xFFFFFFFF);
      if (high > 0) { // Only print high part if it's not zero
          Serial.print(high, HEX);
      }
      // Pad the low part with leading zeros if necessary
      char lowStr[9];
      sprintf(lowStr, "%08X", low); // Format as 8-digit hex
      Serial.print(lowStr);
      Serial.println();
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error) {
      Serial.print("Error starting periodic measurement: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      Serial.println("Periodic measurement started.");

      // Disable Automatic Self-Calibration (ASC)
      Serial.println("Disabling Automatic Self-Calibration (ASC)...");
      error = scd4x.setAutomaticSelfCalibrationEnabled(false); // Correct function name
      if (error) {
          Serial.print("Error disabling ASC: ");
          errorToString(error, errorMessage, 256);
          Serial.println(errorMessage);
      } else {
          Serial.println("ASC disabled successfully.");
      }
      delay(100); // Wait a bit after command
    }
  } else {
     Serial.println("Skipping Sensor Initialization (Serial Number, Measurement Start, ASC) due to communication failure at 0x62.");
  }

  Serial.println("Waiting for first measurement... (takes approx. 5 seconds)");

  // Connect to WiFi
  connectToWiFi();
}

// Function to connect to WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop() {
  // Wait 5 seconds between measurements (as recommended by datasheet)
  delay(5000);

  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  bool isDataReady = false;

  // Check if data is ready
  Serial.println("Checking if sensor data is ready...");
  error = scd4x.getDataReadyStatus(isDataReady);
  if (error) {
    Serial.print("Error checking data ready status. Code: ");
    Serial.print(error);
    Serial.print(" Message: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    return; // Skip measurement if error
  }

  if (!isDataReady) {
     Serial.println("Sensor data not ready yet. Skipping read attempt.");
    return; // No new data available
  }

  // Data is ready, attempt to read measurement
  Serial.println("Sensor data ready. Reading measurement...");
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("Error reading measurement. Code: ");
    Serial.print(error);
    Serial.print(" Message: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    // Note: We might still attempt to post old data if postInterval is met below,
    // depending on desired behavior. Current logic proceeds. Consider adding 'return;' here
    // if you want to skip posting entirely on read error.
  } else {
    Serial.println("Measurement read successfully.");
    // Print results regardless of CO2 value for debugging stabilization
    if (co2 == 0) {
        if (sensorStabilized) {
             Serial.print("CO2: 0 ppm (Warning: Reading 0 after stabilization!)");
        } else {
             Serial.print("CO2: 0 ppm (Stabilizing?)");
        }
    } else {
        Serial.print("CO2:");
        Serial.print(co2);
        Serial.print("ppm");
    }
    Serial.print("\t");
    Serial.print("Temperature:");
    Serial.print(temperature, 1); // Print with 1 decimal place
    Serial.print("°C\t");
    Serial.print("Humidity:");
    Serial.print(humidity, 1); // Print with 1 decimal place
    Serial.println("%RH");

    // Check if sensor has provided its first valid reading
    if (!sensorStabilized && co2 > 0) {
        sensorStabilized = true;
        Serial.println("Sensor stabilized: First valid CO2 reading received.");
    }

    // Send data to server periodically ONLY after stabilization
    if (sensorStabilized && (millis() - lastPostTime > postInterval)) {
      lastPostTime = millis();
      // Send each metric separately
      sendSensorData("CO2", (float)co2); // Cast co2 (uint16_t) to float for the function
      sendSensorData("Temperature", temperature);
      sendSensorData("Humidity", humidity);
      Serial.println("Sensor data sent to server.");
    } else if (!sensorStabilized) {
      Serial.println("Sensor not yet stabilized, skipping data send.");
    }
  }

  // Yield to allow background processes (like WiFi) to run
  yield();
}

// Function to send a single sensor reading to the metrics server
void sendSensorData(const char* sensorName, float sensorValue) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    // Configure the request
    // Serial.print("Connecting to server: "); // Reduce serial noise
    // Serial.println(serverUrl);
    http.begin(client, serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload for a single metric
    String payload = "{";
    payload += "\"sensor_name\": \"" + String(sensorName) + "\",";
    // Format float value with 1 decimal place
    payload += "\"sensor_value\": " + String(sensorValue, 1);
    payload += "}";

    Serial.print("Sending payload: ");
    Serial.println(payload);

    // Send the request
    int httpResponseCode = http.POST(payload);

    // Check response
    if (httpResponseCode > 0) {
      // String response = http.getString(); // Read response only if needed
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      // Serial.println(response);
    } else {
      Serial.print("Error on sending POST for ");
      Serial.print(sensorName);
      Serial.print(": ");
      Serial.println(httpResponseCode);
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    // Free resources
    http.end();
  } else {
    Serial.println("WiFi not connected, cannot send data.");
    // Optional: try to reconnect?
    // connectToWiFi(); // Be careful about blocking the loop here
  }
}
