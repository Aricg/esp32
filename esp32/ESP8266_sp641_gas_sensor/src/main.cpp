#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

// WiFi connection details will be retrieved from environment variables
const char* ssid = NULL;
const char* password = NULL;

// Function to translate WiFi status codes to readable text
String getWiFiStatusString(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "Connected (WL_CONNECTED)";
    case WL_NO_SHIELD: return "No WiFi shield (WL_NO_SHIELD)";
    case WL_IDLE_STATUS: return "Idle (WL_IDLE_STATUS)";
    case WL_NO_SSID_AVAIL: return "No SSID available (WL_NO_SSID_AVAIL)";
    case WL_SCAN_COMPLETED: return "Scan completed (WL_SCAN_COMPLETED)";
    case WL_CONNECT_FAILED: return "Connection failed (WL_CONNECT_FAILED)";
    case WL_CONNECTION_LOST: return "Connection lost (WL_CONNECTION_LOST)";
    case WL_DISCONNECTED: return "Disconnected (WL_DISCONNECTED)";
    default: return "Unknown status (" + String(status) + ")";
  }
}

void printWiFiInfo() {
  Serial.println("\n--- WiFi Debug Information ---");
  Serial.print("Status: ");
  Serial.println(getWiFiStatusString(WiFi.status()));
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway IP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS IP: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
  
  Serial.print("WiFi mode: ");
  switch (WiFi.getMode()) {
    case WIFI_OFF: Serial.println("OFF"); break;
    case WIFI_STA: Serial.println("Station"); break;
    case WIFI_AP: Serial.println("Access Point"); break;
    case WIFI_AP_STA: Serial.println("AP+Station"); break;
    default: Serial.println("Unknown");
  }
  
  Serial.print("Using SSID: ");
  Serial.println(ssid ? ssid : "NULL");
  Serial.print("Password provided: ");
  Serial.println(password ? "Yes" : "No");
  Serial.println("-----------------------------");
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=================================");
  Serial.println("ESP8266 WiFi Connectivity Test");
  Serial.println("=================================");
  
  // Print ESP8266 information
  Serial.print("ESP8266 Chip ID: ");
  Serial.println(ESP.getChipId(), HEX);
  Serial.print("Flash Chip ID: ");
  Serial.println(ESP.getFlashChipId(), HEX);
  Serial.print("Flash Chip Size: ");
  Serial.println(ESP.getFlashChipSize());
  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  
  // Get WiFi credentials from environment variables
  ssid = getenv("WIFI_SSID");
  password = getenv("WIFI_PASSWORD");

  if (!ssid || !password) {
    Serial.println("\n[ERROR] WiFi credentials not found!");
    Serial.println("Make sure WIFI_SSID and WIFI_PASSWORD environment variables are set.");
    Serial.println("Note: Environment variables may not work as expected on ESP8266.");
    Serial.println("Consider hardcoding credentials for testing purposes.");
    
    // For testing, you can uncomment and use these lines instead:
    // ssid = "your_wifi_ssid";
    // password = "your_wifi_password";
    
    printWiFiInfo();
    return;
  }

  Serial.print("\nConnecting to WiFi network: ");
  Serial.println(ssid);

  // Disconnect if already connected
  WiFi.disconnect();
  delay(100);
  
  // Set WiFi mode explicitly
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("Attempting connection...");

  // Wait for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    if (attempts % 10 == 9) {
      Serial.println();
      Serial.print("Still trying (status: ");
      Serial.print(getWiFiStatusString(WiFi.status()));
      Serial.println(")");
    }
    attempts++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[SUCCESS] WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("[FAILED] Could not connect to WiFi!");
    Serial.print("Status: ");
    Serial.println(getWiFiStatusString(WiFi.status()));
  }
  
  printWiFiInfo();
}

void loop() {
  // Check WiFi connection status periodically
  static unsigned long lastCheck = 0;
  static int disconnectCount = 0;
  static int reconnectAttempts = 0;
  
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastCheck >= 10000) { // Check every 10 seconds
    lastCheck = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected to WiFi. Signal strength (RSSI): ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      
      // Reset counters if connected
      disconnectCount = 0;
      reconnectAttempts = 0;
    } else {
      disconnectCount++;
      Serial.print("WiFi disconnected! Status: ");
      Serial.println(getWiFiStatusString(WiFi.status()));
      Serial.print("Disconnect count: ");
      Serial.println(disconnectCount);
      
      // Try to reconnect if disconnected for more than 2 checks
      if (disconnectCount >= 2 && reconnectAttempts < 3) {
        reconnectAttempts++;
        Serial.print("Attempting to reconnect (attempt ");
        Serial.print(reconnectAttempts);
        Serial.println(")...");
        
        WiFi.disconnect();
        delay(1000);
        WiFi.mode(WIFI_STA);
        delay(100);
        WiFi.begin(ssid, password);
        
        // Wait briefly for connection
        int quickAttempts = 0;
        while (WiFi.status() != WL_CONNECTED && quickAttempts < 10) {
          delay(500);
          Serial.print(".");
          quickAttempts++;
        }
        
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Reconnected successfully!");
        } else {
          Serial.print("Reconnection failed. Status: ");
          Serial.println(getWiFiStatusString(WiFi.status()));
        }
        
        printWiFiInfo();
      }
    }
  }
  
  // Keep the loop running without blocking
  delay(100);
}
