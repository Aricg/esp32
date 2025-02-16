#include <Wire.h>
#include <Adafruit_BME680.h>

// Create BME680 instance
Adafruit_BME680 bme;

void setup() {
  Serial.begin(115200);
  Serial.println("Serial test - if you see this, serial is working!");
  delay(2000); // Give extra time for serial to initialize
  Serial.println("Starting setup...");
  delay(1000);
  
  // Check if Serial is working
  if (!Serial) {
    Serial.println("Serial communication failed!");
    while (1);
  }
  Serial.println("Serial initialized");

  // Initialize I2C communication
  Serial.println("Initializing I2C...");
  Wire.begin(21, 22);  // SDA, SCL
  Serial.println("I2C initialized");

  // Scan I2C bus
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
      Serial.println(" !");
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found!");
  }

  // Initialize BME680
  Serial.println("Initializing BME680...");
  if (!bme.begin(0x77)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    Serial.println("Possible causes:");
    Serial.println("1. Incorrect I2C address (should be 0x77)");
    Serial.println("2. SDA/SCL pins not connected properly");
    Serial.println("3. 3.3V power not connected");
    Serial.println("4. GND not connected");
    Serial.println("5. SDO pin not connected to VCC");
    while (1);
  }
  Serial.println("BME680 initialized successfully");

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop() {
  unsigned long startTime = millis();
  
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading, possible causes:");
    Serial.println("1. Sensor not ready yet");
    Serial.println("2. Gas heater not functioning");
    Serial.println("3. I2C communication error");
    Serial.println("Retrying in 1 second...");
    delay(1000);
    return;
  }

  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  // Check if gas reading is valid
  if (bme.gas_resistance > 0) {
    Serial.print("Gas = ");
    Serial.print(bme.gas_resistance / 1000.0);
    Serial.println(" KOhms");
  } else {
    Serial.println("Gas reading invalid (heater may be off)");
  }

  // Calculate and display reading time
  unsigned long readTime = millis() - startTime;
  Serial.print("Reading took ");
  Serial.print(readTime);
  Serial.println(" ms");

  Serial.println();
  delay(2000); // Wait 2 seconds between readings
}
