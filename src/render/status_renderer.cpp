#include "status_renderer.h"

#include <Arduino.h>
#include <cstring>
#include <math.h>

#include "display_manager.h"

namespace {

constexpr uint16_t kStatusBgColor = 0x0000;
constexpr uint16_t kStatusGreatColor = 0x001F;
constexpr uint16_t kStatusNormalColor = 0x07E0;
constexpr uint16_t kStatusStuffyColor = 0xFFE0;
constexpr uint16_t kStatusBadColor = 0xF800;
constexpr const char* kColorReset = "\033[0m";

uint16_t dimColor565(uint16_t color565) {
    const uint8_t r = static_cast<uint8_t>((color565 >> 11) & 0x1F);
    const uint8_t g = static_cast<uint8_t>((color565 >> 5) & 0x3F);
    const uint8_t b = static_cast<uint8_t>(color565 & 0x1F);
    const uint8_t dr = static_cast<uint8_t>(r * 5 / 8);
    const uint8_t dg = static_cast<uint8_t>(g * 5 / 8);
    const uint8_t db = static_cast<uint8_t>(b * 5 / 8);
    return static_cast<uint16_t>((dr << 11) | (dg << 5) | db);
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

uint8_t severityBarLevel(air_quality::SeverityLevel airSeverity) {
    return air_quality::severityToInt(airSeverity);
}

uint16_t statusColor565(air_quality::SeverityLevel airSeverity) {
    switch (airSeverity) {
        case air_quality::SeverityLevel::GREAT:
            return kStatusGreatColor;
        case air_quality::SeverityLevel::NORMAL:
            return kStatusNormalColor;
        case air_quality::SeverityLevel::STUFFY:
            return kStatusStuffyColor;
        case air_quality::SeverityLevel::BAD:
        default:
            return kStatusBadColor;
    }
}

const char* statusColorCode(air_quality::SeverityLevel airSeverity) {
    switch (airSeverity) {
        case air_quality::SeverityLevel::GREAT:
            return "\033[38;5;39m";
        case air_quality::SeverityLevel::NORMAL:
            return "\033[38;5;46m";
        case air_quality::SeverityLevel::STUFFY:
            return "\033[38;5;226m";
        case air_quality::SeverityLevel::BAD:
        default:
            return "\033[38;5;196m";
    }
}

uint8_t textLength(const char* text) {
    if (text == nullptr) {
        return 0;
    }
    uint8_t len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return len;
}

void renderStatusDisplay(const char* statusText, uint8_t effectMode, unsigned long nowMs,
                         air_quality::SeverityLevel airSeverity) {
    if (!display::isReady()) {
        return;
    }

    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    const uint16_t w = display::width();
    const uint16_t h = display::height();
    const uint16_t textColor = statusColor565(airSeverity);
    const uint16_t glowColor = dimColor565(textColor);
    const uint16_t deepGlow = dimColor565(glowColor);
    const uint8_t textSize = 8;
    const int16_t centerX = static_cast<int16_t>(w / 2);
    const int16_t baseTextY = static_cast<int16_t>((h - display::textHeight(textSize)) / 2);
    int16_t textY = baseTextY;
    const char* text = statusText == nullptr ? "" : statusText;

    gfx->fillScreen(kStatusBgColor);

    if (effectMode == 1) {
        const float pulse = (sinf(static_cast<float>(nowMs) * 0.0080f) + 1.0f) * 0.5f;
        const int16_t halo = static_cast<int16_t>(4 + pulse * 3.0f);
        const uint16_t outerAura = mixColor565(glowColor, 0xFFFF, static_cast<uint8_t>(32 + pulse * 96.0f));
        const uint16_t innerAura = mixColor565(textColor, 0xFFFF, static_cast<uint8_t>(24 + pulse * 52.0f));
        textY = static_cast<int16_t>(baseTextY - 2 + pulse * 4.0f);
        display::drawTextCentered(text, centerX - halo, textY, outerAura, textSize, kStatusBgColor);
        display::drawTextCentered(text, centerX + halo, textY, outerAura, textSize, kStatusBgColor);
        display::drawTextCentered(text, centerX, static_cast<int16_t>(textY - halo), outerAura, textSize, kStatusBgColor);
        display::drawTextCentered(text, centerX, static_cast<int16_t>(textY + halo), outerAura, textSize, kStatusBgColor);
        display::drawTextCentered(text, centerX, static_cast<int16_t>(textY - 1), innerAura, textSize, kStatusBgColor);
    } else if (effectMode == 2) {
        const float swing = sinf(static_cast<float>(nowMs) * 0.0130f);
        const float bob = cosf(static_cast<float>(nowMs) * 0.0100f);
        const int16_t trailX = static_cast<int16_t>(swing * 18.0f);
        const int16_t trailY = static_cast<int16_t>(bob * 4.0f);
        const uint16_t brightTrail = mixColor565(textColor, 0xFFFF, 92);
        const uint16_t softTrail = mixColor565(glowColor, 0xFFFF, 44);
        textY = static_cast<int16_t>(baseTextY - trailY);
        display::drawTextCentered(text, static_cast<int16_t>(centerX - trailX - 8), static_cast<int16_t>(textY + 4), deepGlow, textSize, kStatusBgColor);
        display::drawTextCentered(text, static_cast<int16_t>(centerX - trailX / 2), static_cast<int16_t>(textY + 2), softTrail, textSize, kStatusBgColor);
        display::drawTextCentered(text, static_cast<int16_t>(centerX + trailX), static_cast<int16_t>(textY - 3), brightTrail, textSize, kStatusBgColor);
    } else {
        display::drawTextCentered(text, centerX + 3, textY + 3, deepGlow, textSize, kStatusBgColor);
        display::drawTextCentered(text, centerX + 1, textY + 1, glowColor, textSize, kStatusBgColor);
    }

    display::drawTextCentered(text, centerX, textY, textColor, textSize, kStatusBgColor);
}

void printSeverityRail(air_quality::SeverityLevel airSeverity) {
    const uint8_t total = 12;
    const uint8_t filled = static_cast<uint8_t>(severityBarLevel(airSeverity) * 3);

    Serial.print(" [");
    for (uint8_t i = 0; i < total; i++) {
        Serial.print(i < filled ? "#" : "-");
    }
    Serial.print("]");
}

void printStatusLine(const char* statusText, const char* colorCode) {
    Serial.print(colorCode);
    Serial.print(statusText == nullptr ? "" : statusText);
    Serial.print(kColorReset);
}

void printLeadingBadge(const char* statusText) {
    const uint8_t len = textLength(statusText);
    const uint8_t framePad = 4;
    const uint8_t width = static_cast<uint8_t>(len + (framePad * 2));

    Serial.print("[");
    for (uint8_t i = 0; i < width; i++) {
        Serial.print(static_cast<char>((i == 0 || i == width - 1) ? '.' : '-'));
    }
    Serial.print("] ");
}

void renderStatusPlain(const char* statusText, air_quality::SeverityLevel airSeverity) {
    Serial.print(">> ");
    printStatusLine(statusText, statusColorCode(airSeverity));
    printSeverityRail(airSeverity);
    Serial.println(" <<");
}

void renderStatusBreath(const char* statusText, unsigned long nowMs, air_quality::SeverityLevel airSeverity) {
    const uint8_t period = 32;
    const uint8_t phase = static_cast<uint8_t>((nowMs / 140) % period);
    const uint8_t halo = static_cast<uint8_t>((phase < 16) ? phase : (period - phase));
    const uint8_t leading = static_cast<uint8_t>(halo / 2);

    printLeadingBadge(statusText);

    for (uint8_t i = 0; i < leading; i++) {
        Serial.print(" ");
    }
    printStatusLine(statusText, statusColorCode(airSeverity));
    Serial.print(" ");
    for (uint8_t i = 0; i < (textLength(statusText) + leading); i++) {
        Serial.print(i == (textLength(statusText) + leading - 1) ? "^" : ".");
    }
    Serial.print(" ");
    printSeverityRail(airSeverity);
    Serial.println();
}

void renderStatusDotWave(const char* statusText, unsigned long nowMs, air_quality::SeverityLevel airSeverity) {
    const uint8_t dotCount = 5;
    const uint8_t phase = static_cast<uint8_t>((nowMs / 180) % dotCount);
    Serial.print(" > ");
    for (uint8_t i = 0; i < dotCount; i++) {
        Serial.print(i <= phase ? "• " : "· ");
    }
    printStatusLine(statusText, statusColorCode(airSeverity));
    Serial.print(" ");
    for (uint8_t i = 0; i < dotCount; i++) {
        Serial.print(i >= (dotCount - 1 - phase) ? "• " : "· ");
    }
    Serial.print(" ");
    printSeverityRail(airSeverity);
    Serial.print("< ");
    Serial.println();
}

}  // namespace

void StatusRenderer::render(const char* statusText, uint8_t effectMode, unsigned long nowMs,
                            air_quality::SeverityLevel airSeverity) {
    if (display::isReady()) {
        renderStatusDisplay(statusText, effectMode, nowMs, airSeverity);
        return;
    }

    Serial.println();
    Serial.println("====== STATUS ======");
    switch (effectMode % 3) {
        case 0:
            renderStatusPlain(statusText, airSeverity);
            break;
        case 1:
            renderStatusBreath(statusText, nowMs, airSeverity);
            break;
        default:
            renderStatusDotWave(statusText, nowMs, airSeverity);
            break;
    }
    Serial.println("===================");
}
