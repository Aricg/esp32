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

// SGP40 I2C address (default is 0x59)
const uint8_t SGP40_I2C_ADDRESS = 0x59;

// I2C pins
const uint8_t SDA_PIN = 4; // D2 on NodeMCU
const uint8_t SCL_PIN = 5; // D1 on NodeMCU

// Flag to track if sensor is connected
bool sensorConnected = false;

// Variables to store sensor readings
int32_t vocIndex = 0;
uint16_t srawVoc = 0;

// Function to scan I2C bus for devices
void scanI2CBus() {
  Serial.println("\n=== Scanning I2C bus ===");
  byte error, address;
  int deviceCount = 0;
  bool sgp40Found = false;
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.print(" (");
      
      // Print known device names
      switch(address) {
        case 0x58: // Some SGP40 might use this address
          Serial.print("Possible SGP40 sensor");
          break;
        case 0x59: // SGP40/41 default address
          Serial.print("SGP40/41 sensor");
          sgp40Found = true;
          break;
        case 0x62: // SP30 or other Sensirion sensor
          Serial.print("Sensirion SP30 or other sensor");
          break;
        default:
          Serial.print("unknown device");
          break;
      }
      
      Serial.println(")");
      deviceCount++;
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found!");
    Serial.println("Check your wiring and pull-up resistors.");
  } else {
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" device(s).");
    
    if (!sgp40Found) {
      Serial.println("WARNING: SGP40 sensor (0x59) not found!");
      Serial.println("Possible issues:");
      Serial.println("1. Incorrect wiring (check SDA/SCL connections)");
      Serial.println("2. Missing pull-up resistors (2.2k-10k ohm to 3.3V)");
      Serial.println("3. Sensor power issue (needs 3.3V)");
      Serial.println("4. Sensor may be damaged");
    }
  }
  Serial.println("=========================");
}

// Function to try different I2C speeds and addresses
bool tryDifferentI2COptions() {
  const uint32_t speeds[] = {10000, 50000, 100000, 400000};
  const char* speedNames[] = {"10kHz", "50kHz", "100kHz", "400kHz"};
  const uint8_t possibleAddresses[] = {0x58, 0x59, 0x62}; // Try common Sensirion addresses
  
  for (int i = 0; i < 4; i++) {
    Serial.print("Trying I2C at ");
    Serial.print(speedNames[i]);
    Serial.println("...");
    
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(speeds[i]);
    Wire.setClockStretchLimit(200000);
    delay(100);
    
    // Check all possible addresses
    for (int j = 0; j < 3; j++) {
      Wire.beginTransmission(possibleAddresses[j]);
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.print("Device found at address 0x");
        Serial.print(possibleAddresses[j], HEX);
        Serial.print(" with speed ");
        Serial.print(speedNames[i]);
        Serial.println("!");
        
        if (possibleAddresses[j] != SGP40_I2C_ADDRESS) {
          Serial.println("NOTE: This is not the standard SGP40 address (0x59).");
          Serial.println("You may need to modify the code to use this address.");
        }
        
        return true;
      }
    }
    
    delay(100);
  }
  
  Serial.println("Failed at all I2C speeds and addresses");
  return false;
}

// Function to reset I2C bus
void resetI2CBus() {
  Serial.println("Resetting I2C bus...");
  
  // Toggle SDA line to unstick any stuck devices
  pinMode(SDA_PIN, OUTPUT);
  for (int i = 0; i < 16; i++) {
    digitalWrite(SDA_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(5);
  }
  
  // Send final STOP condition
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT);
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(5);
  pinMode(SCL_PIN, INPUT_PULLUP);
  
  delay(100);
  
  // Re-initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setClockStretchLimit(200000);
  
  delay(100);
}

