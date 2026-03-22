#include "focus_renderer.h"

#include <stdio.h>

#include "display_manager.h"

namespace {

constexpr uint8_t kProgressWidth = 40;

uint32_t clampUiValue(uint32_t value, uint32_t minValue, uint32_t maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

uint32_t msToSec(uint32_t ms) {
    return (ms + 500) / 1000;
}

void formatMmSs(uint32_t sec, char* out, size_t outSize) {
    snprintf(out, outSize, "%02lu:%02lu", static_cast<unsigned long>(sec / 60), static_cast<unsigned long>(sec % 60));
}

const char* statusLabel(bool running, bool breakMode, bool complete, bool active) {
    if (complete) {
        return "COMPLETE";
    }
    if (active && !running) {
        return "PAUSED";
    }
    if (breakMode) {
        return "BREAK";
    }
    if (running) {
        return "FOCUS";
    }
    return "READY";
}

uint16_t statusColor(bool running, bool breakMode, bool complete, bool active) {
    constexpr uint16_t kFocus = 0x07E0;
    constexpr uint16_t kBreak = 0x001F;
    constexpr uint16_t kPause = 0xFFE0;
    constexpr uint16_t kReady = 0xFFFF;

    if (complete) {
        return kFocus;
    }
    if (active && !running) {
        return kPause;
    }
    if (breakMode) {
        return kBreak;
    }
    if (running) {
        return kFocus;
    }
    return kReady;
}

void printProgressFill(uint8_t width, uint32_t totalMs, uint32_t remainingMs) {
    if (width == 0) {
        return;
    }

    uint8_t filled = 0;
    if (totalMs > 0) {
        const uint32_t scaled = (remainingMs * width) / totalMs;
        filled = static_cast<uint8_t>(clampUiValue(scaled, 0, width));
    }

    for (uint8_t i = 0; i < width; i++) {
        Serial.print(i < filled ? "#" : "-");
    }
}

void printLabeledProgressBar(uint8_t totalWidth,
                             uint32_t focusMs,
                             uint32_t focusRemainMs,
                             uint32_t breakMs,
                             uint32_t breakRemainMs) {
    uint8_t focusWidth = totalWidth / 2;
    uint8_t breakWidth = totalWidth - focusWidth;

    if (focusMs + breakMs > 0) {
        const uint32_t total = focusMs + breakMs;
        uint32_t targetFocus = (static_cast<uint32_t>(totalWidth) * focusMs) / total;
        focusWidth = static_cast<uint8_t>(clampUiValue(targetFocus, 1, totalWidth - 1));
        breakWidth = static_cast<uint8_t>(totalWidth - focusWidth);
        if (breakWidth == 0) {
            breakWidth = 1;
            if (focusWidth > 1) {
                focusWidth--;
            }
        }
    }

    Serial.print("[");
    printProgressFill(focusWidth, focusMs, focusRemainMs);
    Serial.print("][");
    printProgressFill(breakWidth, breakMs, breakRemainMs);
    Serial.print("]");
}

void drawProgressBar(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, float ratio, uint16_t color) {
    if (gfx == nullptr) {
        return;
    }

    if (ratio < 0.0f) {
        ratio = 0.0f;
    }
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    gfx->drawRoundRect(x, y, w, h, 4, color);
    gfx->fillRoundRect(x + 1, y + 1, w - 2, h - 2, 4, 0x18C3);
    const int16_t fill = static_cast<int16_t>((w - 4) * ratio);
    if (fill > 0) {
        gfx->fillRoundRect(x + 2, y + 2, fill, h - 4, 3, color);
    }
}

void renderFocusDisplay(const ui::CountdownState& focusState,
                        const ui::CountdownState& breakState,
                        bool running,
                        bool breakMode,
                        bool complete,
                        bool active) {
    if (!display::isReady()) {
        return;
    }

    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    constexpr uint16_t kBg = 0x0000;
    constexpr uint16_t kFg = 0xFFFF;
    constexpr uint16_t kDim = 0x8410;
    constexpr uint16_t kFocus = 0x07E0;
    constexpr uint16_t kBreak = 0x001F;
    char buf[24];
    const bool compact = display::height() <= 200;
    const char* status = statusLabel(running, breakMode, complete, active);
    const uint16_t statusFg = statusColor(running, breakMode, complete, active);

    gfx->fillScreen(kBg);
    display::drawTextCentered("FOCUS", display::width() / 2, compact ? 8 : 20, kFg, compact ? 1 : 2, kBg);
    display::drawTextCentered(status, display::width() / 2, compact ? 24 : 56, statusFg, 2, kBg);

    if (compact) {
        const int16_t margin = 10;
        const int16_t gap = 10;
        const int16_t cardW = static_cast<int16_t>((display::width() - (margin * 2) - gap) / 2);
        const int16_t cardH = 84;
        const int16_t topY = 54;
        const int16_t leftX = margin;
        const int16_t rightX = static_cast<int16_t>(leftX + cardW + gap);

        gfx->drawRoundRect(leftX, topY, cardW, cardH, 8, breakMode ? kDim : kFocus);
        display::drawText(leftX + 10, topY + 10, "FOCUS", breakMode ? kDim : kFocus, 1, kBg);
        formatMmSs(msToSec(focusState.remainingMs), buf, sizeof(buf));
        display::drawTextCentered(buf, leftX + cardW / 2, topY + 26, breakMode ? kDim : kFg, 3, kBg);
        drawProgressBar(gfx, leftX + 10, topY + 62, cardW - 20, 12,
                        focusState.totalMs > 0 ? static_cast<float>(focusState.remainingMs) / static_cast<float>(focusState.totalMs) : 0.0f,
                        breakMode ? kDim : kFocus);

        gfx->drawRoundRect(rightX, topY, cardW, cardH, 8, breakMode ? kBreak : kDim);
        display::drawText(rightX + 10, topY + 10, "BREAK", breakMode ? kBreak : kDim, 1, kBg);
        formatMmSs(msToSec(breakState.remainingMs), buf, sizeof(buf));
        display::drawTextCentered(buf, rightX + cardW / 2, topY + 26, breakMode ? kFg : kDim, 3, kBg);
        drawProgressBar(gfx, rightX + 10, topY + 62, cardW - 20, 12,
                        breakState.totalMs > 0 ? static_cast<float>(breakState.remainingMs) / static_cast<float>(breakState.totalMs) : 0.0f,
                        breakMode ? kBreak : kDim);

        display::drawTextCentered("tap settings/pause   long reset", display::width() / 2, 146, kDim, 1, kBg);
        display::drawTextCentered("swipe up/down", display::width() / 2, 160, kDim, 1, kBg);
        return;
    }

    gfx->drawRoundRect(10, 106, display::width() - 20, 120, 8, breakMode ? kDim : kFocus);
    display::drawText(20, 122, "FOCUS BLOCK", breakMode ? kDim : kFocus, 1, kBg);
    formatMmSs(msToSec(focusState.remainingMs), buf, sizeof(buf));
    display::drawTextCentered(buf, display::width() / 2, 154, breakMode ? kDim : kFg, 3, kBg);
    drawProgressBar(gfx, 20, 198, display::width() - 40, 14,
                    focusState.totalMs > 0 ? static_cast<float>(focusState.remainingMs) / static_cast<float>(focusState.totalMs) : 0.0f,
                    breakMode ? kDim : kFocus);

    gfx->drawRoundRect(10, 264, display::width() - 20, 120, 8, breakMode ? kBreak : kDim);
    display::drawText(20, 280, "BREAK BLOCK", breakMode ? kBreak : kDim, 1, kBg);
    formatMmSs(msToSec(breakState.remainingMs), buf, sizeof(buf));
    display::drawTextCentered(buf, display::width() / 2, 312, breakMode ? kFg : kDim, 3, kBg);
    drawProgressBar(gfx, 20, 356, display::width() - 40, 14,
                    breakState.totalMs > 0 ? static_cast<float>(breakState.remainingMs) / static_cast<float>(breakState.totalMs) : 0.0f,
                    breakMode ? kBreak : kDim);

    display::drawTextCentered("tap: settings/pause  long: reset", display::width() / 2, 440, kDim, 1, kBg);
    display::drawTextCentered("swipe up: timer  swipe down: home", display::width() / 2, 458, kDim, 1, kBg);
}

void printMmSs(uint32_t sec, bool compact = false) {
    uint32_t min = sec / 60;
    uint32_t sec2 = sec % 60;
    if (!compact && min < 10) {
        Serial.print(' ');
    }
    if (min < 10) Serial.print('0');
    Serial.print(min);
    Serial.print(':');
    if (sec2 < 10) Serial.print('0');
    Serial.print(sec2);
}

}  // namespace

void FocusRenderer::render(const ui::CountdownState& focusState,
                           const ui::CountdownState& breakState,
                           bool running,
                           bool breakMode,
                           bool complete,
                           bool active) {
    if (display::isReady()) {
        renderFocusDisplay(focusState, breakState, running, breakMode, complete, active);
        return;
    }

    Serial.println();
    Serial.println("======= FOCUS ========");
    Serial.println("Focus                    Break");

    const uint32_t focusRemainSec = msToSec(focusState.remainingMs);
    const uint32_t breakRemainSec = msToSec(breakState.remainingMs);
    Serial.print("  ");
    printMmSs(focusRemainSec, true);
    Serial.print("                    ");
    printMmSs(breakRemainSec, true);
    Serial.println();

    printLabeledProgressBar(kProgressWidth, focusState.totalMs, focusState.remainingMs, breakState.totalMs, breakState.remainingMs);
    Serial.println();

    Serial.print("Status: ");
    Serial.println(statusLabel(running, breakMode, complete, active));
    Serial.println();
    Serial.println("====================");
}
