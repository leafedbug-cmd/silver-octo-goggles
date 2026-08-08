// esp32_nrf24_jammer/esp32_nrf24_jammer.cpp
#include "esp32_nrf24_jammer.h"

// nRF24L01 register addresses (from datasheet)
#define REG_CONFIG       0x00
#define REG_EN_AA        0x01
#define REG_EN_RXADDR    0x02
#define REG_SETUP_AW     0x03
#define REG_SETUP_RETR   0x04
#define REG_RF_CH        0x05
#define REG_RF_SETUP     0x06
#define REG_STATUS       0x07
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

// Channel hopping mode for aggressive burst.
// 0 = all channels (0-125), 1 = even channels only, 2 = odd channels only
#define JAM_CHANNEL_MODE 2
// 1 = print every hopped channel to Serial, 0 = periodic TX health only
#define JAM_LOG_EVERY_CHANNEL 0

namespace {
constexpr int kScanSamplesPerChannel = 16;
constexpr uint8_t kScanPeakHoldDecay = 4;
constexpr uint8_t kRadioInitAttempts = 3;
constexpr int kRadioInitRetryDelayMs = 40;
}

// Read internal ESP32 temperature in deci-Celsius for compact payload encoding.
static int16_t readChipTempC10() {
    float tC = temperatureRead();
    return static_cast<int16_t>(tC * 10.0f);
}

static uint8_t nextJamChannel(uint8_t current) {
#if JAM_CHANNEL_MODE == 1
    uint8_t ch = current;
    if (ch & 0x01) {
        ch = static_cast<uint8_t>(ch - 1);
    }
    ch = static_cast<uint8_t>(ch + 2);
    if (ch >= 126) {
        ch = 0;
    }
    return ch;
#elif JAM_CHANNEL_MODE == 2
    uint8_t ch = current;
    if ((ch & 0x01) == 0) {
        ch = static_cast<uint8_t>(ch + 1);
    }
    ch = static_cast<uint8_t>(ch + 2);
    if (ch >= 126) {
        ch = 1;
    }
    return ch;
#else
    return static_cast<uint8_t>((current + 3) % 126);
#endif
}

ESP32NRF24Jammer::ESP32NRF24Jammer(int8_t ledPin, const NRF24RadioConfig& radioConfig)
    : _ledPin(ledPin),
      _radioConfig(radioConfig),
      _radio(*radioConfig.spi, radioConfig.cePin, radioConfig.csnPin, radioConfig.sckPin, radioConfig.misoPin, radioConfig.mosiPin) {
}

void ESP32NRF24Jammer::_setLedColor(uint8_t red, uint8_t green, uint8_t blue) {
    if (_ledPin >= 0) {
        neopixelWrite(_ledPin, red, green, blue);
    }
}

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

    if (_radioConfig.spi == nullptr) {
        Serial.println("[nRF24] ERROR: SPI bus was not configured");
        _radioReady = false;
        return false;
    }

    for (uint8_t attempt = 1; attempt <= kRadioInitAttempts; ++attempt) {
        if (_radio.begin()) {
            Serial.printf("[nRF24] SPI contact OK - module responded on attempt %u\n", attempt);
            _radio.powerDown();

            delay(200);

            _radio.reset();
            _setChannelInternal(_currentChannel);

            _radioReady = true;
            _printRegisters("INIT");
            Serial.println("[nRF24] Initialization complete");
            return true;
        }

        Serial.printf("[nRF24] SPI contact attempt %u/%u failed\n", attempt, kRadioInitAttempts);
        delay(kRadioInitRetryDelayMs);
    }

    Serial.println("[nRF24] ERROR: SPI contact FAILED - module not found!");
    Serial.printf("[nRF24] Check: VCC=3.3V, GND, CSN=GPIO%u, CE=GPIO%u, SCK=%u, MISO=%u, MOSI=%u\n",
        _radioConfig.csnPin, _radioConfig.cePin, _radioConfig.sckPin, _radioConfig.misoPin, _radioConfig.mosiPin);
    _radioReady = false;
    return false;
}

