#include "display_manager.h"

#include "config/app_config.h"
#include "drivers/axs15231_lilygo.h"

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
#include <Arduino_GFX_Library.h>
#endif

namespace {

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
Arduino_Canvas* gCanvas = nullptr;
Arduino_GFX* gSurface = nullptr;
uint16_t* gNativeFrame = nullptr;
bool gDisplayReady = false;

constexpr uint16_t swapColorBytes(uint16_t color) {
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

void rotateLogicalToNative(const uint16_t* logicalFrame,
                           uint16_t logicalWidth,
                           uint16_t logicalHeight,
                           uint16_t* nativeFrame) {
    if (logicalFrame == nullptr || nativeFrame == nullptr) {
        return;
    }

    const uint8_t rotation = static_cast<uint8_t>(app::DISPLAY_ROTATION & 0x03);
    const uint16_t nativeWidth = app::DISPLAY_WIDTH;
    const uint16_t nativeHeight = app::DISPLAY_HEIGHT;

    for (uint16_t y = 0; y < logicalHeight; ++y) {
        for (uint16_t x = 0; x < logicalWidth; ++x) {
            const uint16_t pixel = swapColorBytes(
                logicalFrame[static_cast<size_t>(y) * logicalWidth + x]);
            uint16_t targetX = 0;
            uint16_t targetY = 0;

            switch (rotation) {
                case 1:
                    targetX = y;
                    targetY = static_cast<uint16_t>(nativeHeight - 1 - x);
                    break;
                case 3:
                    targetX = static_cast<uint16_t>(nativeWidth - 1 - y);
                    targetY = x;
                    break;
                case 2:
                    targetX = static_cast<uint16_t>(nativeWidth - 1 - x);
                    targetY = static_cast<uint16_t>(nativeHeight - 1 - y);
                    break;
                case 0:
                default:
                    targetX = x;
                    targetY = y;
                    break;
            }

            nativeFrame[static_cast<size_t>(targetY) * nativeWidth + targetX] = pixel;
        }
    }
}
#endif

}  // namespace

namespace display {

bool begin() {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    pinMode(app::DISPLAY_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(app::DISPLAY_BACKLIGHT_PIN, LOW);

    if (!lilygo_display::begin()) {
        Serial.println("Display init failed.");
        return false;
    }

    const int16_t surfaceWidth = static_cast<int16_t>((app::DISPLAY_ROTATION & 0x01) ? app::DISPLAY_HEIGHT : app::DISPLAY_WIDTH);
    const int16_t surfaceHeight = static_cast<int16_t>((app::DISPLAY_ROTATION & 0x01) ? app::DISPLAY_WIDTH : app::DISPLAY_HEIGHT);

    if (gCanvas == nullptr) {
        gCanvas = new Arduino_Canvas(surfaceWidth, surfaceHeight, nullptr);
    }
    if (gNativeFrame == nullptr) {
        gNativeFrame = static_cast<uint16_t*>(ps_malloc(static_cast<size_t>(app::DISPLAY_WIDTH) *
                                                        static_cast<size_t>(app::DISPLAY_HEIGHT) *
                                                        sizeof(uint16_t)));
    }

    if (gCanvas != nullptr && gNativeFrame != nullptr && gCanvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
        gCanvas->fillScreen(0x0000);
        gCanvas->setTextWrap(false);
        gSurface = gCanvas;
        Serial.printf("Display canvas enabled: %ux%u\n",
                      static_cast<unsigned>(gCanvas->width()),
                      static_cast<unsigned>(gCanvas->height()));
    } else {
        Serial.println("Display canvas/native frame init failed.");
        return false;
    }

    digitalWrite(app::DISPLAY_BACKLIGHT_PIN, HIGH);
    gDisplayReady = true;
    Serial.printf("Display ready: rotation=%u size=%ux%u\n",
                  static_cast<unsigned>(app::DISPLAY_ROTATION & 0x03),
                  static_cast<unsigned>(surfaceWidth),
                  static_cast<unsigned>(surfaceHeight));
    return true;
#else
    return false;
#endif
}

bool isReady() {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    return gDisplayReady;
#else
    return false;
#endif
}

Arduino_GFX* gfx() {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    return gDisplayReady ? gSurface : nullptr;
#else
    return nullptr;
#endif
}

void present() {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    if (!gDisplayReady || gCanvas == nullptr) {
        return;
    }
    rotateLogicalToNative(gCanvas->getFramebuffer(),
                          static_cast<uint16_t>(gCanvas->width()),
                          static_cast<uint16_t>(gCanvas->height()),
                          gNativeFrame);
    lilygo_display::present(gNativeFrame, app::DISPLAY_WIDTH, app::DISPLAY_HEIGHT);
#else
    return;
#endif
}

uint16_t width() {
    Arduino_GFX* display = gfx();
    return display == nullptr ? 0 : static_cast<uint16_t>(display->width());
}

uint16_t height() {
    Arduino_GFX* display = gfx();
    return display == nullptr ? 0 : static_cast<uint16_t>(display->height());
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void clear(uint16_t color) {
    Arduino_GFX* display = gfx();
    if (display == nullptr) {
        return;
    }
    display->fillScreen(color);
}

int16_t textWidth(const char* text, uint8_t size) {
    if (text == nullptr) {
        return 0;
    }

    int16_t width = 0;
    while (*text != '\0') {
        width += static_cast<int16_t>(6 * size);
        ++text;
    }
    return width;
}

int16_t textHeight(uint8_t size) {
    return static_cast<int16_t>(8 * size);
}

void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size, uint16_t bg) {
    Arduino_GFX* display = gfx();
    if (display == nullptr) {
        return;
    }

    display->setTextSize(size);
    display->setTextColor(color, bg);
    display->setCursor(x, y);
    display->print(text == nullptr ? "" : text);
}

void drawTextCentered(const char* text, int16_t centerX, int16_t y, uint16_t color, uint8_t size, uint16_t bg) {
    drawText(static_cast<int16_t>(centerX - (textWidth(text, size) / 2)), y, text, color, size, bg);
}

void drawTextRight(const char* text, int16_t rightX, int16_t y, uint16_t color, uint8_t size, uint16_t bg) {
    drawText(static_cast<int16_t>(rightX - textWidth(text, size)), y, text, color, size, bg);
}

}  // namespace display
