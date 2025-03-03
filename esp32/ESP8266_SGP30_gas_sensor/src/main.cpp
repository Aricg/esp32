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
String detectSensorType(uint8_t address);
uint32_t getAbsoluteHumidity(float temperature, float humidity);

// Global variables for manual reading
bool useManualReading = false;
uint8_t sensorAddress = 0x58; // Default address
String sensorType = "Unknown"; // Will store detected sensor type: SGP30, SGP40, SGP41, or Unknown

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
  delay(1000); // Give serial port time to initialize
  Serial.println("\n\n--- SGP30 Gas Sensor Test ---");

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

  // Try to initialize SGP30 sensor
  Serial.println("Initializing SGP30 sensor...");
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
  
  // Try a gentler I2C reset approach
  Serial.println("Performing gentle I2C reset...");
  
  // End any pending transmission
  Wire.endTransmission(true);
  delay(100);
  
  // Re-initialize I2C with pullups enabled
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(200);
  
  // Try direct I2C communication first with both addresses
  Wire.beginTransmission(0x58); // Standard SGP address
  byte error = Wire.endTransmission();
  bool foundAt0x58 = (error == 0);
  
  if (foundAt0x58) {
    Serial.println("Direct I2C communication with sensor at 0x58 successful");
    delay(50);
    
    // Try to identify which sensor we have
    sensorType = detectSensorType(0x58);
    Serial.print("Detected sensor type at 0x58: ");
    Serial.println(sensorType);
  } else {
    Serial.print("Communication with 0x58 failed with error: ");
    Serial.println(error);
    delay(50);
  }
  
  // Try alternate address
  Wire.beginTransmission(0x59);
  error = Wire.endTransmission();
  bool foundAt0x59 = (error == 0);
  
  if (foundAt0x59) {
    Serial.println("Found device at alternate address 0x59");
    Serial.println("Will attempt to use this device instead");
    delay(50);
    
    // Try to identify which sensor we have at 0x59
    sensorType = detectSensorType(0x59);
    Serial.print("Detected sensor type at 0x59: ");
    Serial.println(sensorType);
  } else {
    Serial.print("Communication with 0x59 failed with error: ");
    Serial.println(error);
    delay(50);
  }
  
  // Try to read some data from the device at 0x59
  if (foundAt0x59) {
    Wire.beginTransmission(0x59);
    Wire.write(0x20); // Command to read serial ID
    Wire.write(0x03);
    error = Wire.endTransmission();
    
    if (error == 0) {
      delay(10);
      Wire.requestFrom((uint8_t)0x59, (uint8_t)6);
      if (Wire.available() >= 6) {
        Serial.print("Raw data from 0x59: ");
        for (int i = 0; i < 6; i++) {
          byte data = Wire.read();
          Serial.print("0x");
          if (data < 16) Serial.print("0");
          Serial.print(data, HEX);
          Serial.print(" ");
        }
        Serial.println();
      } else {
        Serial.println("Not enough data received from 0x59");
      }
    } else {
      Serial.println("Failed to send command to 0x59");
    }
  }
  
  // Now try the Adafruit library initialization with multiple attempts
  bool sensorFound = false;
  
  // First try with standard library
  for (int attempt = 1; attempt <= 2 && !sensorFound; attempt++) {
    Serial.print("SGP30 init attempt ");
    Serial.print(attempt);
    Serial.println("/2 (standard address)");
    delay(50);
    
    sensorFound = sgp.begin();
    
    if (sensorFound) {
      Serial.println("SGP30 sensor initialized successfully at 0x58!");
      delay(50);
    } else {
      Serial.println("SGP30 init failed at standard address");
      delay(500);
    }
  }
  
  // If not found, try manual initialization for device at 0x59
  if (!sensorFound && foundAt0x59) {
    Serial.println("Attempting manual initialization for device at 0x59");
    delay(50);
    
    // Try to initialize the sensor with a custom I2C address
    // This is a workaround since the Adafruit library doesn't support changing the address
    
    // First, send init command to 0x59
    Wire.beginTransmission(0x59);
    Wire.write(0x20); // Init air quality command
    Wire.write(0x03);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.println("Sent init command to device at 0x59");
      delay(10);
      
      // Wait for sensor initialization (10ms according to datasheet)
      delay(10);
      
      // Try to read from the device to confirm it's working
      Wire.beginTransmission(0x59);
      Wire.write(0x20); // Measure air quality command
      Wire.write(0x08);
      error = Wire.endTransmission();
      
      if (error == 0) {
        delay(50); // Wait for measurement
        
        // Read measurement data
        Wire.requestFrom(0x59, 6);
        if (Wire.available() >= 6) {
          uint16_t co2 = (Wire.read() << 8) | Wire.read();
          Wire.read(); // CRC
          uint16_t tvoc = (Wire.read() << 8) | Wire.read();
          Wire.read(); // CRC
          
          Serial.println("Manual reading from device at 0x59:");
          Serial.print("CO2: "); Serial.print(co2); Serial.println(" ppm");
          Serial.print("TVOC: "); Serial.print(tvoc); Serial.println(" ppb");
          
          // If we got reasonable values, consider the sensor found
          if (co2 > 0 && co2 < 60000 && tvoc < 60000) {
            sensorFound = true;
            useManualReading = true;
            sensorAddress = 0x59;
            Serial.println("Manual initialization successful!");
          } else {
            Serial.println("Received invalid values from device");
          }
        } else {
          Serial.println("Not enough data received from device");
        }
      } else {
        Serial.println("Failed to send measure command");
      }
    } else {
      Serial.println("Failed to send init command");
    }
  }
  
  if (!sensorFound) {
    // Try one more approach - direct communication with device at 0x59
    if (foundAt0x59) {
      Serial.println("Trying direct communication with device at 0x59 as last resort...");
      
      // Try different commands for different sensor types
      bool success = false;
      
      // Try SGP30 command
      Wire.beginTransmission(0x59);
      Wire.write(0x20); // Measure air quality command for SGP30
      Wire.write(0x08);
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        delay(50);
        if (Wire.requestFrom((uint8_t)0x59, (uint8_t)6) == 6) {
          uint16_t co2 = (Wire.read() << 8) | Wire.read();
          Wire.read(); // CRC
          uint16_t tvoc = (Wire.read() << 8) | Wire.read();
          Wire.read(); // CRC
          
          if (co2 > 0 && co2 < 60000 && tvoc < 60000) {
            Serial.println("SGP30-compatible device detected at 0x59!");
            sensorFound = true;
            useManualReading = true;
            sensorAddress = 0x59;
            sensorType = "SGP30";
            success = true;
          }
        }
      }
      
      // If SGP30 failed, try SGP40/41
      if (!success) {
        Wire.beginTransmission(0x59);
        Wire.write(0x26); // Raw signal command for SGP40/41
        Wire.write(0x0F);
        error = Wire.endTransmission();
        
        if (error == 0) {
          delay(50);
          if (Wire.requestFrom((uint8_t)0x59, (uint8_t)3) == 3) {
            uint16_t raw = (Wire.read() << 8) | Wire.read();
            Wire.read(); // CRC
            
            if (raw > 0 && raw < 60000) {
              Serial.println("SGP40/41-compatible device detected at 0x59!");
              sensorFound = true;
              useManualReading = true;
              sensorAddress = 0x59;
              sensorType = "SGP40";
              success = true;
            }
          }
        }
      }
    }
    
    if (!sensorFound) {
      Serial.println("Failed to find any SGP sensor after multiple attempts.");
      Serial.println("The program will continue but sensor readings will be invalid.");
    }
  } else {
    // Sensor found, print serial number if using standard library
    if (!useManualReading) {
      Serial.print("Found SGP30 serial #");
      Serial.print(sgp.serialnumber[0], HEX);
      Serial.print(sgp.serialnumber[1], HEX);
      Serial.println(sgp.serialnumber[2], HEX);
    }
    
    // Print which mode we're using
    Serial.print("Using ");
    if (useManualReading) {
      Serial.print("manual reading at address 0x");
      Serial.print(sensorAddress, HEX);
    } else {
      Serial.print("Adafruit library at standard address");
    }
    
    Serial.print(" - Detected sensor type: ");
    Serial.println(sensorType);
  }

  // Set up initial baseline after 12 hours
  Serial.println("Waiting for sensor to warm up...");
}

