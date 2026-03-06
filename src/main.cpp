// main.cpp
#include <Arduino.h>
#include "esp32_nrf24_jammer/esp32_nrf24_jammer.h"
#include "esp32_nrf24_jammer/WiFiController.h"
#include <WiFi.h>
#include <SPI.h>

#define LED_PIN     48  // ESP32 onboard WS2812 LED
#define CSN_PIN     10  // Safe SPI CS pin
#define CE_PIN      9   // Safe CE pin
#define JAM_BUTTON  0   // GPIO0 = BOOT button on DevKitC (or wire external button)

// WiFi credentials - change these!
#define WIFI_SSID "nacho-wifi"
#define WIFI_PASSWORD "password123"

ESP32NRF24Jammer jammer(LED_PIN, CSN_PIN, CE_PIN);
WiFiController wifi(jammer);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n===== FIRMWARE v2 =====");  // Version marker
    
    // Setup JAM button with internal pull-up
    pinMode(JAM_BUTTON, INPUT_PULLUP);
    
    // Show startup LED color (cyan) immediately
    neopixelWrite(LED_PIN, 0, 100, 100);  // Cyan = waiting for init
    
    Serial.println("--- ESP32-S3 N16R8 RF Scanner/Jammer ---");
    
    // Print memory info
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);

    // Initialize WiFi Access Point first
    if (!wifi.begin(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("Failed to start WiFi AP!");
    }

    Serial.println("\n=== WiFi AP Started ===");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Password: ");
    Serial.println(WIFI_PASSWORD);
    Serial.print("IP Address: ");
    Serial.println(wifi.getIP());
    Serial.println("Connect from your phone and visit: http://192.168.4.1");
    
    // Give WiFi time to stabilize before initializing radio
    delay(500);
    
    // Initialize the NRF24 radio
    Serial.println("\nInitializing NRF24L01+ radio...");
    Serial.println("SPI Pins: SCK=12, MISO=13, MOSI=11, CSN=10, CE=9");
    if (!jammer.begin()) {
        Serial.println("ERROR: Failed to initialize radio! Check wiring.");
        neopixelWrite(LED_PIN, 255, 0, 0);  // Red = error
        // Continue anyway to allow WiFi access
    } else {
        Serial.println("Radio initialized OK");
        // Start background scanning
        jammer.startBackgroundScan();
        Serial.println("Background scanning started");
    }
    
    Serial.println("\nPhysical BOOT button toggles jamming ON/OFF");
    neopixelWrite(LED_PIN, 0, 255, 0);  // Green = ready
}

void loop() {
    static bool lastButtonState = HIGH;
    static unsigned long lastDebounce = 0;
    static unsigned long lastPrint = 0;

    // Handle WiFi requests
    wifi.handleClient();
    
    // Physical JAM button - toggle jam on/off
    bool buttonState = digitalRead(JAM_BUTTON);
    
    // Heartbeat debug every 10 seconds
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        Serial.printf("Heap: %d KB, Jamming: %s\n", 
            ESP.getFreeHeap() / 1024, jammer.isJamming() ? "ON" : "OFF");
    }
    
    if (buttonState == LOW && lastButtonState == HIGH && (millis() - lastDebounce > 250)) {
        lastDebounce = millis();
        Serial.println(">>> BUTTON PRESSED! <<<");
        if (jammer.isJamming()) {
            jammer.jammingOff();
            jammer.startBackgroundScan();
            Serial.println("Jamming OFF - scanning resumed");
            neopixelWrite(LED_PIN, 0, 255, 0);  // Green
        } else {
            jammer.stopBackgroundScan();
            jammer.jammingOn();
            Serial.println("Jamming ON");
            neopixelWrite(LED_PIN, 255, 0, 0);  // Red
        }
    }
    lastButtonState = buttonState;

    // Continuous jam burst while jamming
    if (jammer.isJamming()) {
        jammer.aggressiveJamBurst();
    }

    delay(1);  // Prevent watchdog
}
