#include "home_eyes_renderer.h"

#include <math.h>
#include <stdio.h>

#include "display_manager.h"

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
#include <Arduino_GFX_Library.h>
#endif

namespace {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                 ((g & 0xFC) << 3) |
                                 (b >> 3));
}

constexpr uint16_t kBgColor = rgb565(4, 7, 14);
constexpr uint16_t kBackdropGlow = rgb565(12, 18, 30);
constexpr uint16_t kGridColor = rgb565(20, 27, 42);
constexpr uint16_t kRetroBg = rgb565(247, 220, 177);
constexpr uint16_t kRetroShadow = rgb565(210, 220, 228);
constexpr uint16_t kRetroStar = rgb565(132, 94, 58);
constexpr uint16_t kRetroPink = rgb565(255, 118, 194);
constexpr uint16_t kShellFill = rgb565(18, 24, 38);
constexpr uint16_t kShellInner = rgb565(10, 14, 25);
constexpr uint16_t kScreenFill = rgb565(232, 242, 255);
constexpr uint16_t kScreenShade = rgb565(178, 195, 224);
constexpr uint16_t kInkDark = rgb565(8, 11, 18);
constexpr uint16_t kInkSoft = rgb565(90, 104, 130);
constexpr uint16_t kAccentCyan = rgb565(96, 224, 255);
constexpr uint16_t kAccentMagenta = rgb565(255, 88, 204);
constexpr uint16_t kHighlight = rgb565(255, 255, 255);

inline int iMax(int a, int b) {
    return (a > b) ? a : b;
}

inline int iMin(int a, int b) {
    return (a < b) ? a : b;
}

