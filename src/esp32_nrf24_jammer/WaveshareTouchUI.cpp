#include "WaveshareTouchUI.h"

#include <cstring>

namespace {
constexpr int kBacklightPin = 6;
constexpr int kLcdMisoPin = 2;
constexpr int kLcdMosiPin = 1;
constexpr int kLcdSckPin = 5;
constexpr int kLcdCsPin = -1;
constexpr int kLcdDcPin = 3;
constexpr int kLcdResetPin = -1;
constexpr int kLcdWidth = 320;
constexpr int kLcdHeight = 480;
constexpr int kI2cSdaPin = 8;
constexpr int kI2cSclPin = 7;
constexpr uint8_t kLcdResetExpanderPin = 1;

constexpr int kStatusBoxX = 10;
constexpr int kStatusBoxY = 10;
constexpr int kStatusBoxW = 300;
constexpr int kStatusBoxH = 68;

constexpr int kGraphX = 10;
constexpr int kGraphY = 90;
constexpr int kGraphW = 300;
constexpr int kGraphH = 220;

constexpr int kSummaryX = 10;
constexpr int kSummaryY = 322;
constexpr int kSummaryW = 300;
constexpr int kSummaryH = 148;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3));
}

constexpr uint16_t kColorBackground = rgb565(9, 14, 22);
constexpr uint16_t kColorPanel = rgb565(22, 30, 46);
constexpr uint16_t kColorBorder = rgb565(44, 60, 82);
constexpr uint16_t kColorText = rgb565(229, 235, 240);
constexpr uint16_t kColorMuted = rgb565(126, 140, 155);
constexpr uint16_t kColorSuccess = rgb565(24, 198, 124);
constexpr uint16_t kColorWarning = rgb565(255, 170, 0);
constexpr uint16_t kColorDanger = rgb565(226, 82, 76);
constexpr uint16_t kColorBluetooth = rgb565(0, 160, 255);

constexpr uint8_t kSignalStartThreshold = 12;
constexpr uint8_t kSignalKeepThreshold = 6;
constexpr uint8_t kSignalBridgeThreshold = 10;
constexpr uint8_t kSignalMaxGap = 2;

uint8_t clampPercent(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return static_cast<uint8_t>(value);
}
}  // namespace

bool WaveshareTouchUI::begin() {
    Wire.begin(kI2cSdaPin, kI2cSclPin);

    if (!_ioExpander.begin()) {
        Serial.println("[UI] ERROR: TCA9554 expander not detected");
        return false;
    }

    _ioExpander.pinMode1(kLcdResetExpanderPin, OUTPUT);
    resetDisplay();

    _bus = new Arduino_ESP32SPI(
        kLcdDcPin,
        kLcdCsPin,
        kLcdSckPin,
        kLcdMosiPin,
        kLcdMisoPin,
        FSPI);
    _gfx = new Arduino_ST7796(_bus, kLcdResetPin, 0, true, kLcdWidth, kLcdHeight);

    if (!_gfx->begin()) {
        Serial.println("[UI] ERROR: LCD init failed");
        return false;
    }

    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, HIGH);

    _gfx->fillScreen(kColorBackground);
    _gfx->setTextWrap(false);
    _displayReady = true;

    if (_touch.begin(Wire, FT6X36_SLAVE_ADDRESS)) {
        _touchReady = true;
        Serial.println("[UI] FT6336 touch ready");
    } else {
        Serial.println("[UI] WARNING: FT6336 touch not detected");
    }

    drawStaticLayout();
    return true;
}

void WaveshareTouchUI::update(const JammerStateSnapshot& state) {
    if (!_displayReady) {
        return;
    }

    if (!_layoutDrawn) {
        drawStaticLayout();
    }

    if ((millis() - _lastRenderMs) > 125 || stateChanged(state)) {
        drawDynamicSections(state);
        _lastRenderMs = millis();
        _lastRadioReady = state.radioReady;
        _lastScanReady = state.scanReady;
        memcpy(_lastScanResults, state.scanResults, sizeof(_lastScanResults));
    }
}

void WaveshareTouchUI::resetDisplay() {
    _ioExpander.write1(kLcdResetExpanderPin, HIGH);
    delay(10);
    _ioExpander.write1(kLcdResetExpanderPin, LOW);
    delay(10);
    _ioExpander.write1(kLcdResetExpanderPin, HIGH);
    delay(200);
}

