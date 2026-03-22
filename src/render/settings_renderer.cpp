#include "settings_renderer.h"

#include <stdio.h>

#include "display_manager.h"

namespace {

struct FocusCardsLayout {
    bool compact;
    int16_t cardY;
    int16_t cardW;
    int16_t cardH;
    int16_t gap;
    int16_t focusX;
    int16_t breakX;
    int16_t startY;
    int16_t startW;
    int16_t startH;
};

struct TimerCardsLayout {
    bool compact;
    int16_t durationX;
    int16_t durationY;
    int16_t durationW;
    int16_t durationH;
    int16_t startY;
    int16_t startW;
    int16_t startH;
};

const char* kFocusSettingLabels[] = {
    "FOCUS DURATION",
    "BREAK DURATION",
    "START",
};

const char* kTimerSettingLabels[] = {
    "TIMER DURATION",
    "START",
};

FocusCardsLayout focusCardsLayoutForDisplay() {
    const bool compact = display::height() <= 200;
    FocusCardsLayout layout{};
    layout.compact = compact;
    layout.cardY = compact ? 34 : 128;
    layout.gap = compact ? 12 : 24;
    const int16_t outerMargin = compact ? 14 : 32;
    layout.cardW = static_cast<int16_t>((display::width() - (outerMargin * 2) - layout.gap) / 2);
    layout.cardH = compact ? 78 : 120;
    layout.focusX = outerMargin;
    layout.breakX = static_cast<int16_t>(layout.focusX + layout.cardW + layout.gap);
    layout.startW = compact ? 320 : 260;
    layout.startH = compact ? 30 : 42;
    layout.startY = static_cast<int16_t>(layout.cardY + layout.cardH + (compact ? 16 : 36));
    return layout;
}

TimerCardsLayout timerCardsLayoutForDisplay() {
    const bool compact = display::height() <= 200;
    TimerCardsLayout layout{};
    layout.compact = compact;
    if (compact) {
        layout.durationX = 12;
        layout.durationY = 32;
        layout.durationW = static_cast<int16_t>(display::width() - 24);
        layout.durationH = 70;
        layout.startY = 120;
        layout.startW = 300;
        layout.startH = 28;
    } else {
        layout.durationX = 28;
        layout.durationY = 118;
        layout.durationW = static_cast<int16_t>(display::width() - 56);
        layout.durationH = 140;
        layout.startY = 294;
        layout.startW = 260;
        layout.startH = 40;
    }
    return layout;
}

bool containsPoint(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h) {
    return px >= x && py >= y && px < static_cast<int16_t>(x + w) && py < static_cast<int16_t>(y + h);
}

void printLineIndicator(uint8_t optionIndex, uint8_t selectedOption) {
    if (optionIndex == selectedOption) {
        Serial.print("> ");
    } else {
        Serial.print("  ");
    }
}

void formatHhMmSs(uint32_t seconds, char* out, size_t outSize) {
    const unsigned long hours = static_cast<unsigned long>(seconds / 3600UL);
    const unsigned long minutes = static_cast<unsigned long>((seconds / 60UL) % 60UL);
    const unsigned long secs = static_cast<unsigned long>(seconds % 60UL);
    snprintf(out, outSize, "%02lu:%02lu:%02lu", hours, minutes, secs);
}

void renderSettingsDisplay(const ui::SettingsState& settings, ui::AppPage page) {
    if (!display::isReady()) {
        return;
    }

    constexpr uint16_t kBg = 0x0000;
    constexpr uint16_t kFg = 0xFFFF;
    constexpr uint16_t kDim = 0x8410;
    constexpr uint16_t kAccent = 0x07E0;
    constexpr uint16_t kPanel = 0x1082;

    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    const bool compact = display::height() <= 200;

    gfx->fillScreen(kBg);

    if (page == ui::AppPage::FOCUS_SETTINGS) {
        const FocusCardsLayout cards = focusCardsLayoutForDisplay();
        const bool focusSelected = settings.selectedOption == static_cast<uint8_t>(ui::FocusSettingsOption::FOCUS_PRESET);
        const bool breakSelected = settings.selectedOption == static_cast<uint8_t>(ui::FocusSettingsOption::BREAK_PRESET);
        const bool startSelected = settings.selectedOption == static_cast<uint8_t>(ui::FocusSettingsOption::START);
        char focusValue[16];
        char breakValue[16];

        formatHhMmSs(settings.focusPresetSeconds, focusValue, sizeof(focusValue));
        formatHhMmSs(settings.focusBreakSeconds, breakValue, sizeof(breakValue));

        display::drawTextCentered("FOCUS SETUP", display::width() / 2, compact ? 6 : 18, kFg, compact ? 1 : 2, kBg);
        if (compact) {
            display::drawTextCentered("tap card to edit time", display::width() / 2, 18, kDim, 1, kBg);
        }

        gfx->fillRoundRect(cards.focusX, cards.cardY, cards.cardW, cards.cardH, compact ? 10 : 14, kPanel);
        gfx->drawRoundRect(cards.focusX, cards.cardY, cards.cardW, cards.cardH, compact ? 10 : 14, focusSelected ? kAccent : kDim);
        gfx->fillRoundRect(static_cast<int16_t>(cards.focusX + 8),
                           static_cast<int16_t>(cards.cardY + 8),
                           5,
                           static_cast<int16_t>(cards.cardH - 16),
                           2,
                           focusSelected ? kAccent : kDim);
        display::drawText(static_cast<int16_t>(cards.focusX + 20),
                          static_cast<int16_t>(cards.cardY + 8),
                          "FOCUS",
                          focusSelected ? kAccent : kFg,
                          1,
                          kPanel);
        display::drawTextCentered(focusValue,
                                  static_cast<int16_t>(cards.focusX + (cards.cardW / 2)),
                                  static_cast<int16_t>(cards.cardY + (compact ? 32 : 44)),
                                  kFg,
                                  compact ? 3 : 5,
                                  kPanel);

        gfx->fillRoundRect(cards.breakX, cards.cardY, cards.cardW, cards.cardH, compact ? 10 : 14, kPanel);
        gfx->drawRoundRect(cards.breakX, cards.cardY, cards.cardW, cards.cardH, compact ? 10 : 14, breakSelected ? kAccent : kDim);
        gfx->fillRoundRect(static_cast<int16_t>(cards.breakX + 8),
                           static_cast<int16_t>(cards.cardY + 8),
                           5,
                           static_cast<int16_t>(cards.cardH - 16),
                           2,
                           breakSelected ? kAccent : kDim);
        display::drawText(static_cast<int16_t>(cards.breakX + 20),
                          static_cast<int16_t>(cards.cardY + 8),
                          "BREAK",
                          breakSelected ? kAccent : kFg,
                          1,
                          kPanel);
        display::drawTextCentered(breakValue,
                                  static_cast<int16_t>(cards.breakX + (cards.cardW / 2)),
                                  static_cast<int16_t>(cards.cardY + (compact ? 32 : 44)),
                                  kFg,
                                  compact ? 3 : 5,
                                  kPanel);

        const int16_t startX = static_cast<int16_t>((display::width() - cards.startW) / 2);
        gfx->fillRoundRect(startX, cards.startY, cards.startW, cards.startH, compact ? 8 : 10, startSelected ? kAccent : kPanel);
        gfx->drawRoundRect(startX, cards.startY, cards.startW, cards.startH, compact ? 8 : 10, kAccent);
        display::drawTextCentered("START SESSION",
                                  static_cast<int16_t>(startX + (cards.startW / 2)),
                                  static_cast<int16_t>(cards.startY + (compact ? 8 : 12)),
                                  kFg,
                                  compact ? 2 : 3,
                                  startSelected ? kAccent : kPanel);
        return;
    }

    display::drawTextCentered("TIMER SETUP", display::width() / 2, compact ? 6 : 18, kFg, compact ? 1 : 2, kBg);
    if (page == ui::AppPage::TIMER_SETTINGS) {
        const TimerCardsLayout cards = timerCardsLayoutForDisplay();
        const bool durationSelected = settings.selectedOption == static_cast<uint8_t>(ui::TimerSettingsOption::TIMER_PRESET);
        const bool startSelected = settings.selectedOption == static_cast<uint8_t>(ui::TimerSettingsOption::START);
        char timeValue[16];
        formatHhMmSs(settings.timerPresetSeconds, timeValue, sizeof(timeValue));

        if (compact) {
            display::drawTextCentered("tap duration to edit", display::width() / 2, 18, kDim, 1, kBg);
        } else {
            display::drawTextCentered("tap duration to edit HH:MM:SS", display::width() / 2, 50, kDim, 1, kBg);
        }

        gfx->fillRoundRect(cards.durationX, cards.durationY, cards.durationW, cards.durationH, compact ? 10 : 14, kPanel);
        gfx->drawRoundRect(cards.durationX, cards.durationY, cards.durationW, cards.durationH, compact ? 10 : 14,
                           durationSelected ? kAccent : kDim);
        gfx->fillRoundRect(static_cast<int16_t>(cards.durationX + 8),
                           static_cast<int16_t>(cards.durationY + 8),
                           5,
                           static_cast<int16_t>(cards.durationH - 16),
                           2,
                           durationSelected ? kAccent : kDim);
        display::drawText(static_cast<int16_t>(cards.durationX + 20),
                          static_cast<int16_t>(cards.durationY + 8),
                          "DURATION",
                          durationSelected ? kAccent : kFg,
                          1,
                          kPanel);
        display::drawTextCentered(timeValue,
                                  static_cast<int16_t>(cards.durationX + (cards.durationW / 2)),
                                  static_cast<int16_t>(cards.durationY + (compact ? 28 : 52)),
                                  kFg,
                                  compact ? 3 : 5,
                                  kPanel);

        const int16_t startX = static_cast<int16_t>((display::width() - cards.startW) / 2);
        gfx->fillRoundRect(startX, cards.startY, cards.startW, cards.startH, compact ? 8 : 10, startSelected ? kAccent : kPanel);
        gfx->drawRoundRect(startX, cards.startY, cards.startW, cards.startH, compact ? 8 : 10, kAccent);
        display::drawTextCentered("START TIMER",
                                  static_cast<int16_t>(startX + (cards.startW / 2)),
                                  static_cast<int16_t>(cards.startY + (compact ? 6 : 10)),
                                  kFg,
                                  compact ? 2 : 3,
                                  startSelected ? kAccent : kPanel);
        return;
    }
}

}  // namespace

