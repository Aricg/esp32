#include <Arduino.h>
#include <SensirionI2CSgp41.h>
#include <Wire.h>

// Define pins for ESP8266 I2C
#define SDA_PIN D2  // GPIO4
#define SCL_PIN D1  // GPIO5

SensirionI2CSgp41 sgp41;
uint16_t conditioning_s = 10; // Initial conditioning phase in seconds

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
      if (address == 0x59) {
        Serial.println("Expected SGP41)");
      } else if (address == 0x62) {
        Serial.println("Detected 0x62 - THIS IS NOT THE EXPECTED SGP41 ADDRESS!)");
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

  Serial.println("\nESP8266 SGP41 Gas Sensor Test");

  // Initialize I2C with proper pins for ESP8266
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Lower I2C clock speed to 100kHz for stability

  // Scan I2C bus
  scanI2C();

  // Give sensor extra time to power up
  Serial.println("Waiting for sensor to initialize...");
  delay(1000); // Reduced delay slightly

  // Initialize SGP41 
  sgp41.begin(Wire);
  
  // Check if we can communicate with the SGP41 at its expected address
  bool sgp41_found = false;
  Wire.beginTransmission(0x59); // SGP41 Address
  if (Wire.endTransmission() == 0) {
    Serial.println("Communication successful with device at expected SGP41 address 0x59.");
    sgp41_found = true;
  } else {
    Serial.println("ERROR: Failed to communicate with device at expected SGP41 address 0x59!");
    Serial.println("-> Please RE-VERIFY the physical sensor type and ALL wiring connections:");
    Serial.println("   - SENSOR TYPE: Ensure it is truly an SGP41.");
    Serial.println("   - SDA: Sensor SDA to ESP8266 D2 (GPIO4)");
    Serial.println("   - SCL: Sensor SCL to ESP8266 D1 (GPIO5)");
    Serial.println("   - VCC: Sensor VCC to ESP8266 3.3V (MUST be 3.3V, NOT 5V!)");
    Serial.println("   - GND: Sensor GND to ESP8266 GND");
    Serial.println("   - PULL-UPS: Ensure 4.7kOhm pull-up resistors are present on SDA and SCL lines to 3.3V.");
    Serial.println("-> The I2C scan detected a device at 0x62, which is NOT the SGP41.");
    Serial.println("   Continuing initialization attempt, but errors are expected.");
    delay(5000); // Pause to allow reading the error
  }

  // Initialize SGP41 library (will use address 0x59 internally)
  sgp41.begin(Wire);

  // Only proceed with detailed checks if communication at 0x59 was initially successful
  if (sgp41_found) {
      uint16_t error;
  char errorMessage[256];
  
  // Get and print serial number
  uint16_t serialNumber[3];
    error = sgp41.getSerialNumber(serialNumber);
    if (error) {
      Serial.print("Error getting serial number: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      Serial.print("SerialNumber: 0x");
      for (size_t i = 0; i < 3; i++) {
        Serial.print(serialNumber[i], HEX);
      }
      Serial.println();
    }
    
    // Execute self test
    uint16_t testResult;
    error = sgp41.executeSelfTest(testResult);
    if (error) {
      Serial.print("Error in self test: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else if (testResult != 0xD400) {
      Serial.print("Self test failed with error: ");
      Serial.println(testResult, HEX);
    } else {
      Serial.println("Self test passed");
    }
  } else {
     Serial.println("Skipping Serial Number check and Self Test due to communication failure at 0x59.");
  }

  Serial.println("Initial conditioning phase starting (will likely fail if communication error persists)...");
}

void loop() {
  static uint8_t failCount = 0;
  const uint8_t maxFails = 5;

  uint16_t error;
  char errorMessage[256];
  uint16_t defaultRh = 0x8000; // 50% RH
  uint16_t defaultT = 0x6666;  // 25°C
  uint16_t srawVoc = 0;
  uint16_t srawNox = 0;

  delay(1000);

  if (conditioning_s > 0) {
    Serial.print("Conditioning: ");
    Serial.print(conditioning_s);
    Serial.println(" seconds remaining");

    error = sgp41.executeConditioning(defaultRh, defaultT, srawVoc);
    conditioning_s--;
  } else {
    error = sgp41.measureRawSignals(defaultRh, defaultT, srawVoc, srawNox);
  }

  if (error) {
    Serial.print("Error executing measurement: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);

    failCount++;
    if (failCount >= maxFails) {
      Serial.println("Too many failures, rescanning I2C bus...");
      scanI2C();
      failCount = 0;
    }
  } else {
    failCount = 0;
    if (conditioning_s <= 0) {
      Serial.print("SRAW_VOC: ");
      Serial.print(srawVoc);
      Serial.print("\t");
      Serial.print("SRAW_NOx: ");
      Serial.println(srawNox);
    }
  }
}
