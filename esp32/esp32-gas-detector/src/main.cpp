#include <Arduino.h>

// Define MQ135 sensor pin
#define MQ135_PIN_AO 34

// Graph parameters
const int GRAPH_WIDTH = 50;  // Width of the text-based graph
const int HISTORY_SIZE = 50; // Number of previous values to store
int valueHistory[HISTORY_SIZE]; // Array to store historical values
int historyIndex = 0;         // Current position in history array

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("Serial connection established!");

  // Initialize analog pin
  pinMode(MQ135_PIN_AO, INPUT);
  
  // Initialize history array
  memset(valueHistory, 0, sizeof(valueHistory));
  
  Serial.println("MQ135 sensor initialized!");
  Serial.println("Waiting 5 seconds for sensor warm-up...");
  delay(5000);
  Serial.println("Sensor warm-up complete. Starting readings...");
}

void loop() {
  // Read raw analog value
  int rawAnalog = analogRead(MQ135_PIN_AO);

  // Store value in history
  valueHistory[historyIndex] = rawAnalog;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;

  // Print reading and graph
  Serial.println("-----------------------------");
  Serial.print("Raw Analog Value: "); Serial.println(rawAnalog);
  printTrendGraph();

  // Wait 5 seconds before next reading
  delay(5000);
}

void printTrendGraph() {
  // Find min and max values in history
  int minVal = 4096, maxVal = 0;
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (valueHistory[i] < minVal) minVal = valueHistory[i];
    if (valueHistory[i] > maxVal) maxVal = valueHistory[i];
  }

  // Print graph
  Serial.println("Trend Graph:");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    int barLength = map(valueHistory[i], minVal, maxVal, 0, GRAPH_WIDTH);
    for (int j = 0; j < barLength; j++) {
      Serial.print("#");
    }
    Serial.println();
  }
}