// Function to initialize the SGP40 sensor
bool initSGP40() {
  uint16_t error;
  char errorMessage[256];
  
  Serial.println("\n=== Initializing SGP40 sensor ===");
  
  // Initialize I2C bus for SGP40 sensor with clock stretching support
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Set to standard 100kHz
  Wire.setClockStretchLimit(200000); // Increase clock stretching limit
  
  // Scan for I2C devices
  scanI2CBus();
  
  // Check if we need to try different I2C speeds and addresses
  Wire.beginTransmission(SGP40_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("SGP40 not responding at default settings, trying alternatives...");
    if (!tryDifferentI2COptions()) {
      Serial.println("Failed to communicate with SGP40 with any settings");
      resetI2CBus();
      return false;
    }
  }
  
  // Check if there's a device at address 0x62 (which was found in your scan)
  Wire.beginTransmission(0x62);
  if (Wire.endTransmission() == 0) {
    Serial.println("Found device at address 0x62 - this might be your sensor with a non-standard address");
    Serial.println("Attempting to use this device instead...");
    // Note: The SGP40 library doesn't allow changing the address, so we'll continue with 0x59
    // but this information might be useful for debugging
  }
  
  // Initialize SGP40 sensor
  sgp40.begin(Wire);
  
  // Check if sensor is responding
  uint16_t serialNumber[3];
  error = sgp40.getSerialNumber(serialNumber, 3);
  
  if (error) {
    Serial.print("Error getting serial number: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    
    // Try resetting the I2C bus
    resetI2CBus();
    sgp40.begin(Wire);
    
    // Try again
    error = sgp40.getSerialNumber(serialNumber, 3);
    if (error) {
      Serial.println("Still can't get serial number after reset");
      return false;
    }
  }
  
  Serial.print("SGP40 Serial Number: ");
  Serial.print(serialNumber[0], HEX);
  Serial.print(serialNumber[1], HEX);
  Serial.println(serialNumber[2], HEX);
  Serial.println("SGP40 sensor detected!");
  
  // Self-test
  uint16_t testResult;
  error = sgp40.executeSelfTest(testResult);
  if (error) {
    Serial.print("Error executing self-test: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    return false;
  } else if (testResult != 0xD400) {
    Serial.print("Self-test failed, expected: 0xD400, got: 0x");
    Serial.println(testResult, HEX);
    return false;
  } else {
    Serial.println("SGP40 self-test successful!");
  }
  
  // Conditioning phase
  Serial.println("Starting SGP40 conditioning phase...");
  uint16_t defaultRh = 0x8000; // 50% relative humidity
  uint16_t defaultT = 0x6666;  // 25°C
  
  bool conditioningSuccess = false;
  int successfulReadings = 0;
  
  for (uint16_t i = 0; i < CONDITIONING_DURATION_S; i++) {
    error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
    if (error) {
      Serial.print("Error during conditioning: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      
      // Try with a small delay and retry
      delay(100);
      error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
      if (!error) {
        successfulReadings++;
        Serial.print("Retry successful! Conditioning: ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.print(CONDITIONING_DURATION_S);
        Serial.print(", SRAW_VOC: ");
        Serial.println(srawVoc);
      } else {
        Serial.println("Retry failed.");
      }
    } else {
      successfulReadings++;
      Serial.print("Conditioning: ");
      Serial.print(i + 1);
      Serial.print("/");
      Serial.print(CONDITIONING_DURATION_S);
      Serial.print(", SRAW_VOC: ");
      Serial.println(srawVoc);
    }
    delay(1000); // Wait 1 second
  }
  
  if (successfulReadings > 0) {
    conditioningSuccess = true;
    Serial.print("SGP40 conditioning completed with ");
    Serial.print(successfulReadings);
    Serial.print("/");
    Serial.print(CONDITIONING_DURATION_S);
    Serial.println(" successful readings.");
  } else {
    Serial.println("SGP40 conditioning failed completely.");
    Serial.println("Check sensor wiring and connections.");
    return false;
  }
  
  sensorConnected = conditioningSuccess;
  return conditioningSuccess;
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
  sensorConnected = initSGP40();
  
  if (!sensorConnected) {
    Serial.println("WARNING: Could not initialize SGP40 sensor properly.");
    Serial.println("Will continue with WiFi setup, but sensor readings may fail.");
  }
  
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
  
  // Read sensor data every 3 seconds
  if (millis() - lastSensorRead >= 3000) {
    lastSensorRead = millis();
    
    if (!sensorConnected) {
      // Try to reconnect to sensor periodically
      static unsigned long lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt >= 30000) { // Try every 30 seconds
        lastReconnectAttempt = millis();
        Serial.println("\nAttempting to reconnect to SGP40 sensor...");
        sensorConnected = initSGP40();
      }
    } else {
      // Default humidity and temperature values (can be replaced with actual sensor values)
      uint16_t defaultRh = 0x8000; // 50% relative humidity
      uint16_t defaultT = 0x6666;  // 25°C
      
      static int errorCount = 0;
      static bool rescanNeeded = false;
      
      // Measure VOC raw signal
      error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
      
      if (error) {
        errorCount++;
        Serial.print("Error measuring raw signal: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        
        // If we get multiple consecutive errors, try to reset the I2C bus
        if (errorCount > 3 && !rescanNeeded) {
          Serial.println("Multiple errors detected. Rescanning I2C bus...");
          scanI2CBus();
          rescanNeeded = true;
        }
        
        // Try to reset the sensor connection after 5 consecutive errors
        if (errorCount > 5) {
          Serial.println("Attempting to reset sensor connection...");
          resetI2CBus();
          sgp40.begin(Wire);
          errorCount = 0;
          
          // Check if we've lost the sensor completely
          if (errorCount > 10) {
            Serial.println("Too many errors, marking sensor as disconnected");
            sensorConnected = false;
          }
        }
      } else {
        // Reset error count on successful reading
        errorCount = 0;
        rescanNeeded = false;
        
        // Process raw signal with VOC Gas Index Algorithm
        vocIndex = vocAlgorithm.process(srawVoc);
        
        // Print sensor readings
        Serial.println("SGP40 Measurements:");
        Serial.print("SRAW_VOC: ");
        Serial.print(srawVoc);
        Serial.print(" | VOC Index: ");
        Serial.println(vocIndex);
        
        // VOC Index interpretation
        Serial.print("Air Quality: ");
        if (vocIndex <= 10) {
          Serial.println("Excellent");
        } else if (vocIndex <= 50) {
          Serial.println("Good");
        } else if (vocIndex <= 100) {
          Serial.println("Moderate");
        } else if (vocIndex <= 150) {
          Serial.println("Poor");
        } else if (vocIndex <= 200) {
          Serial.println("Unhealthy");
        } else {
          Serial.println("Very Unhealthy");
        }
        
        Serial.println("------------------------------");
      }
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
