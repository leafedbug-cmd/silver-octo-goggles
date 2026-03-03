// esp32_nrf24_jammer/NRF24L01.cpp

#include "NRF24L01.h"

#define CMD_R_REGISTER 0x00
#define CMD_W_REGISTER 0x20
#define REG_CONFIG     0x00

NRF24L01::NRF24L01(uint8_t cePin, uint8_t csnPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin)
    : _cePin(cePin), _csnPin(csnPin), _sckPin(sckPin), _misoPin(misoPin), _mosiPin(mosiPin) {
    pinMode(_cePin, OUTPUT);
    pinMode(_csnPin, OUTPUT);
    digitalWrite(_cePin, LOW);
    digitalWrite(_csnPin, HIGH);
    
    SPI.begin(_sckPin, _misoPin, _mosiPin, -1);
}

bool NRF24L01::begin(void) {
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

uint8_t NRF24L01::transfer(uint8_t val) {
    return SPI.transfer(val);
}