void WaveshareTouchUI::drawStaticLayout() {
    _gfx->fillScreen(kColorBackground);

    _gfx->fillRoundRect(kStatusBoxX, kStatusBoxY, kStatusBoxW, kStatusBoxH, 8, kColorPanel);
    _gfx->drawRoundRect(kStatusBoxX, kStatusBoxY, kStatusBoxW, kStatusBoxH, 8, kColorBorder);

    _gfx->fillRoundRect(kGraphX, kGraphY, kGraphW, kGraphH, 8, kColorPanel);
    _gfx->drawRoundRect(kGraphX, kGraphY, kGraphW, kGraphH, 8, kColorBorder);

    _gfx->fillRoundRect(kSummaryX, kSummaryY, kSummaryW, kSummaryH, 8, kColorPanel);
    _gfx->drawRoundRect(kSummaryX, kSummaryY, kSummaryW, kSummaryH, 8, kColorBorder);

    _gfx->setTextColor(kColorText);
    _gfx->setTextSize(2);
    _gfx->setCursor(14, 96);
    _gfx->print("2.4GHz Spectrum");
    _gfx->setCursor(14, 328);
    _gfx->print("Detected Signals");

    _gfx->setTextColor(kColorMuted);
    _gfx->setTextSize(1);
    _gfx->setCursor(18, 300);
    _gfx->print("2400");
    _gfx->setCursor(104, 300);
    _gfx->print("WiFi-1");
    _gfx->setCursor(183, 300);
    _gfx->print("WiFi-6");
    _gfx->setCursor(253, 300);
    _gfx->print("2525");

    _layoutDrawn = true;
}

void WaveshareTouchUI::drawDynamicSections(const JammerStateSnapshot& state) {
    drawStatusBanner(state);
    drawGraph(state);
    drawSummary(state);
}

void WaveshareTouchUI::drawStatusBanner(const JammerStateSnapshot& state) {
    const uint16_t radioColor = state.radioReady ? kColorSuccess : kColorDanger;
    int peakChannel = -1;
    uint8_t peakStrength = 0;
    uint8_t activeChannels = 0;

    if (state.scanReady) {
        for (int channel = 0; channel < 126; ++channel) {
            const uint8_t value = state.scanResults[channel];
            if (value >= kSignalStartThreshold) {
                activeChannels++;
            }
            if (value > peakStrength) {
                peakStrength = value;
                peakChannel = channel;
            }
        }
    }

    _gfx->fillRect(kStatusBoxX + 2, kStatusBoxY + 2, kStatusBoxW - 4, kStatusBoxH - 4, kColorPanel);

    _gfx->setTextColor(kColorText);
    _gfx->setTextSize(2);
    _gfx->setCursor(16, 18);
    _gfx->print("Radio");
    _gfx->setTextColor(radioColor);
    _gfx->setCursor(82, 18);
    _gfx->print(state.radioReady ? "READY" : "NOT FOUND");

    _gfx->setTextColor(kColorText);
    _gfx->setCursor(16, 44);
    _gfx->print("Mode");
    _gfx->setTextColor(kColorSuccess);
    _gfx->setCursor(82, 44);
    _gfx->print("ANALYZE");

    _gfx->setTextColor(kColorMuted);
    _gfx->setTextSize(1);
    _gfx->setCursor(176, 16);
    _gfx->print(state.scanReady ? "scan live" : "scan pending");
    _gfx->setCursor(176, 30);
    _gfx->print("active ");
    _gfx->print(activeChannels);
    _gfx->print("/126");
    _gfx->setCursor(176, 44);
    _gfx->print("peak ");
    if (peakChannel >= 0) {
        _gfx->print(2400 + peakChannel);
        _gfx->print("MHz ");
        _gfx->print(peakStrength);
        _gfx->print("%");
    } else {
        _gfx->print("--");
    }
    _gfx->setCursor(176, 58);
    _gfx->print(_touchReady ? "touch online" : "touch offline");
}

void WaveshareTouchUI::drawGraph(const JammerStateSnapshot& state) {
    const int innerX = kGraphX + 8;
    const int innerY = kGraphY + 26;
    const int innerW = kGraphW - 16;
    const int innerH = kGraphH - 42;

    _gfx->fillRect(innerX, innerY, innerW, innerH, rgb565(4, 8, 14));

    for (int grid = 0; grid <= 4; ++grid) {
        const int y = innerY + (grid * innerH) / 4;
        _gfx->drawFastHLine(innerX, y, innerW, rgb565(18, 28, 42));
    }

    if (!state.scanReady) {
        _gfx->setTextColor(kColorMuted);
        _gfx->setTextSize(2);
        _gfx->setCursor(innerX + 58, innerY + innerH / 2 - 8);
        _gfx->print("Waiting for scan");
        return;
    }

    for (int channel = 0; channel < 126; ++channel) {
        const int x0 = innerX + (channel * innerW) / 126;
        const int x1 = innerX + ((channel + 1) * innerW) / 126;
        const int width = max(1, x1 - x0 - 1);
        const int height = (state.scanResults[channel] * innerH) / 100;
        const int y = innerY + innerH - height;

        uint16_t color = kColorWarning;
        if (channel <= 80) {
            color = kColorBluetooth;
        } else if (channel >= 12 && channel <= 84) {
            color = kColorSuccess;
        }

        _gfx->fillRect(x0, y, width, max(1, height), color);
    }
}

