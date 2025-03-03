#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>
#include <ESP8266WiFi.h>

// Define pins for I2C
#define SDA_PIN 4
#define SCL_PIN 5

// WiFi credentials from environment variables
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Create SGP30 object
Adafruit_SGP30 sgp;

// Function prototypes
void scanI2CBus();
uint32_t getAbsoluteHumidity(float temperature, float humidity);

// Variables to store sensor readings
uint16_t TVOC = 0;
uint16_t eCO2 = 0;
uint32_t lastMeasurement = 0;
uint32_t lastBaseline = 0;
uint16_t TVOC_base, eCO2_base;
bool baselineValid = false;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("\nSGP30 Gas Sensor Test");

  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
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

  // Try to initialize SGP30 sensor with a few attempts
  int attempts = 0;
  bool sensorFound = false;
  
  // Try with different I2C frequencies
  uint32_t frequencies[] = {100000, 50000, 10000}; // Standard, slower, very slow
  
  for (uint8_t freqIndex = 0; freqIndex < 3 && !sensorFound; freqIndex++) {
    Wire.setClock(frequencies[freqIndex]);
    Serial.print("Setting I2C frequency to ");
    Serial.print(frequencies[freqIndex] / 1000);
    Serial.println(" kHz");
    
    attempts = 0;
    while (!sensorFound && attempts < 3) {
      Serial.print("Attempting to initialize SGP30 (attempt ");
      Serial.print(attempts + 1);
      Serial.print("/3 at ");
      Serial.print(frequencies[freqIndex] / 1000);
      Serial.println(" kHz)");
      
      // Print I2C pins being used
      Serial.print("Using I2C - SDA: ");
      Serial.print(SDA_PIN);
      Serial.print(", SCL: ");
      Serial.println(SCL_PIN);
      
      // Try direct I2C communication first
      Wire.beginTransmission(0x58); // SGP30 address
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.println("Direct I2C communication with SGP30 successful!");
      } else {
        Serial.print("Direct I2C communication failed with error: ");
        Serial.println(error);
      }
      
      // Now try the Adafruit library initialization
      sensorFound = sgp.begin();
      
      if (sensorFound) {
        Serial.println("SGP30 sensor found and initialized!");
      } else {
        Serial.println("SGP30 sensor initialization failed. Retrying in 1 second...");
        delay(1000);
        attempts++;
      }
    }
    
    if (sensorFound) {
      break; // Exit the frequency loop if sensor is found
    }
  }
  
  if (!sensorFound) {
    Serial.println("Failed to find SGP30 sensor after multiple attempts.");
    Serial.println("The program will continue but sensor readings will be invalid.");
  } else {
    // Sensor found, print serial number
    Serial.print("Found SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
  }

  // Set up initial baseline after 12 hours
  Serial.println("Waiting for sensor to warm up...");
}

void loop() {
  static uint8_t failCount = 0;
  static bool sensorWorking = true;
  
  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    
    if (sensorWorking) {
      // Measure TVOC and eCO2
      if (!sgp.IAQmeasure()) {
        failCount++;
        Serial.print("Measurement failed (");
        Serial.print(failCount);
        Serial.println("/5)");
        
        // After 5 consecutive failures, try to reinitialize
        if (failCount >= 5) {
          Serial.println("Too many failures, attempting to reinitialize sensor...");
          sensorWorking = sgp.begin();
          if (sensorWorking) {
            Serial.println("Sensor reinitialized successfully!");
            failCount = 0;
          } else {
            Serial.println("Failed to reinitialize sensor. Will retry later.");
          }
        }
      } else {
        // Reset fail counter on successful reading
        failCount = 0;
        
        // Read sensor values
        TVOC = sgp.TVOC;
        eCO2 = sgp.eCO2;
        
        // Print readings
        Serial.print("TVOC: "); Serial.print(TVOC); Serial.println(" ppb");
        Serial.print("eCO2: "); Serial.print(eCO2); Serial.println(" ppm");
      }
    } else {
      // Try to reinitialize the sensor periodically
      static uint32_t lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 30000) { // Try every 30 seconds
        lastReconnectAttempt = millis();
        Serial.println("Attempting to reconnect to sensor...");
        sensorWorking = sgp.begin();
        if (sensorWorking) {
          Serial.println("Sensor reconnected successfully!");
          failCount = 0;
        } else {
          Serial.println("Failed to reconnect to sensor. Will retry later.");
        }
      }
    }
    
    // Optional: Set absolute humidity to improve accuracy (uncomment if you have temp/humidity sensor)
    // float temperature = 22.1; // Replace with actual temperature reading
    // float humidity = 45.2;    // Replace with actual humidity reading
    // sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));
  }
  
  // Store baseline readings every hour
  if (millis() - lastBaseline > 3600000) {
    lastBaseline = millis();
    
    if (sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.print("Baseline values: eCO2: 0x");
      Serial.print(eCO2_base, HEX);
      Serial.print(", TVOC: 0x");
      Serial.println(TVOC_base, HEX);
      baselineValid = true;
    } else {
      Serial.println("Failed to get baseline readings");
    }
  }
  
  // Yield to prevent watchdog timer from triggering
  yield();
}

// Function to scan I2C bus for devices
void scanI2CBus() {
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int deviceCount = 0;
  bool sgp30Found = false;
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      
      // Check if this is the SGP30 address (0x58)
      if (address == 0x58) {
        Serial.println(" - This is the SGP30 sensor!");
        sgp30Found = true;
      } else if (address == 0x59) {
        Serial.println(" - This might be the SGP30 sensor (alternate address)!");
        sgp30Found = true;
      } else {
        Serial.println(" !");
      }
      
      deviceCount++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" device(s)");
    
    if (!sgp30Found) {
      Serial.println("WARNING: SGP30 sensor (expected at address 0x58) was NOT found!");
    }
  }
}

// Function to convert relative humidity to absolute humidity
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
