#pragma once

#include <stdint.h>

namespace app {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
static constexpr uint8_t I2C_SDA_PIN = 15;
static constexpr uint8_t I2C_SCL_PIN = 10;
static constexpr uint8_t DISPLAY_ROTATION = 3;
static constexpr uint16_t DISPLAY_WIDTH = 180;
static constexpr uint16_t DISPLAY_HEIGHT = 640;
static constexpr uint8_t DISPLAY_BACKLIGHT_PIN = 1;
static constexpr uint8_t DISPLAY_RESET_PIN = 16;
static constexpr uint8_t DISPLAY_QSPI_CS_PIN = 12;
static constexpr uint8_t DISPLAY_QSPI_SCK_PIN = 17;
static constexpr uint8_t DISPLAY_QSPI_D0_PIN = 13;
static constexpr uint8_t DISPLAY_QSPI_D1_PIN = 18;
static constexpr uint8_t DISPLAY_QSPI_D2_PIN = 21;
static constexpr uint8_t DISPLAY_QSPI_D3_PIN = 14;
static constexpr uint8_t TOUCH_I2C_ADDRESS = 0x3B;
static constexpr bool TOUCH_CALIBRATION_DEFAULT_VALID = true;
static constexpr bool TOUCH_CALIBRATION_DEFAULT_SWAP_XY = false;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_X1 = 714;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_X2 = -87;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_Y1 = 170;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_Y2 = -11;
#else
static constexpr uint8_t I2C_SDA_PIN = 21;
static constexpr uint8_t I2C_SCL_PIN = 22;
static constexpr uint8_t DISPLAY_ROTATION = 1;
static constexpr uint16_t DISPLAY_WIDTH = 320;
static constexpr uint16_t DISPLAY_HEIGHT = 170;
static constexpr uint8_t DISPLAY_BACKLIGHT_PIN = 0;
static constexpr uint8_t DISPLAY_RESET_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_CS_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_SCK_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_D0_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_D1_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_D2_PIN = 0;
static constexpr uint8_t DISPLAY_QSPI_D3_PIN = 0;
static constexpr uint8_t TOUCH_I2C_ADDRESS = 0x00;
static constexpr bool TOUCH_CALIBRATION_DEFAULT_VALID = false;
static constexpr bool TOUCH_CALIBRATION_DEFAULT_SWAP_XY = false;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_X1 = 0;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_X2 = 0;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_Y1 = 0;
static constexpr int16_t TOUCH_CALIBRATION_DEFAULT_MAP_Y2 = 0;
#endif

static constexpr unsigned long SENSOR_READ_INTERVAL_MS = 3000;
static constexpr unsigned long RENDER_INTERVAL_MS = 800;
static constexpr unsigned long INPUT_POLL_MS = 24;

// Touch UX tuning (adjust after real hardware testing)
static constexpr uint16_t TOUCH_SWIPE_DISTANCE_PX = 26;
static constexpr uint16_t TOUCH_TAP_MOVEMENT_PX = 14;
static constexpr unsigned long TOUCH_TAP_MAX_MS = 240;
static constexpr unsigned long TOUCH_SWIPE_MAX_MS = 700;
static constexpr unsigned long TOUCH_LONG_PRESS_MS = 720;
// Enable runtime touch-debug logs via serial 'd'
static constexpr bool TOUCH_DEBUG_DEFAULT = false;

static constexpr uint32_t FOCUS_DEFAULT_SECONDS = 25 * 60;
static constexpr uint32_t FOCUS_BREAK_DEFAULT_SECONDS = 5 * 60;
static constexpr uint32_t TIMER_DEFAULT_SECONDS = 5 * 60;

static constexpr unsigned long STATUS_ROTATE_INTERVAL_MS = 2500;
}
