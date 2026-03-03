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
    : _ledPin(ledPin), _radio(cePin, csnPin, 40, 42, 41) {   // SCK=40, MISO=42, MOSI=41
}

bool ESP32NRF24Jammer::begin(void) {
    // Turn on onboard WS2812 LED green at full brightness
    neopixelWrite(_ledPin, 0, 255, 0);  // R, G, B

    if (_radio.begin()) {
        _radio.powerDown();
        
        // Initialize hardware SPI for optimal performance on ESP32
        SPI.begin(40, 42, 41, -1);   // SCK, MISO, MOSI, SS (-1 means use MOSI as DOUT)
        SPI.setFrequency(SPI_FREQUENCY);
        
        delay(200);
        
        _radio.reset();
        _radio.disableAutoAck();      // No ACK waiting
        _radio.setMaxPower();         // Max TX power, 1Mbps
        _setChannelInternal(_currentChannel);
        
        return true;
    }
    return false;
}

void ESP32NRF24Jammer::jammingOn(void) {
    if (!_isJamming && !_safeMode) {
        _radio.powerUp();
        _radio.disableAutoAck();
        _radio.setMaxPower();
        _radio.enableCarrier();              // Continuous carrier wave
        _setChannelInternal(_currentChannel);
        _radio.setCEHigh();                  // Hold CE high to transmit
        _isJamming = true;
        
        neopixelWrite(_ledPin, 255, 0, 0);  // Red indicates jamming active
    }
}

void ESP32NRF24Jammer::jammingOff(void) {
    if (_isJamming) {
        _radio.setCELow();
        _radio.disableCarrier();
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

    // Briefly drop CE to change channel, then re-assert
    _radio.setCELow();
    _radio.writeRegister(REG_RF_CH, rfch);   // Write channel to RF_CH register
    if (_isJamming) {
        _radio.setCEHigh();   // Resume carrier on new channel
    }
}
