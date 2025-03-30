#include <Arduino.h>
#include <SensirionI2CScd4x.h> // Use SCD4x library
#include <Wire.h>

// Define pins for ESP8266 I2C
#define SDA_PIN D2  // GPIO4
#define SCL_PIN D1  // GPIO5

SensirionI2cScd4x scd4x; // Create an SCD4x sensor object (Corrected class name)
uint16_t error;         // Variable to store errors
char errorMessage[256]; // Buffer for error messages

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

  // Initialize SCD4x library
  scd4x.begin(Wire);

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
    uint16_t serialNumber[3];
    error = scd4x.getSerialNumber(serialNumber[0], serialNumber[1], serialNumber[2]);
    if (error) {
      Serial.print("Error getting serial number: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      Serial.print("SerialNumber: 0x");
      Serial.print(serialNumber[0], HEX);
      Serial.print(serialNumber[1], HEX);
      Serial.println(serialNumber[2], HEX);
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error) {
      Serial.print("Error starting periodic measurement: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
    } else {
      Serial.println("Periodic measurement started.");
    }
  } else {
     Serial.println("Skipping Sensor Initialization (Serial Number, Measurement Start) due to communication failure at 0x62.");
  }

  Serial.println("Waiting for first measurement... (takes approx. 5 seconds)");
}

void loop() {
  // Wait 5 seconds between measurements (as recommended by datasheet)
  delay(5000);

  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  bool isDataReady = false;

  // Check if data is ready
  error = scd4x.getDataReadyStatus(isDataReady);
  if (error) {
    Serial.print("Error checking data ready status: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    return; // Skip measurement if error
  }

  if (!isDataReady) {
    // Serial.println("Data not ready yet."); // Optional: uncomment for debugging
    return; // No new data available
  }

  // Read measurement data
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("Error reading measurement: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (co2 == 0) {
    Serial.println("Invalid CO2 reading (0 ppm), sensor might still be stabilizing.");
  } else {
    // Print results
    Serial.print("CO2:");
    Serial.print(co2);
    Serial.print("ppm\t");
    Serial.print("Temperature:");
    Serial.print(temperature, 1); // Print with 1 decimal place
    Serial.print("°C\t");
    Serial.print("Humidity:");
    Serial.print(humidity, 1); // Print with 1 decimal place
    Serial.println("%RH");
  }
}
