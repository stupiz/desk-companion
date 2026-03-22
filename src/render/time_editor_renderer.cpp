#include "time_editor_renderer.h"

#include <stdio.h>

#include "display_manager.h"

namespace {

struct TimeEditorLayout {
    bool compact;
    int16_t selectorX;
    int16_t selectorY;
    int16_t selectorW;
    int16_t selectorH;
    int16_t selectorGap;
    int16_t adjustX;
    int16_t adjustY;
    int16_t adjustW;
    int16_t adjustH;
    int16_t adjustGap;
    int16_t actionY;
    int16_t actionW;
    int16_t actionH;
    int16_t actionGap;
};

const char* kFieldLabels[] = {
    "HOUR",
    "MIN",
    "SEC",
};

TimeEditorLayout layoutForDisplay() {
    const bool compact = display::height() <= 200;
    TimeEditorLayout layout{};
    layout.compact = compact;

    if (compact) {
        layout.selectorX = 14;
        layout.selectorY = 36;
        layout.selectorW = 112;
        layout.selectorH = 78;
        layout.selectorGap = 10;
        layout.adjustX = static_cast<int16_t>(layout.selectorX + (3 * (layout.selectorW + layout.selectorGap)));
        layout.adjustY = layout.selectorY;
        layout.adjustW = layout.selectorW;
        layout.adjustH = layout.selectorH;
        layout.adjustGap = layout.selectorGap;
        layout.actionY = 136;
        layout.actionW = 240;
        layout.actionH = 30;
        layout.actionGap = 16;
    } else {
        layout.selectorX = 32;
        layout.selectorY = 148;
        layout.selectorW = 144;
        layout.selectorH = 94;
        layout.selectorGap = 18;
        layout.adjustX = static_cast<int16_t>(layout.selectorX + (3 * (layout.selectorW + layout.selectorGap)));
        layout.adjustY = layout.selectorY;
        layout.adjustW = layout.selectorW;
        layout.adjustH = layout.selectorH;
        layout.adjustGap = layout.selectorGap;
        layout.actionY = 518;
        layout.actionW = 220;
        layout.actionH = 34;
        layout.actionGap = 24;
    }

    return layout;
}

bool containsPoint(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h) {
    return px >= x && py >= y && px < static_cast<int16_t>(x + w) && py < static_cast<int16_t>(y + h);
}

void splitHms(uint32_t totalSeconds, uint8_t& hours, uint8_t& minutes, uint8_t& seconds) {
    hours = static_cast<uint8_t>(totalSeconds / 3600U);
    minutes = static_cast<uint8_t>((totalSeconds / 60U) % 60U);
    seconds = static_cast<uint8_t>(totalSeconds % 60U);
}

const char* targetLabel(ui::TimeEditorTarget target) {
    switch (target) {
        case ui::TimeEditorTarget::FOCUS_PRESET:
            return "FOCUS DURATION";
        case ui::TimeEditorTarget::BREAK_PRESET:
            return "BREAK DURATION";
        case ui::TimeEditorTarget::TIMER_PRESET:
            return "TIMER DURATION";
        case ui::TimeEditorTarget::NONE:
        default:
            return "TIME";
    }
}

const char* fieldLabel(uint8_t field) {
    if (field >= 3) {
        return "";
    }
    return kFieldLabels[field];
}

void drawButton(int16_t x,
                int16_t y,
                int16_t w,
                int16_t h,
                const char* label,
                uint16_t border,
                uint16_t fg,
                uint16_t bg,
                uint8_t textSize,
                bool compact) {
    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRoundRect(x, y, w, h, compact ? 8 : 10, bg);
    gfx->drawRoundRect(x, y, w, h, compact ? 8 : 10, border);
    display::drawTextCentered(label,
                              static_cast<int16_t>(x + (w / 2)),
                              static_cast<int16_t>(y + (compact ? ((h - display::textHeight(textSize)) / 2) : ((h - display::textHeight(textSize)) / 2))),
                              fg,
                              textSize,
                              bg);
}

}  // namespace

