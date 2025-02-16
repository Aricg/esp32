#include <Arduino.h>
#include <MQUnifiedsensor.h>

// Define MQ135 sensor pins
#define MQ135_PIN_AO 34
#define MQ135_PIN_DO 13

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
  MQ135.setR0(1); // Calibrate this value later

  Serial.println("MQ135 sensor initialized!");
}

void loop() {
  // Read sensor values
  MQ135.update();
  float ppm = MQ135.readSensor();

  // Print readings to serial monitor
  Serial.print("Gas Concentration: ");
  Serial.print(ppm);
  Serial.println(" ppm");

  // Wait 5 seconds before next reading
  delay(5000);
}
