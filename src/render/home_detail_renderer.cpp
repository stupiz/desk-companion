#include "home_detail_renderer.h"

#include <stdio.h>
#include <string.h>

#include "display_manager.h"

namespace {

constexpr uint16_t kBg = 0x0000;
constexpr uint16_t kFg = 0xFFFF;
constexpr uint16_t kDim = 0x8410;
constexpr uint16_t kInk = 0x2209;
constexpr uint16_t kInkSoft = 0x63AE;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                 ((g & 0xFC) << 3) |
                                 (b >> 3));
}

struct CardTheme {
    uint16_t border;
    uint16_t fill;
    uint16_t text;
    uint16_t muted;
    uint16_t stripe;
};

constexpr CardTheme kOfflineTheme = {
    rgb565(158, 166, 178),
    rgb565(236, 239, 244),
    kInk,
    kInkSoft,
    rgb565(206, 211, 219),
};

constexpr CardTheme kWaitingTheme = {
    rgb565(142, 154, 168),
    rgb565(241, 244, 248),
    kInk,
    kInkSoft,
    rgb565(203, 210, 219),
};

constexpr CardTheme kGreatTheme = {
    rgb565(103, 148, 232),
    rgb565(232, 239, 251),
    kInk,
    kInkSoft,
    rgb565(175, 198, 238),
};

constexpr CardTheme kNormalTheme = {
    rgb565(108, 166, 121),
    rgb565(232, 244, 236),
    kInk,
    kInkSoft,
    rgb565(173, 212, 181),
};

constexpr CardTheme kStuffyTheme = {
    rgb565(201, 160, 83),
    rgb565(252, 244, 221),
    kInk,
    kInkSoft,
    rgb565(232, 206, 145),
};

constexpr CardTheme kBadTheme = {
    rgb565(205, 102, 102),
    rgb565(251, 232, 232),
    kInk,
    kInkSoft,
    rgb565(232, 166, 166),
};

uint16_t severityColor565(air_quality::SeverityLevel level) {
    switch (level) {
        case air_quality::SeverityLevel::GREAT:
            return kGreatTheme.border;
        case air_quality::SeverityLevel::NORMAL:
            return kNormalTheme.border;
        case air_quality::SeverityLevel::STUFFY:
            return kStuffyTheme.border;
        case air_quality::SeverityLevel::BAD:
        default:
            return kBadTheme.border;
    }
}

CardTheme severityTheme(air_quality::SeverityLevel level) {
    switch (level) {
        case air_quality::SeverityLevel::GREAT:
            return kGreatTheme;
        case air_quality::SeverityLevel::NORMAL:
            return kNormalTheme;
        case air_quality::SeverityLevel::STUFFY:
            return kStuffyTheme;
        case air_quality::SeverityLevel::BAD:
        default:
            return kBadTheme;
    }
}

uint16_t mixColor565(uint16_t a, uint16_t b, uint8_t weightB) {
    const uint8_t weightA = static_cast<uint8_t>(255 - weightB);

    const uint8_t ar = static_cast<uint8_t>((a >> 11) & 0x1F);
    const uint8_t ag = static_cast<uint8_t>((a >> 5) & 0x3F);
    const uint8_t ab = static_cast<uint8_t>(a & 0x1F);
    const uint8_t br = static_cast<uint8_t>((b >> 11) & 0x1F);
    const uint8_t bg = static_cast<uint8_t>((b >> 5) & 0x3F);
    const uint8_t bb = static_cast<uint8_t>(b & 0x1F);

    const uint8_t rr = static_cast<uint8_t>((ar * weightA + br * weightB) / 255);
    const uint8_t rg = static_cast<uint8_t>((ag * weightA + bg * weightB) / 255);
    const uint8_t rb = static_cast<uint8_t>((ab * weightA + bb * weightB) / 255);

    return static_cast<uint16_t>((rr << 11) | (rg << 5) | rb);
}

void drawCard(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill) {
    if (gfx == nullptr || w <= 4 || h <= 4) {
        return;
    }

    gfx->drawRoundRect(x, y, w, h, 10, border);
    gfx->fillRoundRect(x + 2, y + 2, w - 4, h - 4, 8, fill);
}

void drawPill(Arduino_GFX* gfx,
              int16_t x,
              int16_t y,
              const char* text,
              uint16_t textColor,
              uint16_t fillColor,
              uint16_t borderColor) {
    if (gfx == nullptr || text == nullptr) {
        return;
    }

    const int16_t padX = 8;
    const int16_t pillH = 16;
    const int16_t pillW = static_cast<int16_t>(display::textWidth(text, 1) + (padX * 2));
    gfx->fillRoundRect(x, y, pillW, pillH, 7, fillColor);
    gfx->drawRoundRect(x, y, pillW, pillH, 7, borderColor);
    display::drawText(x + padX, y + 4, text, textColor, 1, fillColor);
}