void TimeEditorRenderer::render(const ui::TimeEditorState& state) {
    if (display::isReady()) {
        Arduino_GFX* gfx = display::gfx();
        if (gfx == nullptr) {
            return;
        }

        constexpr uint16_t kBg = 0x0000;
        constexpr uint16_t kFg = 0xFFFF;
        constexpr uint16_t kDim = 0x8410;
        constexpr uint16_t kAccent = 0x07E0;
        constexpr uint16_t kPanel = 0x1082;

        const TimeEditorLayout layout = layoutForDisplay();
        const bool compact = layout.compact;
        const int16_t titleX = display::width() / 2;
        const int16_t minusX = static_cast<int16_t>(layout.adjustX + layout.adjustW + layout.adjustGap);
        const int16_t actionTotalW = static_cast<int16_t>((layout.actionW * 2) + layout.actionGap);
        const int16_t cancelX = static_cast<int16_t>((display::width() - actionTotalW) / 2);
        const int16_t saveX = static_cast<int16_t>(cancelX + layout.actionW + layout.actionGap);

        uint8_t hours = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;
        splitHms(state.draftSeconds, hours, minutes, seconds);
        const uint8_t values[] = {hours, minutes, seconds};

        gfx->fillScreen(kBg);
        display::drawTextCentered("SET TIME", titleX, compact ? 6 : 18, kFg, compact ? 1 : 2, kBg);
        display::drawTextCentered(targetLabel(state.target), titleX, compact ? 18 : 50, kDim, 1, kBg);

        for (uint8_t field = 0; field < 3; ++field) {
            const int16_t x = static_cast<int16_t>(layout.selectorX + (field * (layout.selectorW + layout.selectorGap)));
            const bool selected = field == state.selectedField;
            const uint16_t border = selected ? kAccent : kDim;
            const uint16_t fill = selected ? kPanel : kBg;
            char valueText[4];
            snprintf(valueText, sizeof(valueText), "%02u", values[field]);

            gfx->fillRoundRect(x, layout.selectorY, layout.selectorW, layout.selectorH, compact ? 10 : 12, fill);
            gfx->drawRoundRect(x, layout.selectorY, layout.selectorW, layout.selectorH, compact ? 10 : 12, border);
            gfx->fillRoundRect(static_cast<int16_t>(x + 8),
                               static_cast<int16_t>(layout.selectorY + 8),
                               5,
                               static_cast<int16_t>(layout.selectorH - 16),
                               2,
                               border);
            display::drawTextCentered(fieldLabel(field),
                                      static_cast<int16_t>(x + (layout.selectorW / 2)),
                                      static_cast<int16_t>(layout.selectorY + 8),
                                      selected ? kAccent : kDim,
                                      1,
                                      fill);
            display::drawTextCentered(valueText,
                                      static_cast<int16_t>(x + (layout.selectorW / 2)),
                                      static_cast<int16_t>(layout.selectorY + (compact ? 32 : 40)),
                                      kFg,
                                      compact ? 3 : 5,
                                      fill);
        }

        gfx->fillRoundRect(layout.adjustX, layout.adjustY, layout.adjustW, layout.adjustH, compact ? 10 : 12, kPanel);
        gfx->drawRoundRect(layout.adjustX, layout.adjustY, layout.adjustW, layout.adjustH, compact ? 10 : 12, kAccent);
        gfx->fillRoundRect(static_cast<int16_t>(layout.adjustX + 8),
                           static_cast<int16_t>(layout.adjustY + 8),
                           5,
                           static_cast<int16_t>(layout.adjustH - 16),
                           2,
                           kAccent);
        display::drawTextCentered("+",
                                  static_cast<int16_t>(layout.adjustX + (layout.adjustW / 2)),
                                  static_cast<int16_t>(layout.adjustY + (compact ? 24 : 30)),
                                  kFg,
                                  compact ? 4 : 6,
                                  kPanel);

        gfx->fillRoundRect(minusX, layout.adjustY, layout.adjustW, layout.adjustH, compact ? 10 : 12, kPanel);
        gfx->drawRoundRect(minusX, layout.adjustY, layout.adjustW, layout.adjustH, compact ? 10 : 12, kAccent);
        gfx->fillRoundRect(static_cast<int16_t>(minusX + 8),
                           static_cast<int16_t>(layout.adjustY + 8),
                           5,
                           static_cast<int16_t>(layout.adjustH - 16),
                           2,
                           kAccent);
        display::drawTextCentered("-",
                                  static_cast<int16_t>(minusX + (layout.adjustW / 2)),
                                  static_cast<int16_t>(layout.adjustY + (compact ? 24 : 30)),
                                  kFg,
                                  compact ? 4 : 6,
                                  kPanel);

        drawButton(cancelX, layout.actionY, layout.actionW, layout.actionH, "BACK", kDim, kFg, kPanel, compact ? 2 : 3, compact);
        drawButton(saveX, layout.actionY, layout.actionW, layout.actionH, "SAVE", kAccent, kFg, kAccent, compact ? 2 : 3, compact);
        return;
    }

    uint8_t hours = 0;
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    splitHms(state.draftSeconds, hours, minutes, seconds);

    Serial.println();
    Serial.println("===== SET TIME =====");
    Serial.print("Target: ");
    Serial.println(targetLabel(state.target));
    Serial.print("Time: ");
    if (hours < 10) Serial.print('0');
    Serial.print(hours);
    Serial.print(':');
    if (minutes < 10) Serial.print('0');
    Serial.print(minutes);
    Serial.print(':');
    if (seconds < 10) Serial.print('0');
    Serial.println(seconds);
    Serial.print("Selected field: ");
    Serial.println(fieldLabel(state.selectedField));
    Serial.println("Touch: tap HR/MIN/SEC box, then use +/- | tap SAVE/BACK");
    Serial.println("Serial: [/] field | +/- adjust | s save | b back");
}