void WaveshareTouchUI::drawSummary(const JammerStateSnapshot& state) {
    _gfx->fillRect(kSummaryX + 2, kSummaryY + 24, kSummaryW - 4, kSummaryH - 28, kColorPanel);

    if (!state.scanReady) {
        _gfx->setTextColor(kColorMuted);
        _gfx->setTextSize(2);
        _gfx->setCursor(kSummaryX + 16, kSummaryY + 48);
        _gfx->print("No scan data yet");
        return;
    }

    SignalSummary summaries[kSummaryCount] = {};
    const uint8_t count = buildSignalSummary(state.scanResults, summaries, kSummaryCount);

    if (count == 0) {
        _gfx->setTextColor(kColorMuted);
        _gfx->setTextSize(2);
        _gfx->setCursor(kSummaryX + 16, kSummaryY + 48);
        _gfx->print("No clear signals");
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        const int y = kSummaryY + 30 + (i * 16);
        _gfx->setTextColor(kColorText);
        _gfx->setTextSize(1);
        _gfx->setCursor(kSummaryX + 14, y);
        _gfx->print(summaries[i].type);

        _gfx->setCursor(kSummaryX + 102, y);
        _gfx->print(summaries[i].freqStart);
        _gfx->print("-");
        _gfx->print(summaries[i].freqEnd);
        _gfx->print(" MHz");

        _gfx->setTextColor(kColorWarning);
        _gfx->setCursor(kSummaryX + 248, y);
        _gfx->print(summaries[i].strength);
        _gfx->print("%");
    }
}

bool WaveshareTouchUI::stateChanged(const JammerStateSnapshot& state) const {
    if (!_layoutDrawn) {
        return true;
    }

    if (state.radioReady != _lastRadioReady || state.scanReady != _lastScanReady) {
        return true;
    }

    return memcmp(_lastScanResults, state.scanResults, sizeof(_lastScanResults)) != 0;
}

uint8_t WaveshareTouchUI::buildSignalSummary(const uint8_t* channels, SignalSummary* summaries, uint8_t maxCount) const {
    uint8_t count = 0;
    int index = 0;

    while (index < 126) {
        if (channels[index] < kSignalStartThreshold) {
            ++index;
            continue;
        }

        const int start = index;
        uint8_t peak = channels[index];
        int sum = 0;
        int samples = 0;
        int gapCount = 0;

        while (index < 126) {
            const uint8_t value = channels[index];
            const bool solidSignal = value >= kSignalKeepThreshold;
            const bool bridgedGap = gapCount < kSignalMaxGap &&
                value < kSignalKeepThreshold &&
                (index + 1) < 126 &&
                channels[index + 1] >= kSignalBridgeThreshold;

            if (!solidSignal && !bridgedGap) {
                break;
            }

            if (solidSignal) {
                if (value > peak) {
                    peak = value;
                }
                gapCount = 0;
            } else {
                gapCount++;
            }

            sum += value;
            samples++;
            ++index;
        }

        const int end = index - 1;
        const int width = end - start + 1;
        const int average = samples > 0 ? (sum / samples) : peak;
        const uint8_t strength = clampPercent(((peak * 7) + (average * 3) + min(width, 16)) / 10);

        if (peak < kSignalStartThreshold && average < kSignalKeepThreshold) {
            continue;
        }

        const char* type = "RF Signal";
        if (width >= 8 && width <= 28) {
            type = "WiFi";
        } else if (width <= 4) {
            type = "Bluetooth";
        } else if (width > 25) {
            type = "Wide Band";
        }

        SignalSummary candidate = {
            type,
            static_cast<uint16_t>(2400 + start),
            static_cast<uint16_t>(2400 + end),
            strength,
        };

        uint8_t insertPos = count;
        for (uint8_t i = 0; i < count; ++i) {
            if (candidate.strength > summaries[i].strength) {
                insertPos = i;
                break;
            }
        }

        if (insertPos < maxCount) {
            uint8_t moveEnd = count;
            if (moveEnd > static_cast<uint8_t>(maxCount - 1)) {
                moveEnd = static_cast<uint8_t>(maxCount - 1);
            }
            for (int move = moveEnd; move > insertPos; --move) {
                summaries[move] = summaries[move - 1];
            }
            summaries[insertPos] = candidate;
            if (count < maxCount) {
                ++count;
            }
        }
    }

    return count;
}
