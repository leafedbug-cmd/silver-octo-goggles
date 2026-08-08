#include <Arduino.h>
#include <SPI.h>

#include "esp32_nrf24_jammer/WaveshareTouchUI.h"
#include "esp32_nrf24_jammer/esp32_nrf24_jammer.h"

namespace {
constexpr int8_t kLedPin = -1;
constexpr uint8_t kRadioCePin = 38;
constexpr uint8_t kRadioCsnPin = 39;
constexpr uint8_t kRadioSckPin = 40;
constexpr uint8_t kRadioMosiPin = 41;
constexpr uint8_t kRadioMisoPin = 42;

SPIClass gRadioSpi(HSPI);

struct RadioProbeCandidate {
    uint8_t cePin;
    uint8_t csnPin;
    uint8_t sckPin;
    uint8_t mosiPin;
    uint8_t misoPin;
};

NRF24RadioConfig makeRadioConfig(uint8_t cePin, uint8_t csnPin, uint8_t sckPin, uint8_t mosiPin, uint8_t misoPin) {
    NRF24RadioConfig config;
    config.spi = &gRadioSpi;
    config.cePin = cePin;
    config.csnPin = csnPin;
    config.sckPin = sckPin;
    config.misoPin = misoPin;
    config.mosiPin = mosiPin;
    return config;
}

NRF24RadioConfig gRadioConfig = makeRadioConfig(kRadioCePin, kRadioCsnPin, kRadioSckPin, kRadioMosiPin, kRadioMisoPin);

ESP32NRF24Jammer jammer(kLedPin, gRadioConfig);
WaveshareTouchUI ui;
}  // namespace

void setup() {
    Serial.begin(921600);
    delay(2000);

    Serial.println("\n\n===== WAVESHARE TOUCH ANALYZER =====");
    Serial.println("--- ESP32-S3 Touch LCD 3.5 Spectrum Analyzer ---");

    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("PSRAM Size: %d KB\n", ESP.getPsramSize() / 1024);
    Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);

    jammer.setSafeMode(true);
    Serial.println("[APP] TX functions disabled - analyzer mode only");

    if (!ui.begin()) {
        Serial.println("[APP] UI init failed");
    }

    Serial.println("\nProbing single nRF24L01+ radio...");
    const RadioProbeCandidate candidates[] = {
        {38, 39, 40, 41, 42},
        {38, 39, 40, 42, 41},
        {39, 38, 40, 41, 42},
        {39, 38, 40, 42, 41},
    };

    bool radioReady = false;
    for (const RadioProbeCandidate& candidate : candidates) {
        gRadioConfig = makeRadioConfig(candidate.cePin, candidate.csnPin, candidate.sckPin, candidate.mosiPin, candidate.misoPin);
        jammer.setRadioConfig(gRadioConfig);
        Serial.printf("[APP] Trying radio pins: CE=%u CSN=%u SCK=%u MISO=%u MOSI=%u\n",
            candidate.cePin, candidate.csnPin, candidate.sckPin, candidate.misoPin, candidate.mosiPin);

        if (jammer.begin()) {
            Serial.println("[APP] Radio initialized OK");
            radioReady = true;
            break;
        }
    }

    if (radioReady) {
        jammer.startBackgroundScan();
    } else {
        Serial.println("[APP] Radio init failed on all candidate pin maps - UI will stay in offline mode");
    }
}

void loop() {
    static unsigned long lastHeartbeatMs = 0;

    JammerStateSnapshot snapshot;
    jammer.getStateSnapshot(snapshot);
    ui.update(snapshot);

    if ((millis() - lastHeartbeatMs) > 30000) {
        lastHeartbeatMs = millis();
        Serial.printf("[APP] Heap=%d KB Radio=%s Analyzer=%s ScanReady=%s\n",
            ESP.getFreeHeap() / 1024,
            jammer.isRadioReady() ? "READY" : "OFFLINE",
            "ON",
            jammer.isScanReady() ? "YES" : "NO");
    }

    delay(1);
}
