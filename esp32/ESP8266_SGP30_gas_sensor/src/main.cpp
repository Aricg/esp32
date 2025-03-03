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
  delay(100); // Give serial port time to initialize
  Serial.println("\n\n--- SGP30 Gas Sensor Test ---");

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
  
  // Try direct I2C communication first
  Wire.beginTransmission(0x58); // SGP30 address
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.println("Direct I2C communication with SGP30 successful");
    delay(50);
  } else {
    Serial.print("Direct I2C communication failed with error: ");
    Serial.println(error);
    delay(50);
  }
  
  // Now try the Adafruit library initialization with multiple attempts
  bool sensorFound = false;
  for (int attempt = 1; attempt <= 3 && !sensorFound; attempt++) {
    Serial.print("SGP30 init attempt ");
    Serial.print(attempt);
    Serial.println("/3");
    delay(50);
    
    sensorFound = sgp.begin();
    
    if (sensorFound) {
      Serial.println("SGP30 sensor initialized successfully!");
      delay(50);
    } else {
      Serial.println("SGP30 init failed, retrying...");
      delay(1000);
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
  static uint32_t printInterval = 0;
  
  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    
    if (sensorWorking) {
      // Measure TVOC and eCO2
      if (!sgp.IAQmeasure()) {
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
          sensorWorking = sgp.begin();
          if (sensorWorking) {
            Serial.println("Sensor reinitialized OK");
            delay(10);
            failCount = 0;
          }
        }
      } else {
        // Reset fail counter on successful reading
        failCount = 0;
        
        // Read sensor values
        TVOC = sgp.TVOC;
        eCO2 = sgp.eCO2;
        
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

// Function to convert relative humidity to absolute humidity
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
