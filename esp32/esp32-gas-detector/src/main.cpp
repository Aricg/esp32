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
  MQ135.setA(605.18);    // Generic coefficient A
  MQ135.setB(-3.937);    // Generic coefficient B
  MQ135.setRL(1);        // Set load resistance to 1KΩ
  MQ135.init(); 
  
  Serial.print("Calibrating MQ135 sensor...");
  float calcR0 = MQ135.calibrate(3.6); // Calibrate in clean air
  Serial.print("Calibration complete! R0 = ");
  Serial.println(calcR0);
  
  MQ135.setRegressionMethod(1); // Use exponential regression
  
  Serial.println("MQ135 sensor initialized!");
  Serial.println("Waiting 5 seconds for sensor warm-up...");
  delay(5000);
  Serial.println("Sensor warm-up complete. Starting readings...");
}

void loop() {
  // Read sensor values
  MQ135.update();
  float concentration = MQ135.readSensor();
  int rawAnalog = analogRead(MQ135_PIN_AO);

  // Print readings
  Serial.println("-----------------------------");
  Serial.print("Raw Analog Value: "); Serial.println(rawAnalog);
  Serial.print("Gas Concentration: "); Serial.print(concentration); Serial.println(" ppm");

  // Wait 5 seconds before next reading
  delay(5000);
}
