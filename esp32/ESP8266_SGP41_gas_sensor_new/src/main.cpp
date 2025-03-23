#include <Arduino.h>
#include <SensirionI2CSgp41.h>
#include <Wire.h>

SensirionI2CSgp41 sgp41;

// Time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }

    Wire.begin();
    sgp41.begin(Wire);

    // Get and print serial number
    uint16_t error;
    char errorMessage[256];
    uint16_t serialNumber[3];
    error = sgp41.getSerialNumber(serialNumber);

    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
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
        Serial.print("Error trying to execute executeSelfTest(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else if (testResult != 0xD400) {
        Serial.print("executeSelfTest failed with error: ");
        Serial.println(testResult);
    }
}

void loop() {
    uint16_t error;
    char errorMessage[256];
    uint16_t defaultRh = 0x8000;  // 50% RH
    uint16_t defaultT = 0x6666;   // 25°C
    uint16_t srawVoc = 0;
    uint16_t srawNox = 0;

    delay(1000);

    if (conditioning_s > 0) {
        // During NOx conditioning (10s) SRAW NOx will remain 0
        error = sgp41.executeConditioning(defaultRh, defaultT, srawVoc);
        conditioning_s--;
    } else {
        // Read Measurement
        error = sgp41.measureRawSignals(defaultRh, defaultT, srawVoc, srawNox);
    }

    if (error) {
        Serial.print("Error trying to execute measureRawSignals(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("SRAW_VOC:");
        Serial.print(srawVoc);
        Serial.print("\t");
        Serial.print("SRAW_NOx:");
        Serial.println(srawNox);
    }
}
#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSgp41.h>

SensirionI2CSgp41 sgp41;
int conditioning_s = 10; // Initial conditioning phase in seconds

void scanI2C() {
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;
    
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.println();
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
    // SDA = GPIO4 (D2), SCL = GPIO5 (D1)
    Wire.begin(); 
    Wire.setClock(100000); // Lower I2C clock speed to 100kHz for stability
    
    // Scan I2C bus to see if sensor is detected
    scanI2C();  
    
    // Give sensor extra time to power up
    Serial.println("Waiting for sensor to initialize...");
    delay(2000);
    
    uint16_t error;
    char errorMessage[256];
    
    error = sgp41.begin(Wire);
    if (error) {
        Serial.print("Error initializing SGP41: ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        Serial.println("Check your wiring and power supply. SGP41 should be at address 0x59.");
    } else {
        Serial.println("SGP41 initialized successfully");
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
