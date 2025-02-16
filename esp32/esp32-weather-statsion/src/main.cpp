#include <Wire.h>
#include <Adafruit_BME680.h>

// Create BME680 instance
Adafruit_BME680 bme;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Serial initialized");

  // Initialize I2C communication
  Wire.begin(21, 22);  // SDA, SCL
  Serial.println("I2C initialized");

  // Initialize BME680
  Serial.println("Initializing BME680...");
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    Serial.println("Possible causes:");
    Serial.println("1. Incorrect I2C address (should be 0x76)");
    Serial.println("2. SDA/SCL pins not connected properly");
    Serial.println("3. 3.3V power not connected");
    Serial.println("4. GND not connected");
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
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading :(");
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

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.println();
  delay(2000); // Wait 2 seconds between readings
}
