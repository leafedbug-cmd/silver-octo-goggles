// main.cpp
#include <Arduino.h>
#include "esp32_nrf24_jammer/esp32_nrf24_jammer.h"
#define ENABLE_WIFI_AP 0
#if ENABLE_WIFI_AP
#include "esp32_nrf24_jammer/WiFiController.h"
#include <WiFi.h>
#endif
#include <SPI.h>

#define LED_PIN     48  // ESP32 onboard WS2812 LED
#define CSN_PIN     10  // Safe SPI CS pin
#define CE_PIN      9   // Safe CE pin
#define JAM_SWITCH_PIN 21   // External switch signal pin (kept free from nRF IRQ)
#define JAM_SWITCH_ACTIVE_LEVEL LOW  // LOW when switch is ON (NO->pin, COM->GND)

#if ENABLE_WIFI_AP
// WiFi credentials - change these!
#define WIFI_SSID "nacho-wifi"
#define WIFI_PASSWORD "password123"
#endif

ESP32NRF24Jammer jammer(LED_PIN, CSN_PIN, CE_PIN);
#if ENABLE_WIFI_AP
WiFiController wifi(jammer);
#endif
static bool gRadioReady = false;

// Apply a desired jammer state and keep LED state aligned.
static void setJamState(bool enable) {
    if (!gRadioReady) {
        Serial.println("Jamming request ignored: nRF24 handshake not complete");
        neopixelWrite(LED_PIN, 0, 0, 0);  // Off until radio handshake completes
        return;
    }

    if (enable) {
        if (!jammer.isJamming()) {
            jammer.jammingOn();
            if (jammer.isJamming()) {
                Serial.println("Jamming ON (switch)");
                neopixelWrite(LED_PIN, 255, 0, 0);  // Red
            } else {
                Serial.println("Jamming ON blocked");
                neopixelWrite(LED_PIN, 0, 255, 0);  // Green
            }
        }
    } else {
        if (jammer.isJamming()) {
            jammer.jammingOff();
            Serial.println("Jamming OFF (switch)");
            neopixelWrite(LED_PIN, 0, 255, 0);  // Green
        }
    }
}

void setup() {
    Serial.begin(921600);
    // Wait for USB-CDC to enumerate so serial monitor catches all output
    delay(2000);
    Serial.println("\n\n===== FIRMWARE v3 =====");  // Version marker
    
    // External 3-pin switch input: NO->GPIO21, COM->GND, NC unused.
    pinMode(JAM_SWITCH_PIN, INPUT_PULLUP);
    
    // Keep LED off during boot and handshake.
    neopixelWrite(LED_PIN, 0, 0, 0);
    
    Serial.println("--- ESP32-S3 N16R8 RF Scanner/Jammer ---");
    
    // Print memory info
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);

    // Initialize WiFi Access Point first (optional)
#if ENABLE_WIFI_AP
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
#else
    Serial.println("\nWiFi AP disabled for this build");
#endif
    Serial.printf("External switch pin: GPIO%d, active level: %s\n",
        JAM_SWITCH_PIN,
        (JAM_SWITCH_ACTIVE_LEVEL == LOW) ? "LOW" : "HIGH");
    
    // Short delay before initializing radio
    delay(500);
    
    // Initialize the NRF24 radio
    Serial.println("\nInitializing NRF24L01+ radio...");
    Serial.println("SPI Pins: SCK=12, MISO=13, MOSI=11, CSN=10, CE=9");
    gRadioReady = jammer.begin();
    if (!gRadioReady) {
        Serial.println("ERROR: Failed to initialize radio! Check wiring.");
        neopixelWrite(LED_PIN, 0, 0, 0);  // Stay off if handshake fails
        // Continue anyway to allow WiFi access
    } else {
        Serial.println("Radio initialized OK");
    }
    
    Serial.println("\nExternal switch controls jamming ON/OFF");
    if (gRadioReady) {
        neopixelWrite(LED_PIN, 0, 255, 0);  // Green = ready (handshake complete)
    }

    // Respect physical switch position at startup.
    bool switchOn = (digitalRead(JAM_SWITCH_PIN) == JAM_SWITCH_ACTIVE_LEVEL);
    if (gRadioReady) {
        setJamState(switchOn);
    }
}

void loop() {
    static bool rawSwitchOn = false;
    static bool lastRawSwitchOn = false;
    static bool debouncedSwitchOn = false;
    static unsigned long lastRawChange = 0;
    static unsigned long lastPrint = 0;

    // Handle WiFi requests when AP is enabled
#if ENABLE_WIFI_AP
    wifi.handleClient();
#endif
    
    // Physical JAM switch - maintain ON/OFF state
    bool switchState = digitalRead(JAM_SWITCH_PIN);
    rawSwitchOn = (switchState == JAM_SWITCH_ACTIVE_LEVEL);
    
    // Heartbeat debug every 30 seconds
    if (millis() - lastPrint > 30000) {
        lastPrint = millis();
        Serial.printf("Heap: %d KB, Jamming: %s\n", 
            ESP.getFreeHeap() / 1024, jammer.isJamming() ? "ON" : "OFF");
    }
    
    // Debounce and apply switch position.
    if (rawSwitchOn != lastRawSwitchOn) {
        lastRawSwitchOn = rawSwitchOn;
        lastRawChange = millis();
    }

    if ((millis() - lastRawChange) > 35 && rawSwitchOn != debouncedSwitchOn) {
        debouncedSwitchOn = rawSwitchOn;
        setJamState(debouncedSwitchOn);
    }

    // Continuous jam burst while jamming
    if (jammer.isJamming()) {
        jammer.aggressiveJamBurst();
    }

    delay(1);  // Prevent watchdog
}