void SettingsRenderer::render(const ui::SettingsState& settings, ui::AppPage page) {
    if (display::isReady()) {
        renderSettingsDisplay(settings, page);
        return;
    }

    char value[32];
    Serial.println();
    Serial.println("====== SETTINGS =====");

    if (page == ui::AppPage::FOCUS_SETTINGS) {
        Serial.println("Mode: Focus");
        printLineIndicator(0, settings.selectedOption);
        Serial.print(kFocusSettingLabels[0]);
        Serial.print(": ");
        formatHhMmSs(settings.focusPresetSeconds, value, sizeof(value));
        Serial.println(value);

        printLineIndicator(1, settings.selectedOption);
        Serial.print(kFocusSettingLabels[1]);
        Serial.print(": ");
        formatHhMmSs(settings.focusBreakSeconds, value, sizeof(value));
        Serial.println(value);

        printLineIndicator(2, settings.selectedOption);
        Serial.print(kFocusSettingLabels[2]);
        Serial.println();
    } else {
        Serial.println("Mode: Timer");
        printLineIndicator(0, settings.selectedOption);
        Serial.print(kTimerSettingLabels[0]);
        Serial.print(": ");
        formatHhMmSs(settings.timerPresetSeconds, value, sizeof(value));
        Serial.println(value);
        printLineIndicator(1, settings.selectedOption);
        Serial.print(kTimerSettingLabels[1]);
        Serial.println();
    }

    if (page == ui::AppPage::FOCUS_SETTINGS) {
        Serial.println("Touch: tap focus/break card = edit | tap START = begin | swipe down = back");
    } else if (page == ui::AppPage::TIMER_SETTINGS) {
        Serial.println("Touch: tap duration = edit | tap START = begin | swipe down = back");
    }
    Serial.println("Serial: [/]: option | +/-: value | b: back");
}

