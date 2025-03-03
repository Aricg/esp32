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

// SGP40/41 I2C addresses
const uint8_t SGP40_I2C_ADDRESS = 0x59; // Default address for both SGP40 and SGP41
uint8_t alternativeI2CAddress = 0x62;   // Alternative address found in scan
bool useAlternativeAddress = false;
bool isSGP41 = false;                   // Flag to indicate if we're using SGP41

// I2C pins
const uint8_t SDA_PIN = 4; // D2 on NodeMCU
const uint8_t SCL_PIN = 5; // D1 on NodeMCU

// I2C configuration
const uint32_t I2C_FREQUENCY = 10000; // Use a very low frequency (10kHz) for reliability
const uint32_t I2C_STRETCH_LIMIT = 200000; // Longer clock stretching limit

// Flag to track if sensor is connected
bool sensorConnected = false;

// Function to calculate CRC8 for Sensirion sensors
uint8_t calculateCRC8(uint8_t* data, uint8_t len) {
  // CRC parameters for Sensirion sensors
  const uint8_t CRC8_POLYNOMIAL = 0x31;
  const uint8_t CRC8_INIT = 0xFF;
  
  uint8_t crc = CRC8_INIT;
  
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      } else {
        crc = (crc << 1);
      }
    }
  }
  
  return crc;
}

// Variables to store sensor readings
int32_t vocIndex = 0;
int32_t noxIndex = 0;
uint16_t srawVoc = 0;
uint16_t srawNox = 0;

// Function to scan I2C bus for devices
void scanI2CBus() {
  Serial.println("\n=== Scanning I2C bus ===");
  Serial.print("Using SDA pin: D");
  Serial.print(SDA_PIN == 4 ? "2" : String(SDA_PIN));
  Serial.print(" (GPIO");
  Serial.print(SDA_PIN);
  Serial.print("), SCL pin: D");
  Serial.print(SCL_PIN == 5 ? "1" : String(SCL_PIN));
  Serial.print(" (GPIO");
  Serial.print(SCL_PIN);
  Serial.println(")");
  Serial.print("I2C Frequency: ");
  Serial.print(I2C_FREQUENCY / 1000.0);
  Serial.println(" kHz");
  
  byte error, address;
  int deviceCount = 0;
  bool sgp40Found = false;
  
  // Re-initialize I2C with current settings
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQUENCY);
  Wire.setClockStretchLimit(I2C_STRETCH_LIMIT);
  delay(100);
  
  for(address = 1; address < 127; address++) {
    // Try multiple times for reliability
    bool deviceFound = false;
    for (int attempt = 0; attempt < 3; attempt++) {
      Wire.beginTransmission(address);
      error = Wire.endTransmission();
      if (error == 0) {
        deviceFound = true;
        break;
      }
      delay(10);
    }
    
    if (deviceFound) {
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
          Serial.print("Sensirion SP30/SGP41 or other sensor");
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
    Serial.println("Try adding 4.7k pull-up resistors from SDA/SCL to 3.3V.");
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
      Serial.println("5. Sensor might be using a different I2C address");
    }
  }
  Serial.println("=========================");
}

// Function to test if a device responds to SGP40-like commands
bool testSGP40Commands(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0x36); // Get serial ID command for SGP40
  Wire.write(0x82); // Command argument
  
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  delay(10);
  
  if (Wire.requestFrom((int)address, (int)9) != 9) {
    return false;
  }
  
  // Read and discard the data
  for (int i = 0; i < 9; i++) {
    Wire.read();
  }
  
  return true;
}

// Function to test if a device is an SGP41
bool testSGP41Device(uint8_t address) {
  // Try SGP41-specific command: measure raw signals
  Wire.beginTransmission(address);
  Wire.write(0x23); // SGP41 measure raw signals command
  Wire.write(0x03);
  Wire.write(0x00); // Default humidity
  Wire.write(0x80);
  Wire.write(0x00); // CRC for humidity
  Wire.write(0x66); // Default temperature
  Wire.write(0x66);
  Wire.write(0x93); // CRC for temperature
  Wire.write(0x00); // NOx compensation
  Wire.write(0x80);
  Wire.write(0xA2); // CRC for NOx compensation
  
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  delay(50); // SGP41 needs more time
  
  // SGP41 should return 6 bytes (2 for VOC, 1 CRC, 2 for NOx, 1 CRC)
  if (Wire.requestFrom((int)address, (int)6) != 6) {
    return false;
  }
  
  // Read the data
  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }
  
  // Check if we got reasonable data
  uint16_t rawVoc = (data[0] << 8) | data[1];
  uint16_t rawNox = (data[3] << 8) | data[4];
  
  // If we got non-zero values, it's likely an SGP41
  if (rawVoc > 0 && rawNox > 0) {
    Serial.print("SGP41 detected! Raw VOC: ");
    Serial.print(rawVoc);
    Serial.print(", Raw NOx: ");
    Serial.println(rawNox);
    return true;
  }
  
  return false;
}

