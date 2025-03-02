#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <stdlib.h>

// WiFi connection details
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
  // Note: This won't work on ESP8266 directly, but we'll handle it in the code
  Serial.println("\nAttempting to use WiFi credentials from environment");
  
  // For ESP8266, we need to set these values before uploading
  // They should be set in your environment when compiling
  #ifdef WIFI_SSID_VALUE
    ssid = WIFI_SSID_VALUE;
  #endif
  
  #ifdef WIFI_PASSWORD_VALUE
    password = WIFI_PASSWORD_VALUE;
  #endif
  
  if (!ssid || !password) {
    Serial.println("[ERROR] WiFi credentials not found!");
    Serial.println("Make sure to define WIFI_SSID_VALUE and WIFI_PASSWORD_VALUE in build flags");
    printWiFiInfo();
    return;
  }

  // Scan for networks first
  Serial.println("\nScanning for WiFi networks...");
  int networksFound = WiFi.scanNetworks();
  Serial.print(networksFound);
  Serial.println(" networks found:");
  
  bool targetNetworkFound = false;
  for (int i = 0; i < networksFound; i++) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm) ");
    Serial.println(WiFi.encryptionType(i) == ENC_TYPE_NONE ? "Open" : "Encrypted");
    
    if (WiFi.SSID(i) == ssid) {
      targetNetworkFound = true;
    }
  }
  
  if (targetNetworkFound) {
    Serial.print("\nTarget network '");
    Serial.print(ssid);
    Serial.println("' was found in scan results!");
  } else {
    Serial.print("\nWARNING: Target network '");
    Serial.print(ssid);
    Serial.println("' was NOT found in scan results!");
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