inline int clampInt(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
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

struct EyeStyle {
    uint16_t accent;
    uint8_t openHeightRatio;
    uint8_t irisRadius;
    uint8_t blinkLineThickness;
};

struct NaturalMotion {
    bool initialized = false;
    bool inBlink = false;
    unsigned long blinkStartMs = 0;
    unsigned long blinkDurationMs = 120;
    unsigned long nextBlinkMs = 0;
    float curLookX = 0.0f;
    float curLookY = 0.0f;
    float targetLookX = 0.0f;
    float targetLookY = 0.0f;
    unsigned long nextLookMs = 0;
    unsigned long lastUpdateMs = 0;
};

struct MotionSample {
    float eyeOpenRatio;
    float lookX;
    float lookY;
};

EyeStyle styleFor(const mood::MoodState& moodState) {
    switch (moodState.eyeMood) {
        case mood::EyeMood::GREAT:
            return {moodState.displayColor565, 84, 24, 3};
        case mood::EyeMood::NORMAL:
            return {moodState.displayColor565, 74, 22, 3};
        case mood::EyeMood::STUFFY:
            return {moodState.displayColor565, 60, 20, 4};
        case mood::EyeMood::BAD:
        default:
            return {moodState.displayColor565, 48, 18, 4};
    }
}

unsigned long jitterMinMs(const mood::MoodState& moodState) {
    switch (moodState.eyeMood) {
        case mood::EyeMood::BAD:
            return 1400;
        case mood::EyeMood::STUFFY:
            return 1900;
        case mood::EyeMood::NORMAL:
            return 2300;
        case mood::EyeMood::GREAT:
        default:
            return 2800;
    }
}

unsigned long blinkDurationMs(const mood::MoodState& moodState) {
    switch (moodState.eyeMood) {
        case mood::EyeMood::BAD:
            return random(80, 135);
        case mood::EyeMood::STUFFY:
            return random(95, 150);
        case mood::EyeMood::NORMAL:
            return random(100, 165);
        case mood::EyeMood::GREAT:
        default:
            return random(110, 180);
    }
}

void initMotionState(NaturalMotion& state, unsigned long nowMs) {
    randomSeed(nowMs ^ (nowMs << 7) ^ 0x63C5);
    state.initialized = true;
    state.inBlink = false;
    state.blinkStartMs = 0;
    state.blinkDurationMs = 130;
    state.nextBlinkMs = nowMs + random(1400, 4200);
    state.curLookX = 0.0f;
    state.curLookY = 0.0f;
    state.targetLookX = 0.0f;
    state.targetLookY = 0.0f;
    state.nextLookMs = nowMs + random(420, 1300);
    state.lastUpdateMs = nowMs;
}

MotionSample sampleMotion(const mood::MoodState& moodState, unsigned long nowMs) {
    static NaturalMotion motion;
    MotionSample sample{1.0f, 0.0f, 0.0f};

    if (!motion.initialized) {
        initMotionState(motion, nowMs);
    }

    if (!motion.inBlink && nowMs >= motion.nextBlinkMs) {
        motion.inBlink = true;
        motion.blinkStartMs = nowMs;
        motion.blinkDurationMs = blinkDurationMs(moodState);
    }

    if (motion.inBlink) {
        const unsigned long dt = nowMs - motion.blinkStartMs;
        if (dt >= motion.blinkDurationMs) {
            motion.inBlink = false;
            motion.nextBlinkMs = nowMs + random(jitterMinMs(moodState), jitterMinMs(moodState) + 2600);
            sample.eyeOpenRatio = 1.0f;
        } else {
            const float t = static_cast<float>(dt) / static_cast<float>(motion.blinkDurationMs);
            sample.eyeOpenRatio = (t < 0.5f) ? (1.0f - (2.0f * t)) : ((t - 0.5f) * 2.0f);
            if (sample.eyeOpenRatio < 0.0f) {
                sample.eyeOpenRatio = 0.0f;
            }
        }
    }

    const unsigned long elapsedMs = nowMs - motion.lastUpdateMs;
    if (elapsedMs > 0) {
        if (nowMs >= motion.nextLookMs) {
            const int maxLookX = (moodState.eyeMood == mood::EyeMood::BAD) ? 16 : 13;
            const int maxLookY = (moodState.eyeMood == mood::EyeMood::BAD) ? 8 : 6;
            motion.targetLookX = static_cast<float>(random(-maxLookX, maxLookX + 1));
            motion.targetLookY = static_cast<float>(random(-maxLookY, maxLookY + 1));
            motion.nextLookMs = nowMs + random(420, 1500);
        }

        float lerp = 0.15f + (static_cast<float>(elapsedMs) * 0.0011f);
        if (lerp < 0.06f) {
            lerp = 0.06f;
        }
        if (lerp > 0.36f) {
            lerp = 0.36f;
        }

        motion.curLookX += (motion.targetLookX - motion.curLookX) * lerp;
        motion.curLookY += (motion.targetLookY - motion.curLookY) * lerp;
    }

    sample.lookX = motion.curLookX;
    sample.lookY = motion.curLookY;
    motion.lastUpdateMs = nowMs;
    return sample;
}

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
void drawBackdrop(Arduino_GFX* gfx, int16_t w, int16_t h, uint16_t accent) {
    if (gfx == nullptr) {
        return;
    }

    gfx->fillScreen(kBgColor);

    const int16_t panelX = 16;
    const int16_t panelY = 16;
    const int16_t panelW = static_cast<int16_t>(w - 32);
    const int16_t panelH = static_cast<int16_t>(h - 32);
    const uint16_t shellBorder = mixColor565(accent, kHighlight, 28);
    const uint16_t shellGlow = mixColor565(kBackdropGlow, accent, 36);

    gfx->fillRoundRect(panelX, panelY, panelW, panelH, 24, shellGlow);
    gfx->fillRoundRect(panelX + 4, panelY + 4, panelW - 8, panelH - 8, 20, kShellFill);
    gfx->drawRoundRect(panelX, panelY, panelW, panelH, 24, shellBorder);
    gfx->drawRoundRect(panelX + 4, panelY + 4, panelW - 8, panelH - 8, 20, mixColor565(kGridColor, accent, 32));

    for (int16_t x = panelX + 26; x < panelX + panelW - 20; x += 40) {
        gfx->drawFastVLine(x, panelY + 16, panelH - 32, kGridColor);
    }
    for (int16_t y = panelY + 18; y < panelY + panelH - 16; y += 18) {
        gfx->drawFastHLine(panelX + 18, y, panelW - 36, kGridColor);
    }

    gfx->fillRoundRect(panelX + 20, panelY + 10, 74, 5, 2, kAccentMagenta);
    gfx->fillRoundRect(panelX + panelW - 94, panelY + 10, 74, 5, 2, kAccentCyan);
    gfx->fillRect(panelX + 10, panelY + panelH - 24, 8, 8, kAccentMagenta);
    gfx->fillRect(panelX + panelW - 18, panelY + panelH - 24, 8, 8, kAccentCyan);
}

void fillPixelPanel(Arduino_GFX* gfx,
                    int16_t x,
                    int16_t y,
                    int16_t w,
                    int16_t h,
                    uint16_t fillColor,
                    uint16_t borderColor) {
    if (gfx == nullptr || w < 6 || h < 6) {
        return;
    }

    gfx->fillRect(x + 2, y, w - 4, h, fillColor);
    gfx->fillRect(x, y + 2, w, h - 4, fillColor);

    gfx->drawFastHLine(x + 2, y, w - 4, borderColor);
    gfx->drawFastHLine(x + 2, y + h - 1, w - 4, borderColor);
    gfx->drawFastVLine(x, y + 2, h - 4, borderColor);
    gfx->drawFastVLine(x + w - 1, y + 2, h - 4, borderColor);

    gfx->drawPixel(x + 1, y + 1, borderColor);
    gfx->drawPixel(x + w - 2, y + 1, borderColor);
    gfx->drawPixel(x + 1, y + h - 2, borderColor);
    gfx->drawPixel(x + w - 2, y + h - 2, borderColor);
}

void fillPixelRow(Arduino_GFX* gfx,
                  int16_t x,
                  int16_t y,
                  int16_t cell,
                  int8_t startBlocks,
                  int8_t widthBlocks,
                  uint16_t color) {
    if (gfx == nullptr || widthBlocks <= 0) {
        return;
    }

    gfx->fillRect(static_cast<int16_t>(x + startBlocks * cell),
                  y,
                  static_cast<int16_t>(widthBlocks * cell),
                  cell,
                  color);
}

void fillMirroredPixelRect(Arduino_GFX* gfx,
                           int16_t originX,
                           int16_t originY,
                           int16_t cell,
                           int16_t gridWidthBlocks,
                           bool mirror,
                           int16_t col,
                           int16_t row,
                           int16_t widthBlocks,
                           int16_t heightBlocks,
                           uint16_t color) {
    if (gfx == nullptr || widthBlocks <= 0 || heightBlocks <= 0) {
        return;
    }

    const int16_t drawCol = mirror
        ? static_cast<int16_t>(gridWidthBlocks - col - widthBlocks)
        : col;

    gfx->fillRect(static_cast<int16_t>(originX + drawCol * cell),
                  static_cast<int16_t>(originY + row * cell),
                  static_cast<int16_t>(widthBlocks * cell),
                  static_cast<int16_t>(heightBlocks * cell),
                  color);
}

void fillMirroredPixelRow(Arduino_GFX* gfx,
                          int16_t originX,
                          int16_t originY,
                          int16_t cell,
                          int16_t gridWidthBlocks,
                          bool mirror,
                          int16_t startBlocks,
                          int16_t row,
                          int16_t widthBlocks,
                          uint16_t color) {
    fillMirroredPixelRect(gfx,
                          originX,
                          originY,
                          cell,
                          gridWidthBlocks,
                          mirror,
                          startBlocks,
                          row,
                          widthBlocks,
                          1,
                          color);
}

void drawPixelStar(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t cell, uint16_t color) {
    if (gfx == nullptr) {
        return;
    }

    gfx->fillRect(x, y, cell, cell, color);
    gfx->fillRect(static_cast<int16_t>(x + cell), y, cell, cell, color);
    gfx->fillRect(static_cast<int16_t>(x + (cell / 2)), static_cast<int16_t>(y - cell), cell, cell, color);
    gfx->fillRect(static_cast<int16_t>(x + (cell / 2)), static_cast<int16_t>(y + cell), cell, cell, color);
}

void drawRetroBackdrop(Arduino_GFX* gfx, int16_t w, int16_t h, uint16_t accent) {
    if (gfx == nullptr) {
        return;
    }

    (void)accent;
    gfx->fillScreen(kRetroBg);
    drawPixelStar(gfx, 96, 34, 3, kRetroStar);
    drawPixelStar(gfx, static_cast<int16_t>(w - 122), 48, 3, kRetroStar);
}

void drawBlinkLine(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, const EyeStyle& style) {
    const uint16_t blinkColor = mixColor565(style.accent, kHighlight, 24);
    const int16_t pad = 22;
    gfx->fillRoundRect(x + pad, y - style.blinkLineThickness, w - (pad * 2),
                       static_cast<int16_t>(style.blinkLineThickness * 2), 3, blinkColor);
    gfx->fillRect(x + 12, y - 4, 8, 8, kAccentMagenta);
    gfx->fillRect(x + w - 20, y - 4, 8, 8, kAccentCyan);
}

void drawRetroBlinkLine(Arduino_GFX* gfx, int16_t x, int16_t y, int16_t w, uint16_t accent) {
    if (gfx == nullptr) {
        return;
    }

    (void)accent;
    const uint16_t blinkColor = kInkDark;
    const int16_t lineY = static_cast<int16_t>(y - 2);
    gfx->fillRect(x + 24, lineY, w - 48, 4, blinkColor);
    gfx->fillRect(x + 16, lineY + 4, 12, 4, kRetroShadow);
    gfx->fillRect(x + w - 28, lineY + 4, 12, 4, kRetroShadow);
}

void drawRobotEye(Arduino_GFX* gfx,
                  int16_t x,
                  int16_t y,
                  int16_t w,
                  int16_t h,
                  float openRatio,
                  const EyeStyle& style,
                  int pupilOffsetX,
                  int pupilOffsetY,
                  bool mirrorDecor) {
    if (gfx == nullptr) {
        return;
    }

    float clampedOpen = openRatio;
    if (clampedOpen < 0.0f) {
        clampedOpen = 0.0f;
    } else if (clampedOpen > 1.0f) {
        clampedOpen = 1.0f;
    }

    const uint16_t shellEdge = mixColor565(style.accent, kHighlight, 34);
    const uint16_t moduleFill = mixColor565(kShellInner, style.accent, 20);
    const uint16_t screenTint = mixColor565(kScreenFill, style.accent, 18);
    const uint16_t screenScan = mixColor565(kScreenShade, style.accent, 26);
    const uint16_t irisFill = mixColor565(style.accent, kAccentCyan, 18);
    const uint16_t irisEdge = mixColor565(style.accent, kHighlight, 30);
    const uint16_t lowerGlow = mixColor565(style.accent, kAccentMagenta, 24);

    gfx->fillRoundRect(x, y, w, h, 18, moduleFill);
    gfx->drawRoundRect(x, y, w, h, 18, shellEdge);
    gfx->drawRoundRect(x + 3, y + 3, w - 6, h - 6, 14, mixColor565(kInkDark, style.accent, 62));

    const int16_t ledX = mirrorDecor ? static_cast<int16_t>(x + w - 16) : static_cast<int16_t>(x + 10);
    gfx->fillRect(ledX, y + 18, 6, 6, kAccentMagenta);
    gfx->fillRect(ledX, y + 30, 6, 6, style.accent);
    gfx->fillRect(ledX, y + 42, 6, 6, kAccentCyan);

    const int16_t screenX = static_cast<int16_t>(x + 18);
    const int16_t screenW = static_cast<int16_t>(w - 36);
    const int16_t maxScreenH = iMin(static_cast<int16_t>(h - 26),
                                    static_cast<int16_t>((h * style.openHeightRatio) / 100));
    const int16_t screenH = iMax(8, static_cast<int16_t>(maxScreenH * (0.14f + (clampedOpen * 0.86f))));
    const int16_t screenY = static_cast<int16_t>(y + ((h - screenH) / 2));

    if (screenH <= 14) {
        drawBlinkLine(gfx, x, static_cast<int16_t>(y + (h / 2)), w, style);
        return;
    }

    gfx->fillRoundRect(screenX, screenY, screenW, screenH, 14, screenTint);
    gfx->drawRoundRect(screenX, screenY, screenW, screenH, 14, mixColor565(shellEdge, kHighlight, 20));
    gfx->fillRoundRect(screenX + 10, screenY + 8, static_cast<int16_t>(screenW / 2), 6, 3,
                       mixColor565(kHighlight, screenTint, 88));
    gfx->fillRoundRect(screenX + 16, screenY + screenH - 11, screenW - 32, 4, 2, lowerGlow);

    for (int16_t yy = screenY + 8; yy < screenY + screenH - 6; yy += 6) {
        gfx->drawFastHLine(screenX + 10, yy, screenW - 20, screenScan);
    }

    const int16_t irisW = iMax(30, static_cast<int16_t>(style.irisRadius * 2 + 10));
    const int16_t irisH = iMax(22, iMin(static_cast<int16_t>(screenH - 18),
                                        static_cast<int16_t>(style.irisRadius * 2 + 4)));
    const int16_t irisX = clampInt(static_cast<int>(x + (w / 2) + pupilOffsetX - (irisW / 2)),
                                   screenX + 12,
                                   screenX + screenW - 12 - irisW);
    const int16_t irisY = clampInt(static_cast<int>(screenY + (screenH / 2) + pupilOffsetY - (irisH / 2)),
                                   screenY + 8,
                                   screenY + screenH - 8 - irisH);

    gfx->fillRoundRect(irisX, irisY, irisW, irisH, 8, irisFill);
    gfx->drawRoundRect(irisX, irisY, irisW, irisH, 8, irisEdge);

    const int16_t pupilW = iMax(8, static_cast<int16_t>(style.irisRadius / 2));
    const int16_t pupilH = iMax(12, static_cast<int16_t>(irisH - 10));
    const int16_t pupilX = static_cast<int16_t>(irisX + (irisW - pupilW) / 2);
    const int16_t pupilY = static_cast<int16_t>(irisY + (irisH - pupilH) / 2);
    gfx->fillRoundRect(pupilX, pupilY, pupilW, pupilH, 3, kInkDark);

    gfx->fillRect(irisX + 6, irisY + 5, 6, 6, kHighlight);
    gfx->fillRect(irisX + 14, irisY + 12, 3, 3, kHighlight);

    const int16_t cheekX = mirrorDecor ? static_cast<int16_t>(x + 18) : static_cast<int16_t>(x + w - 30);
    gfx->fillRect(cheekX, y + h - 18, 4, 4, kAccentMagenta);
    gfx->fillRect(static_cast<int16_t>(cheekX + 8), y + h - 18, 4, 4, kAccentCyan);
}

void drawRetroEye(Arduino_GFX* gfx,
                  int16_t x,
                  int16_t y,
                  int16_t w,
                  int16_t h,
                  float openRatio,
                  const EyeStyle& style,
                  int pupilOffsetX,
                  int pupilOffsetY,
                  bool mirrorDecor) {
    if (gfx == nullptr) {
        return;
    }

    (void)w;
    (void)h;

    float clampedOpen = openRatio;
    if (clampedOpen < 0.0f) {
        clampedOpen = 0.0f;
    } else if (clampedOpen > 1.0f) {
        clampedOpen = 1.0f;
    }

    constexpr int16_t cell = 6;
    constexpr int16_t gridW = 32;

    if (clampedOpen < 0.18f) {
        drawRetroBlinkLine(gfx,
                           x,
                           static_cast<int16_t>(y + 68),
                           static_cast<int16_t>(gridW * cell + 16),
                           style.accent);
        return;
    }

    const uint16_t eyeBorder = kInkDark;
    const uint16_t eyeWhite = rgb565(251, 252, 249);
    const uint16_t eyeGray = kRetroShadow;
    const uint16_t irisFill = style.accent;
    const uint16_t irisDark = mixColor565(style.accent, kInkDark, 72);
    const uint16_t irisMid = mixColor565(style.accent, kHighlight, 14);
    const uint16_t irisLight = mixColor565(style.accent, kHighlight, 84);
    const uint16_t irisGlow = mixColor565(style.accent, kHighlight, 132);
    constexpr int8_t kEyeStart[] = {11, 9, 7, 5, 3, 1, 0, 0, 1, 2, 4, 7, 10, 13};
    constexpr int8_t kEyeWidth[] = {8, 12, 16, 20, 24, 28, 30, 29, 27, 23, 18, 12, 7, 3};
    constexpr int8_t kTopLidStart[] = {10, 8, 6, 4, 3};
    constexpr int8_t kTopLidWidth[] = {10, 14, 18, 22, 24};
    constexpr int8_t kIrisStart[] = {10, 8, 6, 5, 5, 6, 8, 10};
    constexpr int8_t kIrisWidth[] = {8, 12, 16, 18, 18, 16, 12, 8};
    const int rowCount = static_cast<int>(sizeof(kEyeStart) / sizeof(kEyeStart[0]));
    const int lidRows = static_cast<int>(sizeof(kTopLidStart) / sizeof(kTopLidStart[0]));
    const int irisRows = static_cast<int>(sizeof(kIrisStart) / sizeof(kIrisStart[0]));
    const int16_t shapeX = static_cast<int16_t>(x + 8);
    const int16_t shapeY = static_cast<int16_t>(y + 26);

    for (int row = 0; row < rowCount; ++row) {
        fillMirroredPixelRow(gfx,
                             shapeX,
                             shapeY,
                             cell,
                             gridW,
                             mirrorDecor,
                             static_cast<int16_t>(kEyeStart[row] - 1),
                             row,
                             static_cast<int16_t>(kEyeWidth[row] + 2),
                             eyeBorder);
        fillMirroredPixelRow(gfx,
                             shapeX,
                             shapeY,
                             cell,
                             gridW,
                             mirrorDecor,
                             kEyeStart[row],
                             row,
                             kEyeWidth[row],
                             eyeWhite);
    }

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 29, 5, 3, 2, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 30, 3, 2, 2, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 27, 7, 4, 2, eyeBorder);

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 22, -4, 4, 1, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 18, -5, 5, 1, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 14, -6, 6, 1, eyeBorder);

    for (int row = 0; row < lidRows; ++row) {
        fillMirroredPixelRow(gfx,
                             shapeX,
                             shapeY,
                             cell,
                             gridW,
                             mirrorDecor,
                             kTopLidStart[row],
                             row,
                             kTopLidWidth[row],
                             eyeBorder);
    }

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 2, 1, 2, 1, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 1, 0, 2, 1, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 0, -1, 2, 1, eyeBorder);

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 24, 2, 4, 5, eyeGray);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 25, 7, 3, 3, eyeGray);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 8, 10, 13, 2, eyeGray);

    const int irisColOffset = clampInt(pupilOffsetX / 3, -2, 2);
    const int irisRowOffset = clampInt(pupilOffsetY / 3, -1, 1);
    const int irisBaseRow = 2 + irisRowOffset;
    const int pupilCol = 14 + irisColOffset;
    const int highlightCol = 10 + irisColOffset;

    for (int row = 0; row < irisRows; ++row) {
        uint16_t bandColor = irisMid;
        if (row <= 1) {
            bandColor = irisDark;
        } else if (row <= 4) {
            bandColor = irisFill;
        } else if (row <= 5) {
            bandColor = irisLight;
        } else if (row >= irisRows - 2) {
            bandColor = irisLight;
        }
        fillMirroredPixelRow(gfx,
                             shapeX,
                             shapeY,
                             cell,
                             gridW,
                             mirrorDecor,
                             static_cast<int16_t>(kIrisStart[row] + irisColOffset),
                             static_cast<int16_t>(irisBaseRow + row),
                             kIrisWidth[row],
                             bandColor);
    }

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 9 + irisColOffset, irisBaseRow + 5, 10, 1, irisGlow);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, 11 + irisColOffset, irisBaseRow + 6, 6, 1, irisGlow);

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, pupilCol, irisBaseRow + 1, 2, 7, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, pupilCol - 1, irisBaseRow + 2, 1, 5, eyeBorder);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, pupilCol + 2, irisBaseRow + 2, 1, 5, eyeBorder);

    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, highlightCol, irisBaseRow + 1, 2, 2, kHighlight);
    fillMirroredPixelRect(gfx, shapeX, shapeY, cell, gridW, mirrorDecor, highlightCol + 2, irisBaseRow + 3, 1, 1, kHighlight);
}