// Function to try different I2C speeds and addresses
bool tryDifferentI2COptions() {
  const uint32_t speeds[] = {10000, 20000, 50000, 100000};
  const char* speedNames[] = {"10kHz", "20kHz", "50kHz", "100kHz"};
  const uint8_t possibleAddresses[] = {0x58, 0x59, 0x62, 0x61, 0x60}; // Try common Sensirion addresses
  
  for (int i = 0; i < 4; i++) {
    Serial.print("Trying I2C at ");
    Serial.print(speedNames[i]);
    Serial.println("...");
    
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(speeds[i]);
    Wire.setClockStretchLimit(I2C_STRETCH_LIMIT);
    delay(100);
    
    // Check all possible addresses
    for (int j = 0; j < 5; j++) {
      // Try multiple times for reliability
      bool deviceFound = false;
      for (int attempt = 0; attempt < 3; attempt++) {
        Wire.beginTransmission(possibleAddresses[j]);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
          deviceFound = true;
          break;
        }
        delay(10);
      }
      
      if (deviceFound) {
        Serial.print("Device found at address 0x");
        Serial.print(possibleAddresses[j], HEX);
        Serial.print(" with speed ");
        Serial.print(speedNames[i]);
        Serial.println("!");
        
        // Try to read from this device to confirm it's responsive
        Wire.beginTransmission(possibleAddresses[j]);
        Wire.write(0x36); // Common Sensirion command
        byte error = Wire.endTransmission();
        
        if (error == 0) {
          Serial.println("Device responds to Sensirion commands!");
        }
        
        if (possibleAddresses[j] != SGP40_I2C_ADDRESS) {
          Serial.println("NOTE: This is not the standard SGP40 address (0x59).");
          Serial.println("Will try to use this address instead.");
          
          // Update the alternative address
          alternativeI2CAddress = possibleAddresses[j];
          useAlternativeAddress = true;
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
  
  // Release I2C pins
  pinMode(SDA_PIN, INPUT);
  pinMode(SCL_PIN, INPUT);
  delay(50);
  
  // Toggle SDA line to unstick any stuck devices
  pinMode(SDA_PIN, OUTPUT);
  for (int i = 0; i < 16; i++) {
    digitalWrite(SDA_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(10);
  }
  
  // Send final STOP condition
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT);
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(10);
  pinMode(SCL_PIN, INPUT_PULLUP);
  
  delay(100);
  
  // Re-initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQUENCY);
  Wire.setClockStretchLimit(I2C_STRETCH_LIMIT);
  
  delay(100);
  
  Serial.println("I2C bus reset complete");
}

// Function to test I2C pins
void testI2CPins() {
  Serial.println("\n=== Testing I2C Pins ===");
  
  // Test SDA pin
  pinMode(SDA_PIN, INPUT_PULLUP);
  delay(10);
  bool sdaHigh = digitalRead(SDA_PIN);
  
  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delay(10);
  bool sdaCanGoLow = (digitalRead(SDA_PIN) == LOW);
  
  digitalWrite(SDA_PIN, HIGH);
  delay(10);
  bool sdaCanGoHigh = (digitalRead(SDA_PIN) == HIGH);
  
  pinMode(SDA_PIN, INPUT_PULLUP);
  delay(10);
  bool sdaPullup = digitalRead(SDA_PIN);
  
  // Test SCL pin
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(10);
  bool sclHigh = digitalRead(SCL_PIN);
  
  pinMode(SCL_PIN, OUTPUT);
  digitalWrite(SCL_PIN, LOW);
  delay(10);
  bool sclCanGoLow = (digitalRead(SCL_PIN) == LOW);
  
  digitalWrite(SCL_PIN, HIGH);
  delay(10);
  bool sclCanGoHigh = (digitalRead(SCL_PIN) == HIGH);
  
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(10);
  bool sclPullup = digitalRead(SCL_PIN);
  
  // Report results
  Serial.println("SDA Pin (D2/GPIO4) Test:");
  Serial.print("  Initial state (should be HIGH): ");
  Serial.println(sdaHigh ? "HIGH ✓" : "LOW ✗");
  Serial.print("  Can drive LOW: ");
  Serial.println(sdaCanGoLow ? "YES ✓" : "NO ✗");
  Serial.print("  Can drive HIGH: ");
  Serial.println(sdaCanGoHigh ? "YES ✓" : "NO ✗");
  Serial.print("  Pull-up working: ");
  Serial.println(sdaPullup ? "YES ✓" : "NO ✗");
  
  Serial.println("SCL Pin (D1/GPIO5) Test:");
  Serial.print("  Initial state (should be HIGH): ");
  Serial.println(sclHigh ? "HIGH ✓" : "LOW ✗");
  Serial.print("  Can drive LOW: ");
  Serial.println(sclCanGoLow ? "YES ✓" : "NO ✗");
  Serial.print("  Can drive HIGH: ");
  Serial.println(sclCanGoHigh ? "YES ✓" : "NO ✗");
  Serial.print("  Pull-up working: ");
  Serial.println(sclPullup ? "YES ✓" : "NO ✗");
  
  if (!sdaPullup || !sclPullup) {
    Serial.println("\n⚠️ WARNING: Pull-up resistors may be missing!");
    Serial.println("Add 4.7kΩ resistors from SDA/SCL to 3.3V");
  }
  
  Serial.println("=========================");
  
  // Reset pins to I2C mode
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQUENCY);
  Wire.setClockStretchLimit(I2C_STRETCH_LIMIT);
}

// Function to initialize the SGP40 sensor
bool initSGP40() {
  uint16_t error;
  char errorMessage[256];
  
  Serial.println("\n=== Initializing SGP40 sensor ===");
  
  // Initialize I2C bus for SGP40 sensor with clock stretching support
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQUENCY); // Use a very low frequency for reliability
  Wire.setClockStretchLimit(I2C_STRETCH_LIMIT);
  
  // Print pin configuration
  Serial.println("I2C Configuration:");
  Serial.print("SDA: D2 (GPIO4), SCL: D1 (GPIO5), Frequency: ");
  Serial.print(I2C_FREQUENCY / 1000.0);
  Serial.println(" kHz");
  
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
  Wire.beginTransmission(alternativeI2CAddress);
  if (Wire.endTransmission() == 0) {
    Serial.println("Found device at address 0x62 - this might be your sensor with a non-standard address");
    
    // First, check if it's an SGP41
    if (testSGP41Device(alternativeI2CAddress)) {
      Serial.println("Device at 0x62 appears to be an SGP41 sensor!");
      useAlternativeAddress = true;
      isSGP41 = true;
      return true;
    }
    
    // If not SGP41, try as SGP40
    Serial.println("Let's try to use this device directly as SGP40...");
    
    // Create a custom I2C implementation for address 0x62
    useAlternativeAddress = true;
    
    // We'll use direct I2C communication for this address since the library 
    // doesn't support changing the address
    Wire.beginTransmission(alternativeI2CAddress);
    Wire.write(0x20); // SGP40 measure command
    Wire.write(0x08);
    Wire.write(0x00); // Default humidity
    Wire.write(0x80);
    Wire.write(0x00);
    Wire.write(0x66); // Default temperature
    Wire.write(0x66);
    Wire.write(0x93); // CRC for default temperature
    
    if (Wire.endTransmission() == 0) {
      Serial.println("Successfully sent command to device at 0x62!");
      delay(30); // Wait for measurement
      
      // Read response
      if (Wire.requestFrom((int)alternativeI2CAddress, (int)3) == 3) {
        uint16_t rawValue = 0;
        uint8_t data[3];
        
        for (int i = 0; i < 3; i++) {
          data[i] = Wire.read();
        }
        
        rawValue = (data[0] << 8) | data[1];
        Serial.print("Raw value from 0x62: ");
        Serial.println(rawValue);
        
        if (rawValue > 0) {
          Serial.println("Device at 0x62 is responding to SGP40-like commands!");
          Serial.println("We'll try to use this device for measurements.");
        }
      } else {
        Serial.println("Failed to read from device at 0x62");
        useAlternativeAddress = false;
      }
    } else {
      Serial.println("Failed to send command to device at 0x62");
      useAlternativeAddress = false;
    }
  }
  
  // Also check if the standard address 0x59 is an SGP41
  Wire.beginTransmission(SGP40_I2C_ADDRESS);
  if (Wire.endTransmission() == 0) {
    if (testSGP41Device(SGP40_I2C_ADDRESS)) {
      Serial.println("Device at 0x59 appears to be an SGP41 sensor!");
      useAlternativeAddress = false;
      isSGP41 = true;
      return true;
    }
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

  // Test I2C pins first
  testI2CPins();
  
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
      
      // Measure raw signals
      if (isSGP41) {
        // Use SGP41-specific commands
        uint8_t address = useAlternativeAddress ? alternativeI2CAddress : SGP40_I2C_ADDRESS;
        
        // SGP41 measure raw signals command
        Wire.beginTransmission(address);
        Wire.write(0x23); // SGP41 measure raw signals command
        Wire.write(0x03);
        Wire.write(0x00); // Default humidity
        Wire.write(0x80);
        Wire.write(0x00); // CRC for humidity
        Wire.write(0x66); // Default temperature
        Wire.write(0x66);
        Wire.write(0x93); // CRC for temperature
        Wire.write(0x00); // NOx compensation
        Wire.write(0x80);
        Wire.write(0xA2); // CRC for NOx compensation
        
        error = Wire.endTransmission();
        
        if (error == 0) {
          delay(50); // SGP41 needs more time
          
          // Read response (6 bytes: 2 for VOC, 1 CRC, 2 for NOx, 1 CRC)
          if (Wire.requestFrom((int)address, (int)6) == 6) {
            uint8_t data[6];
            for (int i = 0; i < 6; i++) {
              data[i] = Wire.read();
            }
            
            srawVoc = (data[0] << 8) | data[1];
            srawNox = (data[3] << 8) | data[4];
            
            // Process both VOC and NOx values
            vocIndex = vocAlgorithm.process(srawVoc);
            noxIndex = vocAlgorithm.process(srawNox); // Using VOC algorithm for NOx too
            
            error = 0; // No error
          } else {
            error = 1; // Error reading data
          }
        }
      } else if (useAlternativeAddress) {
        // Use direct I2C communication for SGP40 at alternative address
        Wire.beginTransmission(alternativeI2CAddress);
        Wire.write(0x20); // SGP40 measure command
        Wire.write(0x08);
        Wire.write(0x00); // Default humidity
        Wire.write(0x80);
        Wire.write(0x00);
        Wire.write(0x66); // Default temperature
        Wire.write(0x66);
        Wire.write(0x93); // CRC for default temperature
        
        error = Wire.endTransmission();
        
        if (error == 0) {
          delay(30); // Wait for measurement
          
          // Read response
          if (Wire.requestFrom((int)alternativeI2CAddress, (int)3) == 3) {
            uint8_t data[3];
            for (int i = 0; i < 3; i++) {
              data[i] = Wire.read();
            }
            
            srawVoc = (data[0] << 8) | data[1];
            vocIndex = vocAlgorithm.process(srawVoc);
            error = 0; // No error
          } else {
            error = 1; // Error reading data
          }
        }
      } else {
        // Use the library for the standard SGP40 address
        error = sgp40.measureRawSignal(defaultRh, defaultT, srawVoc);
        if (error == 0) {
          vocIndex = vocAlgorithm.process(srawVoc);
        }
      }
      
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
        if (isSGP41) {
          Serial.println("SGP41 Measurements:");
          Serial.print("SRAW_VOC: ");
          Serial.print(srawVoc);
          Serial.print(" | VOC Index: ");
          Serial.println(vocIndex);
          
          Serial.print("SRAW_NOx: ");
          Serial.print(srawNox);
          Serial.print(" | NOx Index: ");
          Serial.println(noxIndex);
          
          // Add more detailed information
          Serial.print("VOC raw value in hex: 0x");
          Serial.println(srawVoc, HEX);
          Serial.print("NOx raw value in hex: 0x");
          Serial.println(srawNox, HEX);
          
          // VOC Index interpretation
          Serial.print("VOC Air Quality: ");
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
          
          // NOx Index interpretation
          Serial.print("NOx Air Quality: ");
          if (noxIndex <= 10) {
            Serial.println("Excellent");
          } else if (noxIndex <= 50) {
            Serial.println("Good");
          } else if (noxIndex <= 100) {
            Serial.println("Moderate");
          } else if (noxIndex <= 150) {
            Serial.println("Poor");
          } else if (noxIndex <= 200) {
            Serial.println("Unhealthy");
          } else {
            Serial.println("Very Unhealthy");
          }
        } else {
          Serial.println("SGP40 Measurements:");
          Serial.print("SRAW_VOC: ");
          Serial.print(srawVoc);
          Serial.print(" | VOC Index: ");
          Serial.println(vocIndex);
          
          // Add more detailed information about the raw value
          Serial.print("Raw value in hex: 0x");
          Serial.println(srawVoc, HEX);
          
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
        }
        
        Serial.print("Using address: 0x");
        Serial.print(useAlternativeAddress ? alternativeI2CAddress : SGP40_I2C_ADDRESS, HEX);
        Serial.print(" (");
        Serial.print(isSGP41 ? "SGP41" : "SGP40");
        Serial.println(")");
        
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
