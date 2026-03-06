// esp32_nrf24_jammer/esp32_nrf24_jammer.cpp
#include "esp32_nrf24_jammer.h"
#include <SPI.h>

#define SPI_FREQUENCY 10E6   // 10MHz clock speed (typical for nRF24L01)

// nRF24L01 register addresses (from datasheet)
#define REG_CONFIG      0x00
#define REG_EN_AA       0x01
#define REG_EN_RXADDR   0x02
#define REG_SETUP_AW    0x03
#define REG_SETUP_RETR  0x04
#define REG_RF_CH       0x05
#define REG_RF_SETUP    0x06
#define REG_STATUS      0x07
#define REG_OBSERVE_TX   0x08
#define REG_RPD          0x09
#define REG_RX_ADDR_P0   0x0A
#define REG_RX_ADDR_P1   0x0B
#define REG_RX_ADDR_P2   0x0C
#define REG_RX_ADDR_P3   0x0D
#define REG_RX_ADDR_P4   0x0E
#define REG_RX_ADDR_P5   0x0F
#define REG_TX_ADDR      0x10
#define REG_RX_PW_P0     0x11
#define REG_RX_PW_P1     0x12
#define REG_RX_PW_P2     0x13
#define REG_RX_PW_P3     0x14
#define REG_RX_PW_P4     0x15
#define REG_RX_PW_P5     0x16
#define REG_FIFO_STATUS  0x17

ESP32NRF24Jammer::ESP32NRF24Jammer(uint8_t ledPin, uint8_t csnPin, uint8_t cePin)
    : _ledPin(ledPin), _radio(cePin, csnPin, 12, 13, 11) {   // SCK=12, MISO=13, MOSI=11
}

bool ESP32NRF24Jammer::begin(void) {
    // Turn on onboard WS2812 LED green at full brightness
    neopixelWrite(_ledPin, 0, 255, 0);  // R, G, B

    if (_radio.begin()) {
        _radio.powerDown();
        
        // Initialize hardware SPI for optimal performance on ESP32
        SPI.begin(12, 13, 11, -1);   // SCK, MISO, MOSI, SS (-1 means use MOSI as DOUT)
        SPI.setFrequency(SPI_FREQUENCY);
        
        delay(200);
        
        _radio.reset();
        _setChannelInternal(_currentChannel);
        
        return true;
    }
    return false;
}

void ESP32NRF24Jammer::jammingOn(void) {
    if (!_isJamming && !_safeMode) {
        _configureAggressiveMode();
        _radio.powerUp();
        _setChannelInternal(_currentChannel);
        _isJamming = true;
        
        neopixelWrite(_ledPin, 255, 0, 0);  // Red indicates jamming active
    }
}

void ESP32NRF24Jammer::_configureAggressiveMode(void) {
    _radio.setMaxPower();      // 0dBm max output
    _radio.disableAutoAck();   // No retries, continuous TX
    _radio.setTxMode();
}

void ESP32NRF24Jammer::jammingOff(void) {
    if (_isJamming) {
        _radio.powerDown();
        _isJamming = false;
        neopixelWrite(_ledPin, 0, 255, 0);  // Green when idle
    }
}

bool ESP32NRF24Jammer::isJamming(void) {
    return _isJamming;
}

void ESP32NRF24Jammer::updateLED(void) {
    if (_isJamming) {
        neopixelWrite(_ledPin, 255, 0, 0);  // Red when jamming
    } else {
        neopixelWrite(_ledPin, 0, 255, 0);  // Green when idle
    }
}

void ESP32NRF24Jammer::setSafeMode(bool enable) {
    _safeMode = enable;
    
    if (_safeMode && _isJamming) {
        jammingOff();
    }
}

void ESP32NRF24Jammer::setFrequencyStep(uint32_t stepHz) {
    if (stepHz > 0 && stepHz <= 5000000) {   // Limit to reasonable range
        _frequencyStepHz = stepHz;
    }
}

void ESP32NRF24Jammer::setChannel(uint16_t channel) {
    if (channel <= 124) {   // Valid channel range for 2.4GHz ISM band
        _currentChannel = channel;
        
        if (_isJamming) {
            _setChannelInternal(_currentChannel);
        }
    }
}

