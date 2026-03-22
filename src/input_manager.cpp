#include "input_manager.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/app_config.h"
#include "display_manager.h"

namespace {

constexpr uint8_t kTouchReadCommand[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};
constexpr uint8_t kTouchReadBytes = 8;

void printTouchActionLog(const char* label,
                        int16_t startX,
                        int16_t startY,
                        int16_t endX,
                        int16_t endY,
                        unsigned long durationMs,
                        uint16_t moveX,
                        uint16_t moveY) {
    Serial.print("[TOUCH] ");
    Serial.print(label);
    Serial.print(" start=(");
    Serial.print(startX);
    Serial.print(",");
    Serial.print(startY);
    Serial.print(") end=(");
    Serial.print(endX);
    Serial.print(",");
    Serial.print(endY);
    Serial.print(") duration_ms=");
    Serial.print(durationMs);
    Serial.print(" move_x=");
    Serial.print(moveX);
    Serial.print(" move_y=");
    Serial.print(moveY);
    Serial.print(" thresholds(tap_ms=");
    Serial.print(app::TOUCH_TAP_MAX_MS);
    Serial.print(" tap_move=");
    Serial.print(app::TOUCH_TAP_MOVEMENT_PX);
    Serial.print(" swipe_ms=");
    Serial.print(app::TOUCH_SWIPE_MAX_MS);
    Serial.print(" swipe_dist=");
    Serial.print(app::TOUCH_SWIPE_DISTANCE_PX);
    Serial.print(" long_press_ms=");
    Serial.print(app::TOUCH_LONG_PRESS_MS);
    Serial.println(")");
}

struct TouchPoint {
    int16_t x;
    int16_t y;
};

struct TouchTracker {
    bool initialized = false;
    bool hasTouch = false;
    bool touchValid = false;
    unsigned long startMs = 0;
    TouchPoint start = {0, 0};
    TouchPoint last = {0, 0};
};

input::TouchCalibrationData defaultCalibrationData() {
    input::TouchCalibrationData data{};
    data.valid = app::TOUCH_CALIBRATION_DEFAULT_VALID;
    data.swapXY = app::TOUCH_CALIBRATION_DEFAULT_SWAP_XY;
    data.mapX1 = app::TOUCH_CALIBRATION_DEFAULT_MAP_X1;
    data.mapX2 = app::TOUCH_CALIBRATION_DEFAULT_MAP_X2;
    data.mapY1 = app::TOUCH_CALIBRATION_DEFAULT_MAP_Y1;
    data.mapY2 = app::TOUCH_CALIBRATION_DEFAULT_MAP_Y2;
    return data;
}

TouchTracker gTracker;
TouchPoint gLastActionPoint = {0, 0};
bool gTouchDebugEnabled = app::TOUCH_DEBUG_DEFAULT;
bool gTouchBusReady = false;
input::TouchCalibrationData gTouchCalibration = defaultCalibrationData();

input::TouchAction remapGestureForBoard(input::TouchAction action) {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    if (gTouchCalibration.valid) {
        return action;
    }
    if ((app::DISPLAY_ROTATION & 0x03) == 3) {
        switch (action) {
            case input::TouchAction::SWIPE_RIGHT:
                return input::TouchAction::SWIPE_UP;
            case input::TouchAction::SWIPE_LEFT:
                return input::TouchAction::SWIPE_DOWN;
            case input::TouchAction::SWIPE_UP:
                return input::TouchAction::SWIPE_LEFT;
            case input::TouchAction::SWIPE_DOWN:
                return input::TouchAction::SWIPE_RIGHT;
            case input::TouchAction::NONE:
            case input::TouchAction::TAP:
            case input::TouchAction::LONG_PRESS:
            default:
                return action;
        }
    }
#endif
    return action;
}

int16_t clampCoord(int32_t value, int16_t maxValue) {
    if (value < 0) {
        return 0;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return static_cast<int16_t>(value);
}

int16_t mapAxisClamped(int16_t rawValue, int16_t inMin, int16_t inMax, int16_t outMax) {
    if (outMax <= 0) {
        return 0;
    }
    if (inMin == inMax) {
        return clampCoord(rawValue, outMax);
    }

    const int32_t numerator = static_cast<int32_t>(rawValue - inMin) * static_cast<int32_t>(outMax);
    const int32_t denominator = static_cast<int32_t>(inMax - inMin);
    return clampCoord(numerator / denominator, outMax);
}

bool readRawTouchPointInternal(int16_t& rawXOut, int16_t& rawYOut) {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    if (!gTouchBusReady) {
        Wire.begin(app::I2C_SDA_PIN, app::I2C_SCL_PIN);
        gTouchBusReady = true;
    }

    uint8_t buff[kTouchReadBytes] = {0};
    Wire.beginTransmission(app::TOUCH_I2C_ADDRESS);
    Wire.write(kTouchReadCommand, sizeof(kTouchReadCommand));
    if (Wire.endTransmission() != 0) {
        return false;
    }

    const uint8_t available = Wire.requestFrom(app::TOUCH_I2C_ADDRESS, kTouchReadBytes);
    if (available < kTouchReadBytes) {
        return false;
    }

    if (Wire.readBytes(buff, kTouchReadBytes) < kTouchReadBytes) {
        return false;
    }

    const uint8_t gesture = buff[0];
    const uint16_t rawX = static_cast<uint16_t>(((buff[2] & 0x0F) << 8) | buff[3]);
    const uint16_t rawY = static_cast<uint16_t>(((buff[4] & 0x0F) << 8) | buff[5]);
    if (gesture != 0 || (rawX == 0 && rawY == 0)) {
        return false;
    }

    rawXOut = static_cast<int16_t>(rawX);
    rawYOut = static_cast<int16_t>(rawY);
    return true;
#else
    (void)rawXOut;
    (void)rawYOut;
    return false;
#endif
}

bool readTouchPoint(int16_t& xOut, int16_t& yOut) {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    int16_t rawX = 0;
    int16_t rawY = 0;
    if (!readRawTouchPointInternal(rawX, rawY)) {
        return false;
    }

    const uint8_t rotation = static_cast<uint8_t>(app::DISPLAY_ROTATION & 0x03);
    const int16_t nativeWidth = static_cast<int16_t>(app::DISPLAY_WIDTH);
    const int16_t nativeHeight = static_cast<int16_t>(app::DISPLAY_HEIGHT);
    const int16_t screenWidth = static_cast<int16_t>(
        display::isReady() ? display::width() : ((rotation & 0x01) ? app::DISPLAY_HEIGHT : app::DISPLAY_WIDTH));
    const int16_t screenHeight = static_cast<int16_t>(
        display::isReady() ? display::height() : ((rotation & 0x01) ? app::DISPLAY_WIDTH : app::DISPLAY_HEIGHT));

    int16_t screenX = 0;
    int16_t screenY = 0;
    if (gTouchCalibration.valid) {
        const int16_t maxX = static_cast<int16_t>(screenWidth - 1);
        const int16_t maxY = static_cast<int16_t>(screenHeight - 1);
        if (gTouchCalibration.swapXY) {
            screenX = mapAxisClamped(rawY, gTouchCalibration.mapX1, gTouchCalibration.mapX2, maxX);
            screenY = mapAxisClamped(rawX, gTouchCalibration.mapY1, gTouchCalibration.mapY2, maxY);
        } else {
            screenX = mapAxisClamped(rawX, gTouchCalibration.mapX1, gTouchCalibration.mapX2, maxX);
            screenY = mapAxisClamped(rawY, gTouchCalibration.mapY1, gTouchCalibration.mapY2, maxY);
        }
    } else {
        switch (rotation) {
            case 0:
                screenX = static_cast<int16_t>(rawX);
                screenY = static_cast<int16_t>(rawY);
                break;
            case 1:
                screenX = static_cast<int16_t>(nativeHeight - 1 - rawY);
                screenY = static_cast<int16_t>(rawX);
                break;
            case 2:
                screenX = static_cast<int16_t>(nativeWidth - 1 - rawX);
                screenY = static_cast<int16_t>(nativeHeight - 1 - rawY);
                break;
            case 3:
                screenX = static_cast<int16_t>(rawY);
                screenY = static_cast<int16_t>(nativeWidth - 1 - rawX);
                break;
            default:
                break;
        }
    }

    if (screenX < 0) {
        screenX = 0;
    } else if (screenX >= screenWidth) {
        screenX = static_cast<int16_t>(screenWidth - 1);
    }

    if (screenY < 0) {
        screenY = 0;
    } else if (screenY >= screenHeight) {
        screenY = static_cast<int16_t>(screenHeight - 1);
    }

    xOut = screenX;
    yOut = screenY;
    return true;
#else
    (void)xOut;
    (void)yOut;
    return false;
#endif
}

inline uint16_t abs16(int16_t a) {
    return static_cast<uint16_t>(a >= 0 ? a : -a);
}

TouchPoint midpoint(const TouchPoint& a, const TouchPoint& b) {
    TouchPoint point{};
    point.x = static_cast<int16_t>((static_cast<int32_t>(a.x) + static_cast<int32_t>(b.x)) / 2);
    point.y = static_cast<int16_t>((static_cast<int32_t>(a.y) + static_cast<int32_t>(b.y)) / 2);
    return point;
}

}  // namespace

namespace input {

void TouchManager::begin() {
    gTracker = TouchTracker{};
    gLastActionPoint = {0, 0};
    gTouchDebugEnabled = app::TOUCH_DEBUG_DEFAULT;
    gTouchCalibration = defaultCalibrationData();
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    gTouchBusReady = false;
#else
    gTouchBusReady = false;
#endif
}

void TouchManager::setDebugEnabled(bool enabled) {
    gTouchDebugEnabled = enabled;
}

bool TouchManager::debugEnabled() const {
    return gTouchDebugEnabled;
}

int16_t TouchManager::lastActionX() const {
    return gLastActionPoint.x;
}

int16_t TouchManager::lastActionY() const {
    return gLastActionPoint.y;
}

TouchAction TouchManager::update(unsigned long nowMs) {
    int16_t x = 0;
    int16_t y = 0;
    const bool touched = readTouchPoint(x, y);

    if (touched) {
        if (!gTracker.hasTouch) {
            gTracker.hasTouch = true;
            gTracker.touchValid = true;
            gTracker.startMs = nowMs;
            gTracker.start = {x, y};
            gTracker.last = {x, y};
        } else {
            gTracker.last = {x, y};
        }
        return TouchAction::NONE;
    }

    if (!gTracker.hasTouch) {
        return TouchAction::NONE;
    }

    gTracker.hasTouch = false;
    if (!gTracker.touchValid) {
        return TouchAction::NONE;
    }

    const int16_t dx = gTracker.last.x - gTracker.start.x;
    const int16_t dy = gTracker.last.y - gTracker.start.y;
    const uint16_t moveX = abs16(dx);
    const uint16_t moveY = abs16(dy);
    const unsigned long durationMs = nowMs - gTracker.startMs;

    gTracker.touchValid = false;

    if (durationMs <= app::TOUCH_TAP_MAX_MS &&
        moveX < app::TOUCH_SWIPE_DISTANCE_PX &&
        moveY < app::TOUCH_SWIPE_DISTANCE_PX) {
        gLastActionPoint = gTracker.start;
        if (gTouchDebugEnabled) {
            printTouchActionLog("TAP",
                               gTracker.start.x,
                               gTracker.start.y,
                               gTracker.last.x,
                               gTracker.last.y,
                               durationMs,
                               moveX,
                               moveY);
        }
        return TouchAction::TAP;
    }

    if (durationMs >= app::TOUCH_LONG_PRESS_MS &&
        moveX <= app::TOUCH_TAP_MOVEMENT_PX &&
        moveY <= app::TOUCH_TAP_MOVEMENT_PX) {
        gLastActionPoint = gTracker.start;
        if (gTouchDebugEnabled) {
            printTouchActionLog("LONG_PRESS",
                               gTracker.start.x,
                               gTracker.start.y,
                               gTracker.last.x,
                               gTracker.last.y,
                               durationMs,
                               moveX,
                               moveY);
        }
        return TouchAction::LONG_PRESS;
    }

    if (durationMs <= app::TOUCH_SWIPE_MAX_MS &&
        (moveX >= app::TOUCH_SWIPE_DISTANCE_PX ||
         moveY >= app::TOUCH_SWIPE_DISTANCE_PX)) {
        TouchAction action = TouchAction::NONE;
        gLastActionPoint = gTracker.last;
        if (moveX >= moveY) {
            action = (dx < 0) ? TouchAction::SWIPE_LEFT : TouchAction::SWIPE_RIGHT;
        } else {
            action = (dy < 0) ? TouchAction::SWIPE_UP : TouchAction::SWIPE_DOWN;
        }
        action = remapGestureForBoard(action);

        if (gTouchDebugEnabled) {
            const char* label = "UNKNOWN";
            switch (action) {
                case TouchAction::SWIPE_LEFT:
                    label = "SWIPE_LEFT";
                    break;
                case TouchAction::SWIPE_RIGHT:
                    label = "SWIPE_RIGHT";
                    break;
                case TouchAction::SWIPE_UP:
                    label = "SWIPE_UP";
                    break;
                case TouchAction::SWIPE_DOWN:
                    label = "SWIPE_DOWN";
                    break;
                case TouchAction::LONG_PRESS:
                    label = "LONG_PRESS";
                    break;
                default:
                    break;
            }
            printTouchActionLog(label,
                               gTracker.start.x,
                               gTracker.start.y,
                               gTracker.last.x,
                               gTracker.last.y,
                               durationMs,
                               moveX,
                               moveY);
        }
        return action;
    }

    if (gTouchDebugEnabled) {
        printTouchActionLog("NONE",
                           gTracker.start.x,
                           gTracker.start.y,
                           gTracker.last.x,
                           gTracker.last.y,
                           durationMs,
                           moveX,
                           moveY);
    }

    return TouchAction::NONE;
}

bool readRawPoint(int16_t& rawXOut, int16_t& rawYOut) {
    return readRawTouchPointInternal(rawXOut, rawYOut);
}

void setCalibration(const TouchCalibrationData& data) {
    gTouchCalibration = data;
}

void clearCalibration() {
    gTouchCalibration = defaultCalibrationData();
}

bool calibrationEnabled() {
    return gTouchCalibration.valid;
}

TouchCalibrationData calibrationData() {
    return gTouchCalibration;
}

}  // namespace input
