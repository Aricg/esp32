#include <Arduino.h>

// Define MQ135 sensor pin
#define MQ135_PIN_AO 34

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("Serial connection established!");

  // Initialize analog pin
  pinMode(MQ135_PIN_AO, INPUT);
  
  Serial.println("MQ135 sensor initialized!");
  Serial.println("Waiting 5 seconds for sensor warm-up...");
  delay(5000);
  Serial.println("Sensor warm-up complete. Starting readings...");
}

void loop() {
  // Read raw analog value
  int rawAnalog = analogRead(MQ135_PIN_AO);

  // Print reading
  Serial.println("-----------------------------");
  Serial.print("Raw Analog Value: "); Serial.println(rawAnalog);

  // Wait 5 seconds before next reading
  delay(5000);
}