void drawMetricCard(Arduino_GFX* gfx,
                    int16_t x,
                    int16_t y,
                    int16_t w,
                    int16_t h,
                    const char* title,
                    const char* value,
                    const char* unit,
                    air_quality::SeverityLevel severity,
                    bool online,
                    bool waiting = false) {
    if (gfx == nullptr) {
        return;
    }

    const char* shownValue = online ? (waiting ? "WARMING" : value) : "OFFLINE";
    const char* shownUnit = (online && !waiting) ? unit : "";
    const CardTheme theme = !online ? kOfflineTheme : (waiting ? kWaitingTheme : severityTheme(severity));
    const uint16_t dividerColor = mixColor565(theme.fill, theme.border, 96);
    const uint16_t valueColor = (online && !waiting) ? theme.border : theme.text;
    drawCard(gfx, x, y, w, h, theme.border, theme.fill);
    gfx->fillRoundRect(x + 2, y + h - 7, w - 4, 5, 3, theme.stripe);

    const int16_t padX = 12;
    const int16_t titleY = static_cast<int16_t>(y + 10);
    display::drawText(x + padX, titleY, title, theme.text, 2, theme.fill);
    gfx->drawFastHLine(x + padX, static_cast<int16_t>(y + 31), w - (padX * 2), dividerColor);

    const uint8_t unitSize = online ? 2 : 1;
    int16_t valueSize = (strlen(shownValue) <= 5) ? 3 : 2;
    if (!online || waiting) {
        valueSize = 2;
    }
    int16_t groupW = 0;
    do {
        groupW = display::textWidth(shownValue, static_cast<uint8_t>(valueSize));
        if (shownUnit[0] != '\0') {
            groupW = static_cast<int16_t>(groupW + 8 + display::textWidth(shownUnit, unitSize));
        }
        if (groupW <= (w - (padX * 2)) || valueSize <= 2) {
            break;
        }
        --valueSize;
    } while (true);

    const int16_t valueY = static_cast<int16_t>(y + ((valueSize == 3) ? 40 : 44));
    const int16_t valueX = static_cast<int16_t>(x + ((w - groupW) / 2));
    display::drawText(valueX, valueY, shownValue, valueColor, static_cast<uint8_t>(valueSize), theme.fill);

    if (shownUnit[0] != '\0') {
        const int16_t unitX = static_cast<int16_t>(
            valueX + display::textWidth(shownValue, static_cast<uint8_t>(valueSize)) + 8);
        const int16_t unitY = static_cast<int16_t>(
            valueY + ((display::textHeight(static_cast<uint8_t>(valueSize)) - display::textHeight(unitSize)) / 2) + 2);
        display::drawText(unitX, unitY, shownUnit, theme.muted, unitSize, theme.fill);
    }
}

void drawSummaryCard(Arduino_GFX* gfx,
                     int16_t x,
                     int16_t y,
                     int16_t w,
                     int16_t h,
                     const air_quality::AirQualityScore& score,
                     bool sampleValid,
                     bool scdOnline,
                     bool sgpOnline,
                     bool sgpWarmingUp) {
    if (gfx == nullptr) {
        return;
    }

    const bool waiting = !scdOnline || !sampleValid || (sgpOnline && sgpWarmingUp);
    const CardTheme theme = !scdOnline ? kOfflineTheme : (waiting ? kWaitingTheme : severityTheme(score.overall));
    const uint16_t statusColor = (!scdOnline || waiting) ? theme.text : theme.border;
    drawCard(gfx, x, y, w, h, theme.border, theme.fill);
    gfx->fillRoundRect(x + 2, y + h - 7, w - 4, 5, 3, theme.stripe);

    display::drawText(x + 12, y + 10, "AIR QUALITY", theme.text, 2, theme.fill);

    const char* overall = !scdOnline ? "OFFLINE"
                        : (!sampleValid ? "WAITING"
                                        : ((sgpOnline && sgpWarmingUp) ? "WARMING" : air_quality::severityLabel(score.overall)));
    const uint8_t overallSize = (display::textWidth(overall, 4) <= (w - 24)) ? 4 : 3;
    display::drawTextCentered(overall,
                              static_cast<int16_t>(x + w / 2),
                              static_cast<int16_t>(y + (h / 2) - 14),
                              statusColor,
                              overallSize,
                              theme.fill);
}