TimeEditorHit TimeEditorRenderer::hitTest(int16_t x, int16_t y, const ui::TimeEditorState& state) const {
    TimeEditorHit hit{};
    if (!display::isReady()) {
        return hit;
    }

    const TimeEditorLayout layout = layoutForDisplay();
    const int16_t minusX = static_cast<int16_t>(layout.adjustX + layout.adjustW + layout.adjustGap);
    const int16_t actionTotalW = static_cast<int16_t>((layout.actionW * 2) + layout.actionGap);
    const int16_t cancelX = static_cast<int16_t>((display::width() - actionTotalW) / 2);
    const int16_t saveX = static_cast<int16_t>(cancelX + layout.actionW + layout.actionGap);

    for (uint8_t field = 0; field < 3; ++field) {
        const int16_t boxX = static_cast<int16_t>(layout.selectorX + (field * (layout.selectorW + layout.selectorGap)));
        if (containsPoint(x,
                          y,
                          static_cast<int16_t>(boxX - 8),
                          static_cast<int16_t>(layout.selectorY - 6),
                          static_cast<int16_t>(layout.selectorW + 16),
                          static_cast<int16_t>(layout.selectorH + 12))) {
            hit.type = TimeEditorHitType::SelectField;
            hit.field = field;
            return hit;
        }
    }

    if (containsPoint(x,
                      y,
                      static_cast<int16_t>(layout.adjustX - 6),
                      static_cast<int16_t>(layout.adjustY - 6),
                      static_cast<int16_t>(layout.adjustW + 12),
                      static_cast<int16_t>(layout.adjustH + 12))) {
        hit.type = TimeEditorHitType::AdjustField;
        hit.field = state.selectedField;
        hit.delta = 1;
        return hit;
    }

    if (containsPoint(x,
                      y,
                      static_cast<int16_t>(minusX - 6),
                      static_cast<int16_t>(layout.adjustY - 6),
                      static_cast<int16_t>(layout.adjustW + 12),
                      static_cast<int16_t>(layout.adjustH + 12))) {
        hit.type = TimeEditorHitType::AdjustField;
        hit.field = state.selectedField;
        hit.delta = -1;
        return hit;
    }

    if (containsPoint(x,
                      y,
                      static_cast<int16_t>(cancelX - 8),
                      static_cast<int16_t>(layout.actionY - 16),
                      static_cast<int16_t>(layout.actionW + 16),
                      static_cast<int16_t>(layout.actionH + 24))) {
        hit.type = TimeEditorHitType::Cancel;
        return hit;
    }

    if (containsPoint(x,
                      y,
                      static_cast<int16_t>(saveX - 8),
                      static_cast<int16_t>(layout.actionY - 16),
                      static_cast<int16_t>(layout.actionW + 16),
                      static_cast<int16_t>(layout.actionH + 24))) {
        hit.type = TimeEditorHitType::Save;
        return hit;
    }

    return hit;
}