void renderEyesDisplay(const mood::MoodState& mood,
                       bool eyesOpen,
                       const air_quality::AirSample& sample,
                       const air_quality::AirQualityScore& score,
                       HomeEyesStyle visualStyle) {
    (void)eyesOpen;
    (void)sample;
    (void)score;

    Arduino_GFX* gfx = display::gfx();
    if (gfx == nullptr) {
        return;
    }

    const uint16_t w = gfx->width();
    const uint16_t h = gfx->height();
    const EyeStyle style = styleFor(mood);

    const unsigned long nowMs = millis();
    const MotionSample motion = sampleMotion(mood, nowMs);
    const float driftX = sinf(static_cast<float>(nowMs) * 0.0017f) * 1.8f;
    const float driftY = cosf(static_cast<float>(nowMs) * 0.0012f) * 1.2f;
    const int lookX = static_cast<int>((motion.lookX * 1.3f) + driftX);
    const int lookY = static_cast<int>((motion.lookY * 1.2f) + driftY);

    if (visualStyle == HomeEyesStyle::PixelRetro) {
        drawRetroBackdrop(gfx, static_cast<int16_t>(w), static_cast<int16_t>(h), style.accent);

        const int16_t eyeW = 205;
        const int16_t eyeH = 142;
        const int16_t leftX = static_cast<int16_t>((w / 2) - eyeW - 6);
        const int16_t faceY = static_cast<int16_t>((h / 2) - (eyeH / 2) - 3);
        const int16_t rightX = static_cast<int16_t>((w / 2) + 6);
        const int retroLookX = static_cast<int>(lookX * 0.28f);
        const int retroLookY = static_cast<int>(lookY * 0.22f);

        drawRetroEye(gfx, leftX, faceY, eyeW, eyeH, motion.eyeOpenRatio, style, retroLookX, retroLookY, false);
        drawRetroEye(gfx, rightX, faceY, eyeW, eyeH, motion.eyeOpenRatio, style, retroLookX, retroLookY, true);
        return;
    }

    drawBackdrop(gfx, static_cast<int16_t>(w), static_cast<int16_t>(h), style.accent);

    const int16_t faceX = 32;
    const int16_t faceY = 34;
    const int16_t faceW = static_cast<int16_t>(w - 64);
    const int16_t faceH = static_cast<int16_t>(h - 68);
    const int16_t bridgeW = 34;
    const int16_t eyeGap = 28;
    const int16_t eyeW = static_cast<int16_t>((faceW - bridgeW - eyeGap) / 2);
    const int16_t eyeH = faceH;
    const int16_t leftX = faceX;
    const int16_t rightX = static_cast<int16_t>(faceX + eyeW + eyeGap + bridgeW);

    const uint16_t bridgeFill = mixColor565(kShellFill, style.accent, 18);
    const uint16_t bridgeBorder = mixColor565(style.accent, kHighlight, 24);
    const int16_t bridgeX = static_cast<int16_t>(leftX + eyeW + (eyeGap / 2));
    const int16_t bridgeY = static_cast<int16_t>(faceY + 10);
    const int16_t bridgeH = static_cast<int16_t>(faceH - 20);

    gfx->fillRoundRect(bridgeX, bridgeY, bridgeW, bridgeH, 9, bridgeFill);
    gfx->drawRoundRect(bridgeX, bridgeY, bridgeW, bridgeH, 9, bridgeBorder);
    gfx->fillRect(bridgeX + 10, bridgeY + 16, 14, 8, kAccentMagenta);
    gfx->fillRect(bridgeX + 10, bridgeY + 32, 14, 8, style.accent);
    gfx->fillRect(bridgeX + 10, bridgeY + 48, 14, 8, kAccentCyan);

    drawRobotEye(gfx, leftX, faceY, eyeW, eyeH, motion.eyeOpenRatio, style, lookX, lookY, false);
    drawRobotEye(gfx, rightX, faceY, eyeW, eyeH, motion.eyeOpenRatio, style, lookX, lookY, true);
}
#endif

