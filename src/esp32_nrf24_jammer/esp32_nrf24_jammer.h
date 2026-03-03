// esp32_nrf24_jammer/esp32_nrf24_jammer.h
#ifndef ESP32_NRF24_JAMMER_H_
#define ESP32_NRF24_JAMMER_H_

#include <Arduino.h>
#include "NRF24L01.h"

class ESP32NRF24Jammer {
public:
    // Constructor using standard ESP32 pins
    ESP32NRF24Jammer(uint8_t ledPin = 4, 
                     uint8_t csnPin = 32, 
                     uint8_t cePin = 38);

    bool begin(void);
    
    void setSafeMode(bool enable);   // Disable RF writing to prevent accidental damage
    void jammingOn(void);
    void jammingOff(void);
    bool isJamming(void);
    void updateLED(void);   // Refresh LED display
    
    void setFrequencyStep(uint32_t stepHz);   // For continuous sweep (default 500kHz)
    void setChannel(uint16_t channel);      // Set single channel (0-124)

private:
    uint8_t _ledPin;
    NRF24L01 _radio;
    
    bool _safeMode = false;   // Default off
    bool _isJamming = false;

    uint16_t _currentChannel = 0;           // Current channel in sweep
    uint32_t _frequencyStepHz = 500000;      // Default step: 500kHz
    
    void _setChannelInternal(uint16_t ch);
};
#endif  // ESP32_NRF24_JAMMER_H_
