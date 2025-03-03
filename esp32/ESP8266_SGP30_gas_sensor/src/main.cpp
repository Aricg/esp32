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

// Variables to store sensor readings
uint16_t TVOC = 0;
uint16_t eCO2 = 0;
uint32_t getAbsoluteHumidity(float temperature, float humidity);
uint32_t lastMeasurement = 0;
uint32_t lastBaseline = 0;
uint16_t TVOC_base, eCO2_base;
bool baselineValid = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\nSGP30 Gas Sensor Test");

  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
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

  // Initialize SGP30 sensor
  if (!sgp.begin()) {
    Serial.println("SGP30 sensor not found. Check wiring!");
    while (1);
  }
  
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // Set up initial baseline after 12 hours
  Serial.println("Waiting for sensor to warm up...");
}

void loop() {
  // Measure every second
  if (millis() - lastMeasurement > 1000) {
    lastMeasurement = millis();
    
    // Measure TVOC and eCO2
    if (!sgp.IAQmeasure()) {
      Serial.println("Measurement failed");
      return;
    }
    
    // Read sensor values
    TVOC = sgp.TVOC;
    eCO2 = sgp.eCO2;
    
    // Print readings
    Serial.print("TVOC: "); Serial.print(TVOC); Serial.println(" ppb");
    Serial.print("eCO2: "); Serial.print(eCO2); Serial.println(" ppm");
    
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

// Function to convert relative humidity to absolute humidity
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}
