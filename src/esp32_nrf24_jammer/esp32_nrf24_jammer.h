// esp32_nrf24_jammer/esp32_nrf24_jammer.h
#ifndef ESP32_NRF24_JAMMER_H_
#define ESP32_NRF24_JAMMER_H_

#include <Arduino.h>
#include <SPI.h>
#include "NRF24L01.h"

struct NRF24RadioConfig {
    SPIClass* spi = nullptr;
    uint8_t cePin = 0;
    uint8_t csnPin = 0;
    uint8_t sckPin = 0;
    uint8_t misoPin = 0;
    uint8_t mosiPin = 0;
};

struct JammerStateSnapshot {
    bool radioReady = false;
    bool jamming = false;
    bool scanReady = false;
    uint8_t scanResults[126] = {0};
};

class ESP32NRF24Jammer {
public:
    ESP32NRF24Jammer(int8_t ledPin, const NRF24RadioConfig& radioConfig);

    bool begin(void);
    bool isRadioReady(void) const;
    void setRadioConfig(const NRF24RadioConfig& radioConfig);
    
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
    void getStateSnapshot(JammerStateSnapshot& snapshot);  // Copy current UI state

    void setFrequencyStep(uint32_t stepHz);   // For continuous sweep (default 500kHz)
    void setChannel(uint16_t channel);      // Set single channel (0-124)

    // Internal - called by task
    void _scanTask(void);

private:
    int8_t _ledPin;
    NRF24RadioConfig _radioConfig;
    NRF24L01 _radio;
    
    bool _safeMode = false;   // Default off
    bool _isJamming = false;
    bool _radioReady = false;

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
    portMUX_TYPE _stateMux = portMUX_INITIALIZER_UNLOCKED;
    
    void _setChannelInternal(uint16_t ch);
    void _configureAggressiveMode(void);
    void _printRegisters(const char* label);  // Dump key registers to Serial
    void _setLedColor(uint8_t red, uint8_t green, uint8_t blue);
};
#endif  // ESP32_NRF24_JAMMER_H_
