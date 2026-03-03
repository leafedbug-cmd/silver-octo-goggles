#ifndef WIFI_CONTROLLER_H_
#define WIFI_CONTROLLER_H_

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp32_nrf24_jammer.h"

class WiFiController {
public:
    WiFiController(ESP32NRF24Jammer& jammer);
    
    bool begin(const char* ssid, const char* password);
    void handleClient();
    
    const char* getSSID() const { return _ssid; }
    const char* getIP() const { return _ipAddr; }
    bool isRunning() const { return _running; }

private:
    ESP32NRF24Jammer& _jammer;
    WebServer _server;
    const char* _ssid;
    const char* _password;
    char _ipAddr[16];
    bool _running;
    
    void setupRoutes();
    void handleRoot();
    void handleJammingStatus();
    void handleJammingToggle();
    void handleSetChannel();
    void handleSetFrequencyStep();
    void handleNotFound();
    
    String generateHTML();
};

#endif  // WIFI_CONTROLLER_H_