bool ESP32NRF24Jammer::isRadioReady(void) const {
    return _radioReady;
}

void ESP32NRF24Jammer::setRadioConfig(const NRF24RadioConfig& radioConfig) {
    _radioConfig = radioConfig;
    if (_radioConfig.spi != nullptr) {
        _radio.configure(*_radioConfig.spi, _radioConfig.cePin, _radioConfig.csnPin, _radioConfig.sckPin, _radioConfig.misoPin, _radioConfig.mosiPin);
    }
    _radioReady = false;
}

void ESP32NRF24Jammer::jammingOn(void) {
    if (!_radioReady) {
        Serial.println("[nRF24] jammingOn() BLOCKED - radio not ready");
        return;
    }

    if (!_isJamming && !_safeMode) {
        Serial.println("[nRF24] jammingOn() - configuring aggressive TX mode");
        _configureAggressiveMode();
        _radio.powerUp();
        _setChannelInternal(_currentChannel);
        _isJamming = true;
        _printRegisters("JAM-ON");
        _setLedColor(255, 0, 0);
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
        _setLedColor(0, 255, 0);
    }
}

bool ESP32NRF24Jammer::isJamming(void) {
    return _isJamming;
}

void ESP32NRF24Jammer::updateLED(void) {
    if (_isJamming) {
        _setLedColor(255, 0, 0);
    } else {
        _setLedColor(0, 255, 0);
    }
}

void ESP32NRF24Jammer::setSafeMode(bool enable) {
    _safeMode = enable;

    if (_safeMode && _isJamming) {
        jammingOff();
    }
}

void ESP32NRF24Jammer::setFrequencyStep(uint32_t stepHz) {
    if (stepHz > 0 && stepHz <= 5000000) {
        _frequencyStepHz = stepHz;
    }
}

void ESP32NRF24Jammer::setChannel(uint16_t channel) {
    if (channel <= 124) {
        _currentChannel = channel;

        if (_isJamming) {
            _setChannelInternal(_currentChannel);
        }
    }
}

void ESP32NRF24Jammer::_setChannelInternal(uint16_t ch) {
    uint8_t rfch = static_cast<uint8_t>(ch);
    _radio.writeRegister(REG_RF_CH, rfch);
    _radio.pulseCE();
}

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
            Serial.println("Timed jam completed");
        } else {
            aggressiveJamBurst();
        }
    }
}

bool ESP32NRF24Jammer::isTimedJamActive(void) {
    return _timedJamActive;
}

unsigned long ESP32NRF24Jammer::getTimedJamRemaining(void) {
    if (!_timedJamActive) {
        return 0;
    }
    unsigned long elapsed = millis() - _timedJamStart;
    if (elapsed >= _timedJamDuration) {
        return 0;
    }
    return _timedJamDuration - elapsed;
}