SettingsHit SettingsRenderer::hitTest(int16_t x, int16_t y, ui::AppPage page) const {
    SettingsHit hit{};
    if (!display::isReady()) {
        return hit;
    }

    if (page == ui::AppPage::FOCUS_SETTINGS) {
        const FocusCardsLayout cards = focusCardsLayoutForDisplay();
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(cards.focusX - 8),
                          static_cast<int16_t>(cards.cardY - 6),
                          static_cast<int16_t>(cards.cardW + 16),
                          static_cast<int16_t>(cards.cardH + 12))) {
            hit.type = SettingsHitType::EditTime;
            hit.option = static_cast<uint8_t>(ui::FocusSettingsOption::FOCUS_PRESET);
            return hit;
        }
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(cards.breakX - 8),
                          static_cast<int16_t>(cards.cardY - 6),
                          static_cast<int16_t>(cards.cardW + 16),
                          static_cast<int16_t>(cards.cardH + 12))) {
            hit.type = SettingsHitType::EditTime;
            hit.option = static_cast<uint8_t>(ui::FocusSettingsOption::BREAK_PRESET);
            return hit;
        }
        const int16_t startX = static_cast<int16_t>((display::width() - cards.startW) / 2);
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(startX - 6),
                          static_cast<int16_t>(cards.startY - 4),
                          static_cast<int16_t>(cards.startW + 12),
                          static_cast<int16_t>(cards.startH + 8))) {
            hit.type = SettingsHitType::Start;
            hit.option = static_cast<uint8_t>(ui::FocusSettingsOption::START);
            return hit;
        }
        return hit;
    }

    if (page == ui::AppPage::TIMER_SETTINGS) {
        const TimerCardsLayout cards = timerCardsLayoutForDisplay();
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(cards.durationX - 8),
                          static_cast<int16_t>(cards.durationY - 6),
                          static_cast<int16_t>(cards.durationW + 16),
                          static_cast<int16_t>(cards.durationH + 12))) {
            hit.type = SettingsHitType::EditTime;
            hit.option = static_cast<uint8_t>(ui::TimerSettingsOption::TIMER_PRESET);
            return hit;
        }

        const int16_t startX = static_cast<int16_t>((display::width() - cards.startW) / 2);
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(startX - 6),
                          static_cast<int16_t>(cards.startY - 4),
                          static_cast<int16_t>(cards.startW + 12),
                          static_cast<int16_t>(cards.startH + 8))) {
            hit.type = SettingsHitType::Start;
            hit.option = static_cast<uint8_t>(ui::TimerSettingsOption::START);
            return hit;
        }

        return hit;
    }

    return hit;
}