void renderEyesSerial(const mood::MoodState& mood,
                      bool eyesOpen,
                      const air_quality::AirSample& sample,
                      const air_quality::AirQualityScore& score) {
    Serial.println();
    Serial.println("========== HOME ==========");
    Serial.print("Mode: Eyes");
    Serial.print(" | Mood: ");
    Serial.println(mood.label);
    Serial.print("Air: ");
    Serial.println(air_quality::severityLabel(score.overall));
    if (sample.valid) {
        Serial.print("CO2:");
        Serial.print(sample.co2ppm);
        Serial.print("ppm  Temp:");
        Serial.print(sample.temperatureC, 1);
        Serial.print("C  RH:");
        Serial.print(sample.humidityPct, 1);
        Serial.print("%  VOC:");
        Serial.println(sample.vocIndex);
    } else {
        Serial.println("No valid sample yet.");
    }
    Serial.println();
    Serial.print(eyesOpen ? mood.eyesOpen : mood.eyesClosed);
    Serial.println("   [pixel visor]");
    Serial.println("[robot eyes are reacting to air quality]");
    Serial.println("==========================");
}

}  // namespace

void HomeEyesRenderer::render(const mood::MoodState& mood,
                              bool eyesOpen,
                              const air_quality::AirSample& sample,
                              const air_quality::AirQualityScore& score,
                              HomeEyesStyle style) {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    if (display::isReady()) {
        renderEyesDisplay(mood, eyesOpen, sample, score, style);
        return;
    }
#endif
    renderEyesSerial(mood, eyesOpen, sample, score);
}