void renderDetailDisplay(const air_quality::AirSample& sample,
                         const air_quality::AirQualityScore& score,
                         bool scdOnline,
                         bool sgpOnline,
                         bool sgpWarmingUp) {
    if (!display::isReady()) {
        return;
    }

    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    char line[48];
    const bool compact = display::height() <= 200;
    const bool wide = display::width() >= 560;
    gfx->fillScreen(kBg);

    if (!wide) {
        display::drawTextCentered("DETAIL", display::width() / 2, compact ? 8 : 22, kFg, compact ? 1 : 2, kBg);
    }

    if (!sample.valid && !wide) {
        display::drawTextCentered("No sensor data yet", display::width() / 2, compact ? 80 : 90, kDim, compact ? 1 : 2, kBg);
        return;
    }

    if (wide) {
        char co2Value[16];
        char tempValue[16];
        char humidityValue[16];
        char vocValue[16];

        if (sample.valid) {
            snprintf(co2Value, sizeof(co2Value), "%u", sample.co2ppm);
            snprintf(tempValue, sizeof(tempValue), "%.1f", sample.temperatureC);
            snprintf(humidityValue, sizeof(humidityValue), "%.1f", sample.humidityPct);
        } else {
            snprintf(co2Value, sizeof(co2Value), "--");
            snprintf(tempValue, sizeof(tempValue), "--");
            snprintf(humidityValue, sizeof(humidityValue), "--");
        }
        snprintf(vocValue, sizeof(vocValue), "%u", sample.vocIndex);

        const int16_t margin = 8;
        const int16_t topY = 8;
        const int16_t cardGap = 8;
        const int16_t summaryW = 202;
        const int16_t summaryH = static_cast<int16_t>(display::height() - topY - margin);
        const int16_t gridX = static_cast<int16_t>(margin + summaryW + cardGap);
        const int16_t gridW = static_cast<int16_t>(display::width() - gridX - margin);
        const int16_t cardW = static_cast<int16_t>((gridW - cardGap) / 2);
        const int16_t cardH = static_cast<int16_t>((summaryH - cardGap) / 2);

        drawSummaryCard(gfx, margin, topY, summaryW, summaryH, score, sample.valid, scdOnline, sgpOnline, sgpWarmingUp);

        drawMetricCard(gfx, gridX, topY, cardW, cardH, "CO2", co2Value, "ppm", score.co2Severity, scdOnline && sample.valid);
        drawMetricCard(gfx,
                       static_cast<int16_t>(gridX + cardW + cardGap),
                       topY,
                       cardW,
                       cardH,
                       "TEMP",
                       tempValue,
                       "C",
                       score.tempSeverity,
                       scdOnline && sample.valid);
        drawMetricCard(gfx,
                       gridX,
                       static_cast<int16_t>(topY + cardH + cardGap),
                       cardW,
                       cardH,
                       "HUMIDITY",
                       humidityValue,
                       "%",
                       score.humiditySeverity,
                       scdOnline && sample.valid);
        drawMetricCard(gfx,
                       static_cast<int16_t>(gridX + cardW + cardGap),
                       static_cast<int16_t>(topY + cardH + cardGap),
                       cardW,
                       cardH,
                       "VOC",
                       vocValue,
                       "index",
                       score.vocSeverity,
                       sgpOnline,
                       sgpWarmingUp);
        return;
    }

    if (compact) {
        const int16_t leftX = 12;
        const int16_t rightX = static_cast<int16_t>(display::width() / 2 + 8);
        const int16_t row1Y = 28;
        const int16_t row2Y = 76;
        const int16_t row3Y = 126;
        const uint16_t kAccent = severityColor565(score.overall);

        display::drawText(leftX, row1Y, "CO2", kDim, 1, kBg);
        snprintf(line, sizeof(line), "%u ppm", sample.co2ppm);
        display::drawText(leftX, row1Y + 14, line, kAccent, 2, kBg);

        display::drawText(rightX, row1Y, "TEMP", kDim, 1, kBg);
        snprintf(line, sizeof(line), "%.1f C", sample.temperatureC);
        display::drawText(rightX, row1Y + 14, line, kFg, 2, kBg);

        display::drawText(leftX, row2Y, "HUMIDITY", kDim, 1, kBg);
        snprintf(line, sizeof(line), "%.1f %%", sample.humidityPct);
        display::drawText(leftX, row2Y + 14, line, kFg, 2, kBg);

        display::drawText(rightX, row2Y, "VOC", kDim, 1, kBg);
        if (!sgpOnline) {
            snprintf(line, sizeof(line), "OFFLINE");
        } else if (sgpWarmingUp) {
            snprintf(line, sizeof(line), "WARMING");
        } else {
            snprintf(line, sizeof(line), "%u", sample.vocIndex);
        }
        display::drawText(rightX, row2Y + 14, line, kFg, 2, kBg);

        snprintf(line, sizeof(line), "AIR %s", air_quality::severityLabel(score.overall));
        display::drawText(leftX, row3Y, line, kAccent, 2, kBg);
        snprintf(line, sizeof(line), "CO2 %s  VOC %s", air_quality::severityLabel(score.co2Severity),
                 air_quality::severityLabel(score.vocSeverity));
        display::drawText(leftX, row3Y + 20, line, kFg, 1, kBg);
        snprintf(line, sizeof(line), "TEMP %s  RH %s", air_quality::severityLabel(score.tempSeverity),
                 air_quality::severityLabel(score.humiditySeverity));
        display::drawText(leftX, row3Y + 34, line, kFg, 1, kBg);
        return;
    }

    const uint16_t kAccent = severityColor565(score.overall);
    display::drawText(10, 74, "CO2", kDim, 1, kBg);
    snprintf(line, sizeof(line), "%u ppm", sample.co2ppm);
    display::drawText(10, 92, line, kAccent, 2, kBg);

    display::drawText(10, 154, "TEMP", kDim, 1, kBg);
    snprintf(line, sizeof(line), "%.1f C", sample.temperatureC);
    display::drawText(10, 172, line, kFg, 2, kBg);

    display::drawText(10, 234, "HUMIDITY", kDim, 1, kBg);
    snprintf(line, sizeof(line), "%.1f %%", sample.humidityPct);
    display::drawText(10, 252, line, kFg, 2, kBg);

    display::drawText(10, 314, "VOC", kDim, 1, kBg);
    if (!sgpOnline) {
        snprintf(line, sizeof(line), "OFFLINE");
    } else if (sgpWarmingUp) {
        snprintf(line, sizeof(line), "WARMING");
    } else {
        snprintf(line, sizeof(line), "%u", sample.vocIndex);
    }
    display::drawText(10, 332, line, kFg, 2, kBg);

    display::drawText(10, 410, "SEVERITY", kDim, 1, kBg);
    display::drawText(10, 430, air_quality::severityLabel(score.overall), kAccent, 3, kBg);

    snprintf(line, sizeof(line), "CO2 %s", air_quality::severityLabel(score.co2Severity));
    display::drawText(10, 510, line, kFg, 1, kBg);
    snprintf(line, sizeof(line), "VOC %s", air_quality::severityLabel(score.vocSeverity));
    display::drawText(10, 528, line, kFg, 1, kBg);
    snprintf(line, sizeof(line), "TEMP %s", air_quality::severityLabel(score.tempSeverity));
    display::drawText(10, 546, line, kFg, 1, kBg);
    snprintf(line, sizeof(line), "RH %s", air_quality::severityLabel(score.humiditySeverity));
    display::drawText(10, 564, line, kFg, 1, kBg);
}

}  // namespace

