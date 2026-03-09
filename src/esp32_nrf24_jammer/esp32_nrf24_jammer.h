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
    
    // Timed jamming
    void startTimedJam(unsigned long durationMs);  // Jam for specified duration
    void updateTimedJam(void);  // Call in loop to check timer
    bool isTimedJamActive(void);
    unsigned long getTimedJamRemaining(void);  // ms remaining
    
    // Aggressive jamming
    void aggressiveJamBurst(void);  // Fast channel hopping burst
    
    // Background scanner
    void startBackgroundScan(void);  // Start continuous background scanning
    void stopBackgroundScan(void);   // Stop scanning (for jamming)
    void getScanResults(uint8_t* results);  // Get latest cached scan results
    bool isScanReady(void);  // Check if first scan complete
    
    void setFrequencyStep(uint32_t stepHz);   // For continuous sweep (default 500kHz)
    void setChannel(uint16_t channel);      // Set single channel (0-124)

    // Internal - called by task
    void _scanTask(void);

private:
    uint8_t _ledPin;
    NRF24L01 _radio;
    
    bool _safeMode = false;   // Default off
    bool _isJamming = false;

    uint16_t _currentChannel = 0;           // Current channel in sweep
    uint32_t _frequencyStepHz = 500000;      // Default step: 500kHz
    
    // Timed jam state
    bool _timedJamActive = false;
    unsigned long _timedJamStart = 0;
    unsigned long _timedJamDuration = 0;
    
    // Background scan state
    uint8_t _scanResults[126];
    volatile bool _scanRunning = false;
    volatile bool _scanReady = false;
    TaskHandle_t _scanTaskHandle = nullptr;
    
    void _setChannelInternal(uint16_t ch);
    void _configureAggressiveMode(void);
    void _printRegisters(const char* label);  // Dump key registers to Serial
};
#endif  // ESP32_NRF24_JAMMER_H_
