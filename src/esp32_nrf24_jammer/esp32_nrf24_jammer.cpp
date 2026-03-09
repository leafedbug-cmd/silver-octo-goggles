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

// Dump key nRF24 registers to Serial for diagnostics
void ESP32NRF24Jammer::_printRegisters(const char* label) {
    Serial.printf("[nRF24 %s] CONFIG=0x%02X  EN_AA=0x%02X  RF_CH=0x%02X  RF_SETUP=0x%02X  STATUS=0x%02X  FIFO=0x%02X  OBSERVE_TX=0x%02X\n",
        label,
        _radio.readRegister(REG_CONFIG),
        _radio.readRegister(REG_EN_AA),
        _radio.readRegister(REG_RF_CH),
        _radio.readRegister(REG_RF_SETUP),
        _radio.readRegister(REG_STATUS),
        _radio.readRegister(REG_FIFO_STATUS),
        _radio.readRegister(REG_OBSERVE_TX));
}

bool ESP32NRF24Jammer::begin(void) {
    Serial.println("[nRF24] begin() starting...");
    neopixelWrite(_ledPin, 0, 255, 0);

    if (_radio.begin()) {
        Serial.println("[nRF24] SPI contact OK - module responded");
        _radio.powerDown();
        
        SPI.begin(12, 13, 11, -1);
        SPI.setFrequency(SPI_FREQUENCY);
        
        delay(200);
        
        _radio.reset();
        _setChannelInternal(_currentChannel);
        
        _printRegisters("INIT");
        Serial.println("[nRF24] Initialization complete");
        return true;
    }
    Serial.println("[nRF24] ERROR: SPI contact FAILED - module not found!");
    Serial.println("[nRF24] Check: VCC=3.3V, GND, CSN=GPIO10, CE=GPIO9, SCK=12, MISO=13, MOSI=11");
    return false;
}

void ESP32NRF24Jammer::jammingOn(void) {
    if (!_isJamming && !_safeMode) {
        Serial.println("[nRF24] jammingOn() - configuring aggressive TX mode");
        _configureAggressiveMode();
        _radio.powerUp();
        _setChannelInternal(_currentChannel);
        _isJamming = true;
        _printRegisters("JAM-ON");
        neopixelWrite(_ledPin, 255, 0, 0);
    } else if (_safeMode) {
        Serial.println("[nRF24] jammingOn() BLOCKED - safe mode is enabled");
    }
}

void ESP32NRF24Jammer::_configureAggressiveMode(void) {
    _radio.setMaxPower();
    _radio.disableAutoAck();
    _radio.setTxMode();
    Serial.printf("[nRF24] Aggressive mode: RF_SETUP=0x%02X  EN_AA=0x%02X  CONFIG=0x%02X\n",
        _radio.readRegister(REG_RF_SETUP),
        _radio.readRegister(REG_EN_AA),
        _radio.readRegister(REG_CONFIG));
}

void ESP32NRF24Jammer::jammingOff(void) {
    if (_isJamming) {
        _radio.powerDown();
        _isJamming = false;
        _printRegisters("JAM-OFF");
        Serial.println("[nRF24] Jamming stopped, radio powered down");
        neopixelWrite(_ledPin, 0, 255, 0);
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

    static uint8_t noise[32] = {
        0xFF, 0x00, 0xAA, 0x55, 0xFF, 0x00, 0xAA, 0x55,
        0x0F, 0xF0, 0x0F, 0xF0, 0x33, 0xCC, 0x33, 0xCC,
        0x55, 0xAA, 0x55, 0xAA, 0xFF, 0xFF, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };

    static unsigned long lastBurstLog = 0;
    static uint32_t burstCount = 0;

    for (int burst = 0; burst < 100; burst++) {
        _currentChannel = (_currentChannel + 3) % 126;
        _radio.writeRegister(REG_RF_CH, _currentChannel);
        _radio.flushTx();
        _radio.writeTxPayload(noise, 32);
        _radio.transmit();

        noise[0] ^= burst;
        noise[15] += burst;
    }
    burstCount++;

    // Log TX health every 5 seconds while jamming
    if (millis() - lastBurstLog > 5000) {
        lastBurstLog = millis();
        uint8_t status = _radio.readRegister(REG_STATUS);
        uint8_t fifo   = _radio.readRegister(REG_FIFO_STATUS);
        uint8_t observe = _radio.readRegister(REG_OBSERVE_TX);
        uint8_t config = _radio.readRegister(REG_CONFIG);
        Serial.printf("[nRF24 TX] bursts=%u  STATUS=0x%02X  FIFO=0x%02X  OBSERVE=0x%02X  CONFIG=0x%02X  ch=%d\n",
            burstCount, status, fifo, observe, config, _currentChannel);
        // Check for common problems
        if (!(config & 0x02)) {
            Serial.println("[nRF24 TX] WARNING: PWR_UP bit is 0 - radio may have reset!");
        }
        if (status & 0x10) {
            Serial.println("[nRF24 TX] WARNING: MAX_RT flag set - TX FIFO not draining");
            _radio.writeRegister(REG_STATUS, 0x10);  // Clear flag
        }
        if (fifo & 0x10) {
            Serial.println("[nRF24 TX] INFO: TX FIFO empty (normal after flush)");
        }
    }
}

// Scanner implementation - background task wrapper
static void scanTaskWrapper(void* param) {
    ESP32NRF24Jammer* jammer = (ESP32NRF24Jammer*)param;
    jammer->_scanTask();
}

void ESP32NRF24Jammer::_scanTask(void) {
    static uint32_t scanCount = 0;
    Serial.println("[nRF24 SCAN] Background scan task started");
    while (_scanRunning) {
        if (!_isJamming) {
            neopixelWrite(_ledPin, 0, 0, 255);
            
            _radio.scanAllChannels(_scanResults, 5);
            _scanReady = true;
            scanCount++;
            
            neopixelWrite(_ledPin, 0, 255, 0);

            // Log scan health every 30 scans (~3 sec)
            if (scanCount % 30 == 0) {
                int activeChannels = 0;
                int maxSignal = 0;
                for (int i = 0; i < 126; i++) {
                    if (_scanResults[i] > 0) activeChannels++;
                    if (_scanResults[i] > maxSignal) maxSignal = _scanResults[i];
                }
                Serial.printf("[nRF24 SCAN] #%u  active_ch=%d  peak=%d%%  STATUS=0x%02X  CONFIG=0x%02X\n",
                    scanCount, activeChannels, maxSignal,
                    _radio.readRegister(REG_STATUS),
                    _radio.readRegister(REG_CONFIG));
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    Serial.println("[nRF24 SCAN] Background scan task exiting");
    vTaskDelete(nullptr);
}

void ESP32NRF24Jammer::startBackgroundScan(void) {
    if (_scanTaskHandle != nullptr) {
        Serial.println("[nRF24 SCAN] Already running, skipping start");
        return;
    }
    
    Serial.println("[nRF24 SCAN] Starting background scan task on Core 1");
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
    Serial.println("[nRF24 SCAN] Stopping background scan task");
    _scanRunning = false;
    if (_scanTaskHandle != nullptr) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
        _scanTaskHandle = nullptr;
    }
}

void ESP32NRF24Jammer::getScanResults(uint8_t* results) {
    memcpy(results, _scanResults, 126);
}

bool ESP32NRF24Jammer::isScanReady(void) {
    return _scanReady;
}
