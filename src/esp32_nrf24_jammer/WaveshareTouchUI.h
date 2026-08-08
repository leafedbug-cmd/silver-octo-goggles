#ifndef WAVESHARE_TOUCH_UI_H_
#define WAVESHARE_TOUCH_UI_H_

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TCA9554.h>
#include <TouchDrvFT6X36.hpp>
#include <Wire.h>
#include "esp32_nrf24_jammer.h"

class WaveshareTouchUI {
public:
    bool begin();
    void update(const JammerStateSnapshot& state);

private:
    struct SignalSummary {
        const char* type;
        uint16_t freqStart;
        uint16_t freqEnd;
        uint8_t strength;
    };

    static constexpr uint8_t kSummaryCount = 3;

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX* _gfx = nullptr;
    TCA9554 _ioExpander = TCA9554(0x20);
    TouchDrvFT6X36 _touch;

    bool _displayReady = false;
    bool _touchReady = false;
    bool _layoutDrawn = false;
    unsigned long _lastRenderMs = 0;

    bool _lastRadioReady = false;
    bool _lastScanReady = false;
    uint8_t _lastScanResults[126] = {0};

    void resetDisplay();
    void drawStaticLayout();
    void drawDynamicSections(const JammerStateSnapshot& state);
    void drawStatusBanner(const JammerStateSnapshot& state);
    void drawGraph(const JammerStateSnapshot& state);
    void drawSummary(const JammerStateSnapshot& state);
    bool stateChanged(const JammerStateSnapshot& state) const;
    uint8_t buildSignalSummary(const uint8_t* channels, SignalSummary* summaries, uint8_t maxCount) const;
};

#endif  // WAVESHARE_TOUCH_UI_H_
