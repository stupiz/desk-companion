#include "timer_renderer.h"

#include <stdio.h>

#include "display_manager.h"

namespace {

void printProgressBar(float ratio) {
    const uint8_t width = 20;
    uint8_t filled = static_cast<uint8_t>(width * ratio);
    Serial.print("[");
    for (uint8_t i = 0; i < width; i++) {
        Serial.print(i < filled ? "#" : ".");
    }
    Serial.print("]");
}

uint32_t msToSec(uint32_t ms) {
    return (ms + 500) / 1000;
}

void formatTimerClock(uint32_t sec, char* out, size_t outSize) {
    const unsigned long hours = static_cast<unsigned long>(sec / 3600UL);
    const unsigned long minutes = static_cast<unsigned long>((sec / 60UL) % 60UL);
    const unsigned long seconds = static_cast<unsigned long>(sec % 60UL);
    if (hours > 0) {
        snprintf(out, outSize, "%02lu:%02lu:%02lu", hours, minutes, seconds);
        return;
    }
    snprintf(out, outSize, "%02lu:%02lu", minutes, seconds);
}

const char* timerStateLabel(const ui::CountdownState& state) {
    if (state.running) {
        return "RUNNING";
    }
    if (state.remainingMs > 0) {
        return "PAUSED";
    }
    if (state.totalMs == 0) {
        return "READY";
    }
    return "COMPLETE";
}

uint16_t timerStateColor(const ui::CountdownState& state) {
    constexpr uint16_t kRunning = 0xFFE0;
    constexpr uint16_t kPaused = 0xFD20;
    constexpr uint16_t kReady = 0xFFFF;
    constexpr uint16_t kComplete = 0x07E0;

    if (state.running) {
        return kRunning;
    }
    if (state.remainingMs > 0) {
        return kPaused;
    }
    if (state.totalMs == 0) {
        return kReady;
    }
    return kComplete;
}

void renderTimerDisplay(const ui::CountdownState& state) {
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
    constexpr uint16_t kPanel = 0x1082;
    char line[24];
    char totalLine[32];
    const bool compact = display::height() <= 200;

    const uint32_t remainSec = msToSec(state.remainingMs);
    const uint32_t totalSec = msToSec(state.totalMs);
    const float ratio = state.totalMs > 0
        ? static_cast<float>(state.remainingMs) / static_cast<float>(state.totalMs)
        : 0.0f;
    const char* status = timerStateLabel(state);
    const uint16_t accent = timerStateColor(state);

    formatTimerClock(remainSec, line, sizeof(line));
    formatTimerClock(totalSec, totalLine, sizeof(totalLine));

    gfx->fillScreen(kBg);
    display::drawTextCentered("TIMER", display::width() / 2, compact ? 8 : 26, kFg, compact ? 1 : 2, kBg);
    display::drawTextCentered(status, display::width() / 2, compact ? 24 : 62, accent, 2, kBg);

    if (compact) {
        const int16_t cardX = 12;
        const int16_t cardY = 48;
        const int16_t cardW = static_cast<int16_t>(display::width() - 24);
        const int16_t cardH = 84;

        gfx->fillRoundRect(cardX, cardY, cardW, cardH, 10, kPanel);
        gfx->drawRoundRect(cardX, cardY, cardW, cardH, 10, state.totalMs > 0 ? accent : kDim);
        display::drawText(cardX + 12, cardY + 10, "COUNTDOWN", state.totalMs > 0 ? accent : kDim, 1, kPanel);
        display::drawTextCentered(line, static_cast<int16_t>(cardX + (cardW / 2)), cardY + 20, kFg, 4, kPanel);

        const int16_t barX = static_cast<int16_t>(cardX + 14);
        const int16_t barY = static_cast<int16_t>(cardY + 58);
        const int16_t barW = static_cast<int16_t>(cardW - 28);
        gfx->drawRoundRect(barX, barY, barW, 12, 4, state.totalMs > 0 ? accent : kDim);
        gfx->fillRoundRect(barX + 1, barY + 1, barW - 2, 10, 4, 0x18C3);
        const int16_t compactFill = static_cast<int16_t>((barW - 4) * ratio);
        if (compactFill > 0) {
            gfx->fillRoundRect(barX + 2, barY + 2, compactFill, 8, 3, accent);
        }

        if (state.totalMs > 0) {
            snprintf(line, sizeof(line), "TOTAL %s", totalLine);
        } else {
            snprintf(line, sizeof(line), "tap to set a duration");
        }
        display::drawTextCentered(line, display::width() / 2, 138, kDim, 1, kBg);
        display::drawTextCentered("tap settings/pause   long reset", display::width() / 2, 154, kDim, 1, kBg);
        display::drawTextCentered("swipe up status   swipe down focus", display::width() / 2, 166, kDim, 1, kBg);
        return;
    }

    const int16_t cardX = 18;
    const int16_t cardY = 118;
    const int16_t cardW = static_cast<int16_t>(display::width() - 36);
    const int16_t cardH = 214;
    gfx->fillRoundRect(cardX, cardY, cardW, cardH, 10, kPanel);
    gfx->drawRoundRect(cardX, cardY, cardW, cardH, 10, state.totalMs > 0 ? accent : kDim);
    display::drawText(cardX + 16, cardY + 16, "COUNTDOWN", state.totalMs > 0 ? accent : kDim, 1, kPanel);
    display::drawTextCentered(line, display::width() / 2, cardY + 58, kFg, 4, kPanel);

    const int16_t barX = static_cast<int16_t>(cardX + 20);
    const int16_t barY = static_cast<int16_t>(cardY + 118);
    const int16_t barW = static_cast<int16_t>(cardW - 40);
    gfx->drawRoundRect(barX, barY, barW, 16, 6, state.totalMs > 0 ? accent : kDim);
    gfx->fillRoundRect(barX + 1, barY + 1, barW - 2, 14, 6, 0x18C3);
    const int16_t fill = static_cast<int16_t>((barW - 4) * ratio);
    if (fill > 0) {
        gfx->fillRoundRect(barX + 2, barY + 2, fill, 12, 4, accent);
    }

    if (state.totalMs > 0) {
        snprintf(line, sizeof(line), "TOTAL %s", totalLine);
    } else {
        snprintf(line, sizeof(line), "tap to open timer setup");
    }
    display::drawTextCentered(line, display::width() / 2, cardY + 150, kDim, 2, kPanel);
    display::drawTextCentered("tap: settings/pause  long: reset", display::width() / 2, 392, kDim, 1, kBg);
    display::drawTextCentered("swipe up: status  swipe down: focus", display::width() / 2, 408, kDim, 1, kBg);
}

}  // namespace

void TimerRenderer::render(const ui::CountdownState& state) {
    if (display::isReady()) {
        renderTimerDisplay(state);
        return;
    }

    Serial.println();
    Serial.println("======= TIMER =======");

    Serial.print("State: ");
    Serial.println(timerStateLabel(state));

    uint32_t remainSec = msToSec(state.remainingMs);
    uint32_t totalSec = msToSec(state.totalMs);
    float ratio = 0.0f;
    if (state.totalMs > 0) {
        ratio = static_cast<float>(state.remainingMs) / static_cast<float>(state.totalMs);
    }

    Serial.print("Remaining: ");
    char remainBuf[24];
    char totalBuf[24];
    formatTimerClock(remainSec, remainBuf, sizeof(remainBuf));
    formatTimerClock(totalSec, totalBuf, sizeof(totalBuf));
    Serial.print(remainBuf);
    Serial.print("  | Total: ");
    Serial.println(totalBuf);

    printProgressBar(ratio);
    Serial.println();
    Serial.println("====================");
}
