// esp32_nrf24_jammer/NRF24L01.h

#ifndef NRF24L01_H_
#define NRF24L01_H_

#include <Arduino.h>
#include <SPI.h>

class NRF24L01 {
public:
    NRF24L01(uint8_t cePin, uint8_t csnPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin);
    
    bool begin(void);
    
    void powerDown(void);
    void powerUp(void);
    void reset(void);

    bool isPresent(void);  // Check if module responds to SPI
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    void pulseCE(void);
    void setCEHigh(void);
    void setCELow(void);
    
    // Scanner functions
    void setRxMode(void);
    void setTxMode(void);
    void setMaxPower(void);  // 0dBm max output
    void disableAutoAck(void);
    void setChannel(uint8_t ch);
    bool detectSignal(void);  // Check RPD register
    void scanAllChannels(uint8_t* results, int numSamples = 100);    void flushTx(void);
    void writeTxPayload(const uint8_t* data, uint8_t len);
    void transmit(void);
private:
    uint8_t _cePin;
    uint8_t _csnPin;
    uint8_t _sckPin;
    uint8_t _misoPin;
    uint8_t _mosiPin;

    uint8_t transfer(uint8_t val);

    static const int MAX_TX = 32;   // Largest payload size
    static const int MAX_RX = 32;
    
    uint8_t _rxBuffer[MAX_RX];
    uint8_t _txBuffer[MAX_TX];

    // ... rest of the class remains mostly unchanged ...
};

#endif  // NRF24L01_H_