void ESP32NRF24Jammer::_setChannelInternal(uint16_t ch) {
    uint8_t rfch = static_cast<uint8_t>(ch);

    _radio.writeRegister(REG_RF_CH, rfch);   // Write channel to RF_CH register

    _radio.pulseCE();
}

// Timed jamming implementation
void ESP32NRF24Jammer::startTimedJam(unsigned long durationMs) {
    _timedJamDuration = durationMs;
    _timedJamStart = millis();
    _timedJamActive = true;
    jammingOn();
    Serial.print("Timed jam started for ");
    Serial.print(durationMs / 1000);
    Serial.println(" seconds");
}

void ESP32NRF24Jammer::updateTimedJam(void) {
    if (_timedJamActive) {
        if (millis() - _timedJamStart >= _timedJamDuration) {
            jammingOff();
            _timedJamActive = false;
            Serial.println("Timed jam completed, resuming scan");
            startBackgroundScan();  // Resume scanning after jam
        } else {
            // Aggressive channel hopping during timed jam
            aggressiveJamBurst();
        }
    }
}

bool ESP32NRF24Jammer::isTimedJamActive(void) {
    return _timedJamActive;
}

unsigned long ESP32NRF24Jammer::getTimedJamRemaining(void) {
    if (!_timedJamActive) return 0;
    unsigned long elapsed = millis() - _timedJamStart;
    if (elapsed >= _timedJamDuration) return 0;
    return _timedJamDuration - elapsed;
}

// Aggressive jamming burst - fast channel hopping with actual TX
void ESP32NRF24Jammer::aggressiveJamBurst(void) {
    if (!_isJamming) return;
    
    // Noise payload - random-ish data to create interference
    static uint8_t noise[32] = {
        0xFF, 0x00, 0xAA, 0x55, 0xFF, 0x00, 0xAA, 0x55,
        0x0F, 0xF0, 0x0F, 0xF0, 0x33, 0xCC, 0x33, 0xCC,
        0x55, 0xAA, 0x55, 0xAA, 0xFF, 0xFF, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };
    
    // Rapidly hop through multiple channels and TX noise
    for (int burst = 0; burst < 20; burst++) {
        _currentChannel = (_currentChannel + 7) % 126;  // Prime step for coverage
        _radio.writeRegister(REG_RF_CH, _currentChannel);
        _radio.flushTx();
        _radio.writeTxPayload(noise, 32);
        _radio.transmit();
        
        // Rotate noise pattern for variety
        noise[0] ^= burst;
        noise[15] += burst;
    }
}

// Scanner implementation - background task wrapper
static void scanTaskWrapper(void* param) {
    ESP32NRF24Jammer* jammer = (ESP32NRF24Jammer*)param;
    jammer->_scanTask();
}

void ESP32NRF24Jammer::_scanTask(void) {
    while (_scanRunning) {
        if (!_isJamming) {
            // Blue LED while scanning
            neopixelWrite(_ledPin, 0, 0, 255);
            
            _radio.scanAllChannels(_scanResults, 5);  // 5 samples, fast
            _scanReady = true;
            
            // Green LED when done
            neopixelWrite(_ledPin, 0, 255, 0);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms between scans
    }
    vTaskDelete(nullptr);
}

void ESP32NRF24Jammer::startBackgroundScan(void) {
    if (_scanTaskHandle != nullptr) return;  // Already running
    
    _scanRunning = true;
    xTaskCreatePinnedToCore(
        scanTaskWrapper,
        "ScanTask",
        4096,
        this,
        1,  // Low priority
        &_scanTaskHandle,
        1   // Run on Core 1 (Arduino/app core, keeps WiFi on Core 0 clear)
    );
}

void ESP32NRF24Jammer::stopBackgroundScan(void) {
    _scanRunning = false;
    if (_scanTaskHandle != nullptr) {
        vTaskDelay(200 / portTICK_PERIOD_MS);  // Let task exit
        _scanTaskHandle = nullptr;
    }
}

void ESP32NRF24Jammer::getScanResults(uint8_t* results) {
    memcpy(results, _scanResults, 126);
}

bool ESP32NRF24Jammer::isScanReady(void) {
    return _scanReady;
}