void ESP32NRF24Jammer::aggressiveJamBurst(void) {
    if (!_isJamming) {
        return;
    }

    static uint8_t noise[32] = {
        0xFF, 0x00, 0xAA, 0x55, 0xFF, 0x00, 0xAA, 0x55,
        0x0F, 0xF0, 0x0F, 0xF0, 0x33, 0xCC, 0x33, 0xCC,
        0x55, 0xAA, 0x55, 0xAA, 0xFF, 0xFF, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };

    static unsigned long lastBurstLog = 0;
    static uint32_t burstCount = 0;
    int16_t tempC10 = readChipTempC10();

    for (int burst = 0; burst < 100; burst++) {
        _currentChannel = nextJamChannel(_currentChannel);
        _radio.writeRegister(REG_RF_CH, _currentChannel);
#if JAM_LOG_EVERY_CHANNEL
        Serial.printf("[nRF24 HOP] ch=%u\n", _currentChannel);
#endif
        noise[28] = static_cast<uint8_t>((tempC10 >> 8) & 0xFF);
        noise[29] = static_cast<uint8_t>(tempC10 & 0xFF);
        _radio.flushTx();
        _radio.writeTxPayload(noise, 32);
        _radio.transmit();

        noise[0] ^= burst;
        noise[15] += burst;
    }
    burstCount++;

    if (millis() - lastBurstLog > 10000) {
        lastBurstLog = millis();
        uint8_t status = _radio.readRegister(REG_STATUS);
        uint8_t fifo = _radio.readRegister(REG_FIFO_STATUS);
        uint8_t observe = _radio.readRegister(REG_OBSERVE_TX);
        uint8_t config = _radio.readRegister(REG_CONFIG);
        Serial.printf("[nRF24 TX] bursts=%u  STATUS=0x%02X  FIFO=0x%02X  OBSERVE=0x%02X  CONFIG=0x%02X  ch=%d  temp=%.1fC\n",
            burstCount, status, fifo, observe, config, _currentChannel, tempC10 / 10.0f);
        if (!(config & 0x02)) {
            Serial.println("[nRF24 TX] WARNING: PWR_UP bit is 0 - radio may have reset!");
        }
        if (status & 0x10) {
            Serial.println("[nRF24 TX] WARNING: MAX_RT flag set - TX FIFO not draining");
            _radio.writeRegister(REG_STATUS, 0x10);
        }
        if (fifo & 0x10) {
            Serial.println("[nRF24 TX] INFO: TX FIFO empty (normal after flush)");
        }
    }
}

static void scanTaskWrapper(void* param) {
    ESP32NRF24Jammer* jammer = (ESP32NRF24Jammer*)param;
    jammer->_scanTask();
}

void ESP32NRF24Jammer::_scanTask(void) {
    static uint32_t scanCount = 0;
    uint8_t localResults[126];
    Serial.println("[nRF24 SCAN] Background scan task started");
    while (_scanRunning) {
        if (!_isJamming) {
            _setLedColor(0, 0, 255);

            _radio.scanAllChannels(localResults, kScanSamplesPerChannel);
            portENTER_CRITICAL(&_stateMux);
            for (size_t channel = 0; channel < sizeof(_scanResults); ++channel) {
                const uint8_t previous = _scanResults[channel];
                const uint8_t current = localResults[channel];
                if (current >= previous) {
                    _scanResults[channel] = current;
                } else {
                    const uint8_t decayed = previous > kScanPeakHoldDecay ? previous - kScanPeakHoldDecay : 0;
                    _scanResults[channel] = max(current, decayed);
                }
            }
            _scanReady = true;
            portEXIT_CRITICAL(&_stateMux);
            scanCount++;

            _setLedColor(0, 255, 0);

            if (scanCount % 30 == 0) {
                int activeChannels = 0;
                int maxSignal = 0;
                for (uint8_t value : localResults) {
                    if (value > 0) {
                        activeChannels++;
                    }
                    if (value > maxSignal) {
                        maxSignal = value;
                    }
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
    if (!_radioReady) {
        Serial.println("[nRF24 SCAN] Radio not ready, scan task not started");
        return;
    }

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
        1,
        &_scanTaskHandle,
        1);
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
    portENTER_CRITICAL(&_stateMux);
    memcpy(results, _scanResults, 126);
    portEXIT_CRITICAL(&_stateMux);
}

bool ESP32NRF24Jammer::isScanReady(void) {
    return _scanReady;
}

void ESP32NRF24Jammer::getStateSnapshot(JammerStateSnapshot& snapshot) {
    portENTER_CRITICAL(&_stateMux);
    snapshot.radioReady = _radioReady;
    snapshot.jamming = _isJamming;
    snapshot.scanReady = _scanReady;
    memcpy(snapshot.scanResults, _scanResults, sizeof(snapshot.scanResults));
    portEXIT_CRITICAL(&_stateMux);
}
