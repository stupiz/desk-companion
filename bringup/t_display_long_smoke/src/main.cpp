#include <Arduino.h>
#include <esp_heap_caps.h>

#include "panel_driver.h"

namespace {

enum class RotationMode : uint8_t {
    Clockwise = 0,
    CounterClockwise = 1,
};

uint16_t* gLandscape = nullptr;
uint16_t* gPortrait = nullptr;
uint8_t gPattern = 1;
RotationMode gRotationMode = RotationMode::Clockwise;
bool gDirty = true;

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void clearLandscape(uint16_t color) {
    const size_t total = static_cast<size_t>(smoke_panel::kLogicalWidth) * smoke_panel::kLogicalHeight;
    for (size_t i = 0; i < total; ++i) {
        gLandscape[i] = color;
    }
}

void fillRectLandscape(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    const int16_t x0 = x < 0 ? 0 : x;
    const int16_t y0 = y < 0 ? 0 : y;
    const int16_t x1 = (x + w) > smoke_panel::kLogicalWidth ? smoke_panel::kLogicalWidth : (x + w);
    const int16_t y1 = (y + h) > smoke_panel::kLogicalHeight ? smoke_panel::kLogicalHeight : (y + h);

    for (int16_t yy = y0; yy < y1; ++yy) {
        uint16_t* row = gLandscape + (static_cast<size_t>(yy) * smoke_panel::kLogicalWidth);
        for (int16_t xx = x0; xx < x1; ++xx) {
            row[xx] = color;
        }
    }
}

void drawFrameLandscape(int16_t x, int16_t y, int16_t w, int16_t h, int16_t thickness, uint16_t color) {
    fillRectLandscape(x, y, w, thickness, color);
    fillRectLandscape(x, y + h - thickness, w, thickness, color);
    fillRectLandscape(x, y, thickness, h, color);
    fillRectLandscape(x + w - thickness, y, thickness, h, color);
}

void drawCheckerLandscape(int16_t x, int16_t y, int16_t w, int16_t h, int16_t cell, uint16_t a, uint16_t b) {
    for (int16_t yy = 0; yy < h; ++yy) {
        for (int16_t xx = 0; xx < w; ++xx) {
            const bool even = (((xx / cell) + (yy / cell)) & 1) == 0;
            fillRectLandscape(x + xx, y + yy, 1, 1, even ? a : b);
        }
    }
}

void drawCrosshairLandscape() {
    const int16_t cx = smoke_panel::kLogicalWidth / 2;
    const int16_t cy = smoke_panel::kLogicalHeight / 2;
    fillRectLandscape(cx - 2, 24, 4, smoke_panel::kLogicalHeight - 48, color565(255, 255, 255));
    fillRectLandscape(24, cy - 2, smoke_panel::kLogicalWidth - 48, 4, color565(255, 255, 255));
}

void renderPattern() {
    switch (gPattern) {
        case 1:
        default:
            clearLandscape(color565(10, 10, 10));
            drawFrameLandscape(0, 0, smoke_panel::kLogicalWidth, smoke_panel::kLogicalHeight, 4, color565(255, 255, 255));
            fillRectLandscape(0, 0, 72, 72, color565(255, 48, 48));
            fillRectLandscape(smoke_panel::kLogicalWidth - 72, 0, 72, 72, color565(48, 255, 48));
            fillRectLandscape(0, smoke_panel::kLogicalHeight - 72, 72, 72, color565(48, 96, 255));
            fillRectLandscape(smoke_panel::kLogicalWidth - 72, smoke_panel::kLogicalHeight - 72, 72, 72, color565(255, 220, 32));
            fillRectLandscape(96, 24, smoke_panel::kLogicalWidth - 192, 16, color565(255, 64, 64));
            fillRectLandscape(96, smoke_panel::kLogicalHeight - 40, smoke_panel::kLogicalWidth - 192, 16, color565(64, 96, 255));
            fillRectLandscape(24, 48, 16, smoke_panel::kLogicalHeight - 96, color565(64, 255, 160));
            fillRectLandscape(smoke_panel::kLogicalWidth - 40, 48, 16, smoke_panel::kLogicalHeight - 96, color565(255, 200, 32));
            drawCrosshairLandscape();
            break;
        case 2:
            clearLandscape(color565(0, 0, 0));
            fillRectLandscape(0, 0, smoke_panel::kLogicalWidth / 2, smoke_panel::kLogicalHeight / 2, color565(255, 0, 0));
            fillRectLandscape(smoke_panel::kLogicalWidth / 2, 0, smoke_panel::kLogicalWidth / 2, smoke_panel::kLogicalHeight / 2, color565(0, 255, 0));
            fillRectLandscape(0, smoke_panel::kLogicalHeight / 2, smoke_panel::kLogicalWidth / 2, smoke_panel::kLogicalHeight / 2, color565(0, 0, 255));
            fillRectLandscape(smoke_panel::kLogicalWidth / 2, smoke_panel::kLogicalHeight / 2, smoke_panel::kLogicalWidth / 2, smoke_panel::kLogicalHeight / 2, color565(255, 255, 0));
            drawFrameLandscape(0, 0, smoke_panel::kLogicalWidth, smoke_panel::kLogicalHeight, 4, color565(255, 255, 255));
            break;
        case 3: {
            clearLandscape(color565(0, 0, 0));
            const uint16_t bars[] = {
                color565(255, 255, 255),
                color565(255, 255, 0),
                color565(0, 255, 255),
                color565(0, 255, 0),
                color565(255, 0, 255),
                color565(255, 0, 0),
                color565(0, 0, 255),
                color565(32, 32, 32),
            };
            const int16_t barWidth = smoke_panel::kLogicalWidth / 8;
            for (int i = 0; i < 8; ++i) {
                fillRectLandscape(i * barWidth, 0, barWidth, smoke_panel::kLogicalHeight, bars[i]);
            }
            break;
        }
        case 4:
            clearLandscape(color565(0, 0, 0));
            drawCheckerLandscape(0, 0, smoke_panel::kLogicalWidth, smoke_panel::kLogicalHeight, 10,
                                 color565(255, 255, 255), color565(0, 0, 0));
            break;
        case 5:
            clearLandscape(color565(255, 255, 255));
            break;
        case 6:
            clearLandscape(color565(0, 0, 0));
            break;
    }
}

void rotateLandscapeToPortrait() {
    for (uint16_t y = 0; y < smoke_panel::kLogicalHeight; ++y) {
        for (uint16_t x = 0; x < smoke_panel::kLogicalWidth; ++x) {
            const uint16_t pixel = gLandscape[static_cast<size_t>(y) * smoke_panel::kLogicalWidth + x];
            uint16_t targetX = 0;
            uint16_t targetY = 0;

            if (gRotationMode == RotationMode::Clockwise) {
                targetX = y;
                targetY = static_cast<uint16_t>(smoke_panel::kPanelHeight - 1 - x);
            } else {
                targetX = static_cast<uint16_t>(smoke_panel::kPanelWidth - 1 - y);
                targetY = x;
            }

            gPortrait[static_cast<size_t>(targetY) * smoke_panel::kPanelWidth + targetX] = pixel;
        }
    }
}

void present() {
    renderPattern();
    rotateLandscapeToPortrait();
    smoke_panel::pushNative(gPortrait, smoke_panel::kPanelWidth, smoke_panel::kPanelHeight);
    Serial.printf("Rendered pattern=%u rotation=%s\n",
                  static_cast<unsigned>(gPattern),
                  gRotationMode == RotationMode::Clockwise ? "CW" : "CCW");
}

void printHelp() {
    Serial.println();
    Serial.println("T-Display S3 Long Smoke Test");
    Serial.println("1: orientation frame");
    Serial.println("2: 4-color quadrants");
    Serial.println("3: color bars");
    Serial.println("4: checkerboard");
    Serial.println("5: full white");
    Serial.println("6: full black");
    Serial.println("r: toggle software rotation (CW/CCW)");
    Serial.println("p: redraw current pattern");
    Serial.println("h or ?: help");
    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Smoke test booting...");

    if (!psramFound()) {
        Serial.println("PSRAM not found.");
    }

    if (!smoke_panel::begin()) {
        Serial.println("Panel init failed.");
        return;
    }

    gLandscape = static_cast<uint16_t*>(ps_malloc(static_cast<size_t>(smoke_panel::kLogicalWidth) *
                                                  smoke_panel::kLogicalHeight * sizeof(uint16_t)));
    gPortrait = static_cast<uint16_t*>(ps_malloc(static_cast<size_t>(smoke_panel::kPanelWidth) *
                                                 smoke_panel::kPanelHeight * sizeof(uint16_t)));
    if (gLandscape == nullptr || gPortrait == nullptr) {
        Serial.println("Framebuffer allocation failed.");
        return;
    }

    printHelp();
    present();
}

void loop() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        switch (c) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
                gPattern = static_cast<uint8_t>(c - '0');
                gDirty = true;
                break;
            case 'r':
            case 'R':
                gRotationMode = gRotationMode == RotationMode::Clockwise
                    ? RotationMode::CounterClockwise
                    : RotationMode::Clockwise;
                gDirty = true;
                break;
            case 'p':
            case 'P':
                gDirty = true;
                break;
            case 'h':
            case 'H':
            case '?':
                printHelp();
                break;
        }
    }

    if (gDirty) {
        gDirty = false;
        present();
    }

    delay(10);
}
