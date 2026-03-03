// main.cpp
#include <Arduino.h>
#include "esp32_nrf24_jammer/esp32_nrf24_jammer.h"
#include <SPI.h>

#define LED_PIN     48  // ESP32 onboard LED (commonly pin 48)
#define CSN_PIN     32
#define CE_PIN      38

ESP32NRF24Jammer jammer(LED_PIN, CSN_PIN, CE_PIN);

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- ESP32 nRF24L01 Jammer ---");

    if (!jammer.begin()) {
        Serial.println("Radio module not found or initialization failed!");
    } else {
        Serial.println("Radio initialized OK.");
    }

    Serial.println("\n=== Serial Commands ===");
    Serial.println("S - Toggle Jamming ON/OFF");
    Serial.println("C<number> - Set channel (0-124)");
    Serial.println("F<number> - Set frequency step (Hz)");
}

void loop() {
    static int channel = 0;

    // Keep LED color persistent
    jammer.updateLED();

    if (Serial.available()) {
        char c = Serial.read();

        switch(tolower(c)) {
            case 's':
                if (jammer.isJamming()) {
                    jammer.jammingOff();
                    Serial.println("Jamming OFF");
                } else {
                    jammer.jammingOn();
                    Serial.println("Jamming ON");
                }
                break;

            case 'c':   // Set channel (single frequency)
                if (Serial.available()) {
                    int ch = Serial.parseInt();
                    if (ch >= 0 && ch <= 124) {
                        jammer.setChannel(ch);
                        channel = ch;
                        Serial.print("Set channel to: ");
                        Serial.println(ch);
                    } else {
                        Serial.println("Invalid channel range! Must be 0-124");
                    }
                }
                break;

            case 'f':   // Set frequency step (for sweep)
                if (Serial.available()) {
                    int step = Serial.parseInt();
                    if (step > 0 && step <= 5000000) {
                        jammer.setFrequencyStep(step);
                        Serial.print("Set frequency step: ");
                        Serial.println(step);
                    } else {
                        Serial.println("Invalid step range! Must be 1-5000000");
                    }
                }
                break;

            default:
                Serial.println("\nCOMMANDS:");
                Serial.println("  S - Toggle Jamming ON/OFF");
                Serial.println("  C<number> - Set channel (0-124)");
                Serial.println("  F<number> - Set frequency step (1-5000000)");
                break;
        }
    }

    // --- AUTOMATIC SWEEP MODE (only while jamming) ---
    static unsigned long lastSweep = 0;

    if (jammer.isJamming() && millis() - lastSweep > 2) {   // Fast hop across channels
        lastSweep = millis();

        channel++;
        if (channel > 124) {
            channel = 0;
        }

        jammer.setChannel(channel);
    }

    delay(1);
}