void loop() {
  static uint8_t failCount = 0;
  static bool sensorWorking = true;
  static uint32_t printInterval = 0;
  static bool manualAttemptMade = false;
  
  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    
    // If sensor not working and we haven't tried manual mode yet, try it
    if (!sensorWorking && !manualAttemptMade && !useManualReading) {
      Serial.println("Trying manual communication with device at 0x59...");
      
      // Try to communicate with the device at 0x59
      Wire.beginTransmission(0x59);
      byte error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.println("Device found at 0x59, attempting manual initialization");
        
        // Initialize the sensor
        Wire.beginTransmission(0x59);
        Wire.write(0x20); // Init air quality command
        Wire.write(0x03);
        error = Wire.endTransmission();
        
        if (error == 0) {
          Serial.println("Manual initialization successful!");
          useManualReading = true;
          sensorAddress = 0x59;
          sensorWorking = true;
          failCount = 0;
        }
      }
      
      manualAttemptMade = true;
    }
    
    if (sensorWorking) {
      bool readSuccess = false;
      
      if (useManualReading) {
        // Manual reading for device at alternate address
        if (sensorType == "SGP40" || sensorType == "SGP41") {
          // SGP40/41 use different commands
          Wire.beginTransmission(sensorAddress);
          Wire.write(0x26); // Measure Raw command
          Wire.write(0x0F);
          byte error = Wire.endTransmission();
          
          if (error == 0) {
            delay(50); // Wait for measurement
            
            // Read measurement data (raw VOC only)
            Wire.requestFrom((uint8_t)sensorAddress, (uint8_t)3);
            if (Wire.available() >= 3) {
              uint16_t rawVOC = (Wire.read() << 8) | Wire.read();
              Wire.read(); // CRC
              
              // Convert raw reading to approximate TVOC
              TVOC = rawVOC / 10; // Rough approximation
              eCO2 = 400 + (TVOC * 3); // Very rough approximation
              readSuccess = true;
            }
          }
        } else {
          // Default to SGP30 protocol
          Wire.beginTransmission(sensorAddress);
          Wire.write(0x20); // Measure air quality command
          Wire.write(0x08);
          byte error = Wire.endTransmission();
          
          if (error == 0) {
            delay(50); // Wait for measurement
            
            // Read measurement data
            Wire.requestFrom((uint8_t)sensorAddress, (uint8_t)6);
            if (Wire.available() >= 6) {
              eCO2 = (Wire.read() << 8) | Wire.read();
              Wire.read(); // CRC
              TVOC = (Wire.read() << 8) | Wire.read();
              Wire.read(); // CRC
              readSuccess = true;
            }
          }
        }
      } else {
        // Standard library reading
        readSuccess = sgp.IAQmeasure();
        if (readSuccess) {
          TVOC = sgp.TVOC;
          eCO2 = sgp.eCO2;
        }
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
          
          if (useManualReading) {
            // Try to reinitialize manually
            Wire.beginTransmission(sensorAddress);
            Wire.write(0x20); // Init air quality command
            Wire.write(0x03);
            byte error = Wire.endTransmission();
            sensorWorking = (error == 0);
          } else {
            sensorWorking = sgp.begin();
          }
          
          if (sensorWorking) {
            Serial.println("Sensor reinitialized OK");
            delay(10);
            failCount = 0;
          }
        }
      } else {
        // Reset fail counter on successful reading
        failCount = 0;
        
        // Print readings every 2 seconds to reduce serial traffic
        if (millis() - printInterval > 2000) {
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
        sensorWorking = sgp.begin();
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

// Function to detect which SGP sensor we're dealing with
String detectSensorType(uint8_t address) {
  byte error;
  String type = "Unknown";
  
  Serial.print("Attempting to identify sensor at address 0x");
  Serial.println(address, HEX);
  delay(50);
  
  // Try SGP30-specific command (Get Feature Set)
  Wire.beginTransmission(address);
  Wire.write(0x20); // Get Feature Set command
  Wire.write(0x2F);
  error = Wire.endTransmission();
  
  if (error == 0) {
    delay(10);
    if (Wire.requestFrom(address, 3) == 3) {
      byte data[3];
      for (int i = 0; i < 3; i++) {
        data[i] = Wire.read();
      }
      
      // SGP30 should return product type 0 in first byte
      if (data[0] == 0x00) {
        type = "SGP30";
        Serial.println("SGP30 identified by feature set response");
        return type;
      }
    }
  }
  
  // Try SGP40-specific command (Measure Raw Signal)
  Wire.beginTransmission(address);
  Wire.write(0x26); // Measure Raw Signal command
  Wire.write(0x0F);
  error = Wire.endTransmission();
  
  if (error == 0) {
    delay(30); // SGP40 needs time to process
    if (Wire.requestFrom(address, 3) == 3) {
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
    if (Wire.requestFrom(address, 3) == 3) {
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

// Function to convert relative humidity to absolute humidity
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
