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
      switch (address) {
        case 0x59: Serial.println("SGP41)"); break;
        case 0x62: Serial.println("Unknown device - NOT SGP41)"); break;
        default: Serial.println("Unknown device)"); break;
      }
      nDevices++;
    }
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

  // Scan I2C bus to see if sensor is detected
  scanI2C();

  // Give sensor extra time to power up
  Serial.println("Waiting for sensor to initialize...");
  delay(2000);

  // Initialize SGP41 
  sgp41.begin(Wire);
  
  // Check if we can communicate with the sensor at the correct address
  Wire.beginTransmission(0x59);
  if (Wire.endTransmission() == 0) {
    Serial.println("SGP41 found at correct address (0x59) and initialized");
  } else {
    Serial.println("WARNING: SGP41 not found at expected address 0x59!");
    Serial.println("Check your wiring connections:");
    Serial.println("- SDA on sensor to D2 (GPIO4) on ESP8266");
    Serial.println("- SCL on sensor to D1 (GPIO5) on ESP8266");
    Serial.println("- VCC on sensor to 3.3V (NOT 5V) on ESP8266");
    Serial.println("- GND on sensor to GND on ESP8266");
    Serial.println("- Check for proper pull-up resistors (4.7kΩ) on SDA and SCL");
    delay(1000);
  }
  
  // Error handling for subsequent operations
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

  Serial.println("Initial conditioning phase starting...");
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
