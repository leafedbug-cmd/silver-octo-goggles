// esp32_nrf24_jammer/NRF24L01.cpp

#include "NRF24L01.h"

#define CMD_R_REGISTER 0x00
#define CMD_W_REGISTER 0x20
#define REG_CONFIG     0x00

NRF24L01::NRF24L01(uint8_t cePin, uint8_t csnPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin)
    : _cePin(cePin), _csnPin(csnPin), _sckPin(sckPin), _misoPin(misoPin), _mosiPin(mosiPin) {}

bool NRF24L01::begin(void) {
    pinMode(_cePin, OUTPUT);
    pinMode(_csnPin, OUTPUT);
    digitalWrite(_cePin, LOW);
    digitalWrite(_csnPin, HIGH);

    SPI.begin(_sckPin, _misoPin, _mosiPin, -1);

    reset();

    delay(5);
    return isPresent();
}

void NRF24L01::powerDown(void) {
    uint8_t config = readRegister(REG_CONFIG);
    config &= static_cast<uint8_t>(~0x02);
    writeRegister(REG_CONFIG, config);
}

void NRF24L01::powerUp(void) {
    uint8_t config = readRegister(REG_CONFIG);
    config |= 0x02;
    writeRegister(REG_CONFIG, config);
    delay(2);
}

void NRF24L01::reset(void) {
    powerDown();
    writeRegister(REG_CONFIG, 0x0A);
}

bool NRF24L01::isPresent(void) {
    const uint8_t probe = 0x2A;
    writeRegister(REG_CONFIG, probe);
    return readRegister(REG_CONFIG) == probe;
}

void NRF24L01::writeRegister(uint8_t reg, uint8_t value) {
    digitalWrite(_csnPin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    transfer(CMD_W_REGISTER | (reg & 0x1F));
    transfer(value);
    SPI.endTransaction();
    digitalWrite(_csnPin, HIGH);
}

uint8_t NRF24L01::readRegister(uint8_t reg) {
    digitalWrite(_csnPin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    transfer(CMD_R_REGISTER | (reg & 0x1F));
    uint8_t value = transfer(0xFF);
    SPI.endTransaction();
    digitalWrite(_csnPin, HIGH);
    return value;
}

void NRF24L01::pulseCE(void) {
    digitalWrite(_cePin, LOW);
    delayMicroseconds(10);
    digitalWrite(_cePin, HIGH);
    delayMicroseconds(10);
}

void NRF24L01::setCEHigh(void) {
    digitalWrite(_cePin, HIGH);
}

void NRF24L01::setCELow(void) {
    digitalWrite(_cePin, LOW);
}

void NRF24L01::setMaxPower(void) {
    // RF_SETUP: 5dBm power (bits 2:1 = 11), 2Mbps (bit 3 = 1)
    writeRegister(0x06, 0x0F);  // 5dBm, 2Mbps, LNA gain
}

void NRF24L01::disableAutoAck(void) {
    writeRegister(0x01, 0x00);  // EN_AA = 0 (disable auto-ack on all pipes)
}

void NRF24L01::setChannel(uint8_t ch) {
    writeRegister(0x05, ch & 0x7F);  // RF_CH register, max 127
}

void NRF24L01::setRxMode(void) {
    uint8_t config = readRegister(REG_CONFIG);
    config |= 0x03;  // PWR_UP + PRIM_RX
    writeRegister(REG_CONFIG, config);
    setCEHigh();
    delayMicroseconds(130);  // RX settling time
}

void NRF24L01::setTxMode(void) {
    uint8_t config = readRegister(REG_CONFIG);
    config |= 0x02;   // PWR_UP
    config &= ~0x01;  // PRIM_RX = 0 (TX mode)
    writeRegister(REG_CONFIG, config);
}

bool NRF24L01::detectSignal(void) {
    // RPD register (0x09) - bit 0 indicates received power > -64dBm
    return (readRegister(0x09) & 0x01) != 0;
}

void NRF24L01::scanAllChannels(uint8_t* results, int numSamples) {
    disableAutoAck();
    
    for (int ch = 0; ch < 126; ch++) {
        setChannel(ch);
        int detected = 0;
        
        for (int s = 0; s < numSamples; s++) {
            setRxMode();
            delayMicroseconds(170);  // Listen briefly
            if (detectSignal()) {
                detected++;
            }
            setCELow();
        }
        
        // Store as percentage (0-100)
        results[ch] = (detected * 100) / numSamples;
        
        // Yield every 5 channels to let WiFi stack run and feed watchdog
        if (ch % 5 == 0) {
            yield();
            delay(1);  // Feed watchdog timer
        }
    }
}

uint8_t NRF24L01::transfer(uint8_t val) {
    return SPI.transfer(val);
}

void NRF24L01::flushTx(void) {
    digitalWrite(_csnPin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    transfer(0xE1);  // FLUSH_TX command
    SPI.endTransaction();
    digitalWrite(_csnPin, HIGH);
}

void NRF24L01::writeTxPayload(const uint8_t* data, uint8_t len) {
    digitalWrite(_csnPin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    transfer(0xA0);  // W_TX_PAYLOAD command
    for (uint8_t i = 0; i < len; i++) {
        transfer(data[i]);
    }
    SPI.endTransaction();
    digitalWrite(_csnPin, HIGH);
}

void NRF24L01::transmit(void) {
    setCEHigh();
    delayMicroseconds(15);  // Min CE high time for TX
    setCELow();
}
