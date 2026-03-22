#pragma once

#include <stdint.h>

namespace input {

enum class TouchAction : uint8_t {
    NONE = 0,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN,
    TAP,
    LONG_PRESS,
};

struct TouchCalibrationData {
    bool valid = false;
    bool swapXY = false;
    int16_t mapX1 = 0;
    int16_t mapX2 = 0;
    int16_t mapY1 = 0;
    int16_t mapY2 = 0;
};

class TouchManager {
public:
    void begin();
    TouchAction update(unsigned long nowMs);
    void setDebugEnabled(bool enabled);
    bool debugEnabled() const;
    int16_t lastActionX() const;
    int16_t lastActionY() const;
};

bool readRawPoint(int16_t& rawXOut, int16_t& rawYOut);
void setCalibration(const TouchCalibrationData& data);
void clearCalibration();
bool calibrationEnabled();
TouchCalibrationData calibrationData();

}  // namespace input
