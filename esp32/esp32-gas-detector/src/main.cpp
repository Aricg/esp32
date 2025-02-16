#include <Arduino.h>
#include <MQUnifiedsensor.h>

// Define MQ135 sensor pins
#define MQ135_PIN_AO 34
#define MQ135_PIN_DO 13

// Calibration constants
#define CALIBRATION_SAMPLE_TIMES 50
#define CALIBRATION_SAMPLE_INTERVAL 500
#define R0_CLEAN_AIR_FACTOR 3.6 // For MQ135 in clean air

// Create MQ135 sensor object
MQUnifiedsensor MQ135("ESP32", 3.3, 12, MQ135_PIN_AO);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("Serial connection established!");

  // Initialize MQ135 sensor
  MQ135.init();
  MQ135.setRegressionMethod(1); // Use exponential regression
  MQ135.setA(110.47); 
  MQ135.setB(-2.862); // These values are for CO2

  // Calibrate R0 value
  Serial.println("Calibrating MQ135 sensor...");
  float calcR0 = 0;
  for (int i = 1; i <= CALIBRATION_SAMPLE_TIMES; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(R0_CLEAN_AIR_FACTOR);
    Serial.print(".");
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  MQ135.setR0(calcR0 / CALIBRATION_SAMPLE_TIMES);
  Serial.println("\nCalibration complete!");

  // Print calibration results
  Serial.print("Calibrated R0 value: ");
  Serial.println(MQ135.getR0());
  Serial.println("MQ135 sensor initialized!");
  Serial.println("Waiting 2 minutes for sensor warm-up...");
  delay(120000); // Wait 2 minutes for sensor to warm up
  Serial.println("Sensor warm-up complete. Starting readings...");
}

void loop() {
  // Read sensor values
  MQ135.update();
  float ppm = MQ135.readSensor();
  float ratio = MQ135.getR0();

  // Print detailed readings
  Serial.println("-----------------------------");
  Serial.print("R0: "); Serial.println(ratio);
  Serial.print("Gas Concentration: "); Serial.print(ppm); Serial.println(" ppm");

  // Wait 5 seconds before next reading
  delay(5000);
}
