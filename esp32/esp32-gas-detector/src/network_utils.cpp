#include "network_utils.h"

NetworkUtils::NetworkUtils(const char* ssid, const char* password, const char* serverUrl)
    : _ssid(ssid), _password(password), _serverUrl(serverUrl), _wifiConnected(false) {}

bool NetworkUtils::connectToWiFi() {
    if (_wifiConnected) return true;

    WiFi.begin(_ssid, _password);
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("\nFailed to connect to WiFi");
    return false;
}

bool NetworkUtils::postSensorData(const char* sensorId, const char* data) {
    if (strlen(_serverUrl) == 0) {
        return false;  // Skip if server URL is not set
    }

    if (!_wifiConnected && !connectToWiFi()) {
        return false;
    }

    HTTPClient http;
    http.begin(_serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "sensor_id=" + String(sensorId) + "&data=" + String(data);
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        http.end();
        return true;
    } else {
        Serial.print("Error in HTTP request: ");
        Serial.println(httpResponseCode);
        http.end();
        return false;
    }
}