void HomeDetailRenderer::render(const air_quality::AirSample& sample,
                                const air_quality::AirQualityScore& score,
                                bool scdOnline,
                                bool sgpOnline,
                                bool sgpWarmingUp) {
    if (display::isReady()) {
        renderDetailDisplay(sample, score, scdOnline, sgpOnline, sgpWarmingUp);
        return;
    }

    Serial.println();
    Serial.println("======= DETAIL =======");
    Serial.print("SCD41: ");
    Serial.print(scdOnline ? "LIVE" : "OFFLINE");
    Serial.print(" | SGP40: ");
    if (!sgpOnline) {
        Serial.println("OFFLINE");
    } else if (sgpWarmingUp) {
        Serial.println("WARMING");
    } else {
        Serial.println("LIVE");
    }
    if (!sample.valid) {
        Serial.println("No sensor data yet.");
        return;
    }
    Serial.print("CO2: ");
    Serial.print(sample.co2ppm);
    Serial.print(" ppm | ");
    Serial.print("Temp: ");
    Serial.print(sample.temperatureC, 1);
    Serial.print(" C | ");
    Serial.print("Humidity: ");
    Serial.print(sample.humidityPct, 1);
    Serial.print("% | VOC: ");
    if (!sgpOnline) {
        Serial.println("OFFLINE");
    } else if (sgpWarmingUp) {
        Serial.println("WARMING");
    } else {
        Serial.println(sample.vocIndex);
    }

    Serial.print("CO2: ");
    Serial.print(air_quality::severityLabel(score.co2Severity));
    Serial.print("  VOC: ");
    Serial.print(air_quality::severityLabel(score.vocSeverity));
    Serial.print("  Temp: ");
    Serial.print(air_quality::severityLabel(score.tempSeverity));
    Serial.print("  RH: ");
    Serial.print(air_quality::severityLabel(score.humiditySeverity));
    Serial.print("  => ");
    Serial.println(air_quality::severityLabel(score.overall));
    Serial.println("=====================");
}
