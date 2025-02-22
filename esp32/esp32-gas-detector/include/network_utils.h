#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

class NetworkUtils {
public:
    NetworkUtils(const char* ssid, const char* password, const char* serverUrl);
    bool connectToWiFi();
    bool postSensorData(const char* sensorName, float sensorValue);
    
private:
    const char* _ssid;
    const char* _password;
    const char* _serverUrl;
    bool _wifiConnected;
};

#endif
