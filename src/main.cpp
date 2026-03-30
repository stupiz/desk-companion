#include <Arduino.h>
#include <math.h>

#include "config/app_config.h"
#include "air_quality_model.h"
#include "display_manager.h"
#include "input_manager.h"
#include "mood_engine.h"
#include "page_manager.h"
#include "sensor_manager.h"

#include "render/focus_renderer.h"
#include "render/home_detail_renderer.h"
#include "render/home_eyes_renderer.h"
#include "render/settings_renderer.h"
#include "render/status_renderer.h"
#include "render/time_editor_renderer.h"
#include "render/timer_renderer.h"

namespace {
    SensorManager gSensors;
    ui::PageManager gPageManager;

    HomeEyesRenderer gHomeEyesRenderer;
    HomeDetailRenderer gHomeDetailRenderer;
    FocusRenderer gFocusRenderer;
    TimerRenderer gTimerRenderer;
    StatusRenderer gStatusRenderer;
    SettingsRenderer gSettingsRenderer;
    TimeEditorRenderer gTimeEditorRenderer;

    air_quality::AirQualityScore gAirQualityScore;
    air_quality::AirSample gSample;
    mood::MoodState gMood;
    HomeEyesStyle gHomeEyesStyle = HomeEyesStyle::PixelRetro;

    unsigned long gLastRenderMs = 0;
    unsigned long gLastTouchPollMs = 0;
    unsigned long gLastBlinkMs = 0;
    bool gEyesOpen = true;
    input::TouchManager gTouchManager;
    uint64_t gLastRenderSignature = 0;
    bool gForceRender = true;

    struct TouchCalibrationSession {
        bool active = false;
        bool fingerDown = false;
        uint8_t currentPoint = 0;
        int32_t totalRawX = 0;
        int32_t totalRawY = 0;
        uint16_t sampleCount = 0;
        int16_t rawX[4] = {0, 0, 0, 0};
        int16_t rawY[4] = {0, 0, 0, 0};
    };

    TouchCalibrationSession gTouchCalibration;

    const char* timeEditorHitLabel(TimeEditorHitType type) {
        switch (type) {
            case TimeEditorHitType::SelectField:
                return "select";
            case TimeEditorHitType::AdjustField:
                return "adjust";
            case TimeEditorHitType::Save:
                return "save";
            case TimeEditorHitType::Cancel:
                return "cancel";
            case TimeEditorHitType::None:
            default:
                return "none";
        }
    }

    uint64_t mixHash(uint64_t seed, uint32_t value) {
        seed ^= static_cast<uint64_t>(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

    uint32_t tenthsOrZero(float value) {
        if (isnan(value)) {
            return 0;
        }
        return static_cast<uint32_t>(value * 10.0f);
    }

    uint64_t hashText(uint64_t seed, const char* text) {
        if (text == nullptr) {
            return mixHash(seed, 0);
        }
        while (*text != '\0') {
            seed = mixHash(seed, static_cast<uint8_t>(*text));
            ++text;
        }
        return seed;
    }

    uint64_t renderSignature(unsigned long nowMs) {
        uint64_t signature = 1469598103934665603ULL;
        signature = mixHash(signature, static_cast<uint8_t>(gPageManager.page()));

        if (gTouchCalibration.active) {
            signature = mixHash(signature, 0xCA11U);
            signature = mixHash(signature, gTouchCalibration.currentPoint);
            signature = mixHash(signature, gTouchCalibration.fingerDown ? 1U : 0U);
            signature = mixHash(signature, gTouchCalibration.sampleCount);
            return signature;
        }

        switch (gPageManager.page()) {
            case ui::AppPage::HOME_EYES:
                signature = mixHash(signature, static_cast<uint8_t>(gMood.eyeMood));
                signature = mixHash(signature, static_cast<uint8_t>(gMood.severity));
                signature = mixHash(signature, gSample.valid ? 1U : 0U);
                signature = mixHash(signature, static_cast<uint8_t>(gAirQualityScore.overall));
                signature = mixHash(signature, gEyesOpen ? 1U : 0U);
                signature = mixHash(signature, static_cast<uint8_t>(gHomeEyesStyle));
                signature = mixHash(signature, static_cast<uint32_t>(nowMs / 120));
                break;
            case ui::AppPage::HOME_DETAIL:
                signature = mixHash(signature, gSample.valid ? 1U : 0U);
                signature = mixHash(signature, gSample.co2ppm);
                signature = mixHash(signature, tenthsOrZero(gSample.temperatureC));
                signature = mixHash(signature, tenthsOrZero(gSample.humidityPct));
                signature = mixHash(signature, gSample.vocIndex);
                signature = mixHash(signature, static_cast<uint8_t>(gAirQualityScore.overall));
                signature = mixHash(signature, gSensors.sgpWarmingUp() ? 1U : 0U);
                break;
            case ui::AppPage::FOCUS:
                signature = mixHash(signature, gPageManager.focusState().totalMs / 1000);
                signature = mixHash(signature, gPageManager.focusState().remainingMs / 1000);
                signature = mixHash(signature, gPageManager.breakState().totalMs / 1000);
                signature = mixHash(signature, gPageManager.breakState().remainingMs / 1000);
                signature = mixHash(signature, gPageManager.isFocusRunning() ? 1U : 0U);
                signature = mixHash(signature, gPageManager.isFocusInBreak() ? 1U : 0U);
                signature = mixHash(signature, gPageManager.isFocusComplete() ? 1U : 0U);
                signature = mixHash(signature, gPageManager.isFocusSessionActive() ? 1U : 0U);
                break;
            case ui::AppPage::TIMER:
                signature = mixHash(signature, gPageManager.timerState().totalMs / 1000);
                signature = mixHash(signature, gPageManager.timerState().remainingMs / 1000);
                signature = mixHash(signature, gPageManager.isTimerRunning() ? 1U : 0U);
                signature = mixHash(signature, gPageManager.isTimerSessionActive() ? 1U : 0U);
                break;
            case ui::AppPage::STATUS:
                signature = hashText(signature, gPageManager.statusText());
                signature = mixHash(signature, gPageManager.statusEffectMode());
                signature = mixHash(signature, static_cast<uint8_t>(gAirQualityScore.overall));
                if (gPageManager.statusEffectMode() != 0) {
                    signature = mixHash(signature, static_cast<uint32_t>(nowMs / 100));
                }
                break;
            case ui::AppPage::FOCUS_SETTINGS:
            case ui::AppPage::TIMER_SETTINGS:
                signature = mixHash(signature, gPageManager.settingsState().focusPresetSeconds);
                signature = mixHash(signature, gPageManager.settingsState().focusBreakSeconds);
                signature = mixHash(signature, gPageManager.settingsState().timerPresetSeconds);
                signature = mixHash(signature, gPageManager.settingsState().selectedOption);
                break;
            case ui::AppPage::TIME_EDITOR:
                signature = mixHash(signature, static_cast<uint8_t>(gPageManager.timeEditorState().target));
                signature = mixHash(signature, gPageManager.timeEditorState().draftSeconds);
                signature = mixHash(signature, gPageManager.timeEditorState().selectedField);
                break;
        }

        return signature;
    }

    void printHelp() {
        Serial.println();
        Serial.println("Desk Companion Controls:");
        Serial.println("1) Home Eyes   2) Home Detail   3) Focus   4) Timer   5) Status");
        Serial.println("Swipe left/right: reserved");
        Serial.println("Home: tap switch eyes/detail | swipe up Focus");
        Serial.println("Focus: tap settings/ pause-resume | long press reset | swipe up Timer | swipe down Home");
        Serial.println("Timer: tap settings/ pause-resume | long press reset | swipe up Status | swipe down Focus");
        Serial.println("Status: tap cycle status (BUSY -> BREAK -> MEETING) | swipe down Timer");
        Serial.println("Settings: tap card/button to edit or start | swipe down back");
        Serial.println("Time Editor: tap HH/MM/SS then use +/- on right | tap SAVE/BACK");
        Serial.println("Long press Status: cycle status text effect");
        Serial.println("f) Start Focus (using current focus preset)");
        Serial.println("F) Open Focus settings");
        Serial.println("t) Start Timer (using current timer preset)");
        Serial.println("T) Open Timer settings");
        Serial.println("i) Rescan sensor buses / dump I2C devices");
        Serial.println("d) Toggle touch debug logs");
        Serial.println("c) Start touch calibration   C) Restore built-in touch calibration");
        Serial.println("In Settings: [/] for options, +/- adjust, b to return");
        Serial.println("In Time Editor: [/] field, +/- adjust, s save, b back");
        Serial.println("x: Pause/Resume Focus   r: Reset Focus to 00:00");
        Serial.println("u/v: Pause/Reset Timer");
        Serial.println("n/p: Next/Prev page");
        Serial.println("e): print this help");
        Serial.println();
    }

    void calibrationPointPosition(uint8_t pointIndex, int16_t& xOut, int16_t& yOut) {
        const int16_t width = static_cast<int16_t>(display::width());
        const int16_t height = static_cast<int16_t>(display::height());
        const int16_t marginX = width > 120 ? 24 : 14;
        const int16_t marginY = height > 120 ? 24 : 14;

        switch (pointIndex) {
            case 0:
                xOut = marginX;
                yOut = marginY;
                break;
            case 1:
                xOut = static_cast<int16_t>(width - 1 - marginX);
                yOut = marginY;
                break;
            case 2:
                xOut = marginX;
                yOut = static_cast<int16_t>(height - 1 - marginY);
                break;
            case 3:
            default:
                xOut = static_cast<int16_t>(width - 1 - marginX);
                yOut = static_cast<int16_t>(height - 1 - marginY);
                break;
        }
    }

    bool computeTouchCalibration(input::TouchCalibrationData& data) {
        const uint16_t deltaX = static_cast<uint16_t>(
            abs(gTouchCalibration.rawX[0] - gTouchCalibration.rawX[1]));
        const uint16_t deltaY = static_cast<uint16_t>(
            abs(gTouchCalibration.rawY[0] - gTouchCalibration.rawY[1]));

        data.valid = true;
        if (deltaX > deltaY) {
            data.swapXY = false;
            data.mapX1 = static_cast<int16_t>((gTouchCalibration.rawX[0] + gTouchCalibration.rawX[2]) / 2);
            data.mapX2 = static_cast<int16_t>((gTouchCalibration.rawX[1] + gTouchCalibration.rawX[3]) / 2);
            data.mapY1 = static_cast<int16_t>((gTouchCalibration.rawY[0] + gTouchCalibration.rawY[1]) / 2);
            data.mapY2 = static_cast<int16_t>((gTouchCalibration.rawY[2] + gTouchCalibration.rawY[3]) / 2);
        } else {
            data.swapXY = true;
            data.mapX1 = static_cast<int16_t>((gTouchCalibration.rawY[0] + gTouchCalibration.rawY[2]) / 2);
            data.mapX2 = static_cast<int16_t>((gTouchCalibration.rawY[1] + gTouchCalibration.rawY[3]) / 2);
            data.mapY1 = static_cast<int16_t>((gTouchCalibration.rawX[0] + gTouchCalibration.rawX[1]) / 2);
            data.mapY2 = static_cast<int16_t>((gTouchCalibration.rawX[2] + gTouchCalibration.rawX[3]) / 2);
        }

        uint16_t spanX = static_cast<uint16_t>(abs(data.mapX1 - data.mapX2));
        uint16_t spanY = static_cast<uint16_t>(abs(data.mapY1 - data.mapY2));
        const int16_t expandX = static_cast<int16_t>(spanX / 6U);
        const int16_t expandY = static_cast<int16_t>(spanY / 6U);

        if (data.mapX1 > data.mapX2) {
            data.mapX1 = static_cast<int16_t>(data.mapX1 + expandX);
            data.mapX2 = static_cast<int16_t>(data.mapX2 - expandX);
        } else {
            data.mapX1 = static_cast<int16_t>(data.mapX1 - expandX);
            data.mapX2 = static_cast<int16_t>(data.mapX2 + expandX);
        }

        if (data.mapY1 > data.mapY2) {
            data.mapY1 = static_cast<int16_t>(data.mapY1 + expandY);
            data.mapY2 = static_cast<int16_t>(data.mapY2 - expandY);
        } else {
            data.mapY1 = static_cast<int16_t>(data.mapY1 - expandY);
            data.mapY2 = static_cast<int16_t>(data.mapY2 + expandY);
        }

        return true;
    }

    void printTouchCalibrationResult(const input::TouchCalibrationData& data) {
        Serial.println("[CAL] Applied touch calibration:");
        Serial.printf("[CAL] swapXY=%s\n", data.swapXY ? "true" : "false");
        Serial.printf("[CAL] mapX1=%d mapX2=%d mapY1=%d mapY2=%d\n",
                      data.mapX1, data.mapX2, data.mapY1, data.mapY2);
    }

    void startTouchCalibration() {
        gTouchCalibration = TouchCalibrationSession{};
        gTouchCalibration.active = true;
        Serial.println("[CAL] Touch calibration started.");
        Serial.println("[CAL] Tap the red X at each corner: TL, TR, BL, BR.");
        gForceRender = true;
    }

    void updateTouchCalibration() {
        if (!gTouchCalibration.active) {
            return;
        }

        int16_t rawX = 0;
        int16_t rawY = 0;
        const bool touched = input::readRawPoint(rawX, rawY);

        if (touched) {
            if (!gTouchCalibration.fingerDown) {
                gTouchCalibration.fingerDown = true;
                gTouchCalibration.totalRawX = rawX;
                gTouchCalibration.totalRawY = rawY;
                gTouchCalibration.sampleCount = 1;
            } else {
                gTouchCalibration.totalRawX += rawX;
                gTouchCalibration.totalRawY += rawY;
                ++gTouchCalibration.sampleCount;
            }
            return;
        }

        if (!gTouchCalibration.fingerDown) {
            return;
        }

        const uint8_t pointIndex = gTouchCalibration.currentPoint;
        gTouchCalibration.fingerDown = false;
        gTouchCalibration.rawX[pointIndex] = static_cast<int16_t>(gTouchCalibration.totalRawX / gTouchCalibration.sampleCount);
        gTouchCalibration.rawY[pointIndex] = static_cast<int16_t>(gTouchCalibration.totalRawY / gTouchCalibration.sampleCount);

        Serial.printf("[CAL] point=%u raw=(%d,%d) samples=%u\n",
                      static_cast<unsigned>(pointIndex),
                      gTouchCalibration.rawX[pointIndex],
                      gTouchCalibration.rawY[pointIndex],
                      static_cast<unsigned>(gTouchCalibration.sampleCount));

        ++gTouchCalibration.currentPoint;
        gTouchCalibration.totalRawX = 0;
        gTouchCalibration.totalRawY = 0;
        gTouchCalibration.sampleCount = 0;
        gForceRender = true;

        if (gTouchCalibration.currentPoint >= 4) {
            input::TouchCalibrationData data;
            if (computeTouchCalibration(data)) {
                input::setCalibration(data);
                printTouchCalibrationResult(data);
            }
            gTouchCalibration.active = false;
            Serial.println("[CAL] Calibration finished.");
            gForceRender = true;
        }
    }

    void renderTouchCalibration() {
        if (!display::isReady()) {
            Serial.println("[CAL] Touch corners in order: TL, TR, BL, BR");
            return;
        }

        Arduino_GFX* gfx = display::gfx();
        if (gfx == nullptr) {
            return;
        }

        constexpr uint16_t kBg = 0x0000;
        constexpr uint16_t kFg = 0xFFFF;
        constexpr uint16_t kDim = 0x8410;
        constexpr uint16_t kDone = 0x07E0;
        constexpr uint16_t kCurrent = 0xF800;

        gfx->fillScreen(kBg);
        display::drawTextCentered("TOUCH CALIBRATION", display::width() / 2, 6, kFg, 1, kBg);
        display::drawTextCentered("Tap TL, TR, BL, BR in order", display::width() / 2, 20, kDim, 1, kBg);

        for (uint8_t i = 0; i < 4; ++i) {
            int16_t x = 0;
            int16_t y = 0;
            calibrationPointPosition(i, x, y);
            const bool done = i < gTouchCalibration.currentPoint;
            const bool current = i == gTouchCalibration.currentPoint;
            const uint16_t color = done ? kDone : (current ? kCurrent : kDim);
            gfx->drawLine(x - 8, y, x + 8, y, color);
            gfx->drawLine(x, y - 8, x, y + 8, color);
        }

        char stepText[24];
        snprintf(stepText, sizeof(stepText), "STEP %u / 4", static_cast<unsigned>(gTouchCalibration.currentPoint + 1));
        if (gTouchCalibration.currentPoint >= 4) {
            snprintf(stepText, sizeof(stepText), "APPLYING...");
        }
        display::drawTextCentered(stepText, display::width() / 2, static_cast<int16_t>(display::height() - 22), kFg, 1, kBg);
    }

    void processSerialCommands() {
        while (Serial.available() > 0) {
            char c = Serial.read();
            switch (c) {
                case '1':
                    gPageManager.setPage(ui::AppPage::HOME_EYES);
                    gForceRender = true;
                    break;
                case '2':
                    gPageManager.setPage(ui::AppPage::HOME_DETAIL);
                    gForceRender = true;
                    break;
                case '3':
                    gPageManager.setPage(ui::AppPage::FOCUS);
                    gForceRender = true;
                    break;
                case '4':
                    gPageManager.setPage(ui::AppPage::TIMER);
                    gForceRender = true;
                    break;
                case '5':
                    gPageManager.setPage(ui::AppPage::STATUS);
                    gForceRender = true;
                    break;
                case 'f':
                    if (gPageManager.page() == ui::AppPage::FOCUS) {
                        gPageManager.startFocus(gPageManager.settingsState().focusPresetSeconds);
                    }
                    break;
                case 'F':
                    gPageManager.enterFocusSettings();
                    gForceRender = true;
                    break;
                case 't':
                    if (gPageManager.page() == ui::AppPage::TIMER) {
                        gPageManager.startTimer(gPageManager.settingsState().timerPresetSeconds);
                    }
                    break;
                case 'T':
                    gPageManager.enterTimerSettings();
                    gForceRender = true;
                    break;
                case 'i':
                case 'I':
                    if (gSensors.rescan()) {
                        Serial.println("Sensors initialized.");
                    } else {
                        Serial.println("Sensor init failed (check wiring/power/SDA/SCL and sensor ID).");
                    }
                    gSample = gSensors.latestSample();
                    gAirQualityScore = gSensors.latestScore();
                    gMood = mood::resolveMood(gAirQualityScore.overall);
                    gForceRender = true;
                    break;
                case 'd':
                case 'D':
                    gTouchManager.setDebugEnabled(!gTouchManager.debugEnabled());
                    Serial.print("Touch debug: ");
                    Serial.println(gTouchManager.debugEnabled() ? "ON" : "OFF");
                    break;
                case 'c':
                    startTouchCalibration();
                    break;
                case 'C':
                    input::clearCalibration();
                    Serial.println("[CAL] Restored built-in touch calibration.");
                    gForceRender = true;
                    break;
                case 'x':
                    gPageManager.pauseResumeFocus();
                    break;
                case 'r':
                    gPageManager.resetFocus();
                    break;
                case 'u':
                    gPageManager.pauseResumeTimer();
                    break;
                case 'v':
                    gPageManager.resetTimer();
                    break;
                case '[':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        const uint8_t currentField = gPageManager.timeEditorState().selectedField;
                        gPageManager.setTimeEditorField(currentField == 0 ? 2 : currentField - 1);
                    } else {
                        gPageManager.previousSettingsOption();
                    }
                    gForceRender = true;
                    break;
                case ']':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        const uint8_t currentField = gPageManager.timeEditorState().selectedField;
                        gPageManager.setTimeEditorField(static_cast<uint8_t>((currentField + 1) % 3));
                    } else {
                        gPageManager.nextSettingsOption();
                    }
                    gForceRender = true;
                    break;
                case '+':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        gPageManager.adjustTimeEditorField(1);
                    } else {
                        gPageManager.adjustCurrentSetting(1);
                    }
                    gForceRender = true;
                    break;
                case '-':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        gPageManager.adjustTimeEditorField(-1);
                    } else {
                        gPageManager.adjustCurrentSetting(-1);
                    }
                    gForceRender = true;
                    break;
                case 's':
                case 'S':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        gPageManager.saveTimeEditor();
                        gForceRender = true;
                    }
                    break;
                case 'b':
                case 'B':
                    if (gPageManager.page() == ui::AppPage::TIME_EDITOR) {
                        gPageManager.cancelTimeEditor();
                    } else {
                        gPageManager.exitSettings();
                    }
                    gForceRender = true;
                    break;
                case 'h':
                case 'H':
                    gPageManager.toggleHomeMode();
                    gForceRender = true;
                    break;
                case 'n':
                    gPageManager.nextPage();
                    gForceRender = true;
                    break;
                case 'p':
                    gPageManager.previousPage();
                    gForceRender = true;
                    break;
                case 'e':
                case '?':
                    printHelp();
                    break;
            }
        }
    }

    void processTouchAction(input::TouchAction action) {
        const ui::AppPage currentPage = gPageManager.page();

        switch (action) {
            case input::TouchAction::SWIPE_UP:
                switch (currentPage) {
                    case ui::AppPage::HOME_EYES:
                    case ui::AppPage::HOME_DETAIL:
                        gPageManager.setPage(ui::AppPage::FOCUS);
                        gForceRender = true;
                        break;
                    case ui::AppPage::FOCUS:
                        gPageManager.setPage(ui::AppPage::TIMER);
                        gForceRender = true;
                        break;
                    case ui::AppPage::TIMER:
                        gPageManager.setPage(ui::AppPage::STATUS);
                        gForceRender = true;
                        break;
                    default:
                        break;
                }
                break;
            case input::TouchAction::SWIPE_DOWN:
                switch (currentPage) {
                    case ui::AppPage::FOCUS:
                        gPageManager.setPage(ui::AppPage::HOME_EYES);
                        gForceRender = true;
                        break;
                    case ui::AppPage::TIMER:
                        gPageManager.setPage(ui::AppPage::FOCUS);
                        gForceRender = true;
                        break;
                    case ui::AppPage::STATUS:
                        gPageManager.setPage(ui::AppPage::TIMER);
                        gForceRender = true;
                        break;
                    case ui::AppPage::FOCUS_SETTINGS:
                    case ui::AppPage::TIMER_SETTINGS:
                        gPageManager.exitSettings();
                        gForceRender = true;
                        break;
                    default:
                        break;
                }
                break;
            case input::TouchAction::SWIPE_LEFT:
            case input::TouchAction::SWIPE_RIGHT:
                break;
            case input::TouchAction::LONG_PRESS:
                if (currentPage == ui::AppPage::FOCUS &&
                    gPageManager.isFocusSessionActive()) {
                    gPageManager.resetFocus();
                } else if (currentPage == ui::AppPage::TIMER &&
                           gPageManager.isTimerSessionActive()) {
                    gPageManager.resetTimer();
                } else if (currentPage == ui::AppPage::STATUS) {
                    gPageManager.nextStatusEffect();
                    gForceRender = true;
                }
                break;
            case input::TouchAction::TAP:
                switch (currentPage) {
                    case ui::AppPage::HOME_EYES:
                    case ui::AppPage::HOME_DETAIL:
                        gPageManager.toggleHomeMode();
                        gForceRender = true;
                        break;
                    case ui::AppPage::FOCUS:
                        if (gPageManager.isFocusSessionActive()) {
                            gPageManager.pauseResumeFocus();
                        } else {
                            gPageManager.enterFocusSettings();
                        }
                        gForceRender = true;
                        break;
                    case ui::AppPage::TIMER:
                        if (gPageManager.isTimerSessionActive()) {
                            gPageManager.pauseResumeTimer();
                        } else {
                            gPageManager.enterTimerSettings();
                        }
                        gForceRender = true;
                        break;
                    case ui::AppPage::STATUS:
                        gPageManager.nextStatus();
                        gForceRender = true;
                        break;
                    case ui::AppPage::FOCUS_SETTINGS:
                    case ui::AppPage::TIMER_SETTINGS: {
                        const SettingsHit hit = gSettingsRenderer.hitTest(
                            gTouchManager.lastActionX(),
                            gTouchManager.lastActionY(),
                            currentPage);
                        switch (hit.type) {
                            case SettingsHitType::Start:
                                gPageManager.setSettingsOption(hit.option);
                                gPageManager.tapSettingsAction();
                                break;
                            case SettingsHitType::EditTime:
                                gPageManager.openTimeEditorForOption(hit.option);
                                break;
                            case SettingsHitType::None:
                            default:
                                break;
                        }
                        gForceRender = true;
                        break;
                    }
                    case ui::AppPage::TIME_EDITOR: {
                        const int16_t tapX = gTouchManager.lastActionX();
                        const int16_t tapY = gTouchManager.lastActionY();
                        const TimeEditorHit hit = gTimeEditorRenderer.hitTest(
                            tapX,
                            tapY,
                            gPageManager.timeEditorState());
                        if (gTouchManager.debugEnabled()) {
                            Serial.printf("[TIME_EDITOR] tap=(%d,%d) hit=%s field=%u delta=%d selected=%u\n",
                                          tapX,
                                          tapY,
                                          timeEditorHitLabel(hit.type),
                                          static_cast<unsigned>(hit.field),
                                          static_cast<int>(hit.delta),
                                          static_cast<unsigned>(gPageManager.timeEditorState().selectedField));
                        }
                        switch (hit.type) {
                            case TimeEditorHitType::AdjustField:
                                gPageManager.setTimeEditorField(hit.field);
                                gPageManager.adjustTimeEditorField(hit.delta);
                                break;
                            case TimeEditorHitType::SelectField:
                                gPageManager.setTimeEditorField(hit.field);
                                break;
                            case TimeEditorHitType::Save:
                                gPageManager.saveTimeEditor();
                                break;
                            case TimeEditorHitType::Cancel:
                                gPageManager.cancelTimeEditor();
                                break;
                            case TimeEditorHitType::None:
                            default:
                                break;
                        }
                        gForceRender = true;
                        break;
                    }
                    default:
                        break;
                }
                break;
            case input::TouchAction::NONE:
            default:
                break;
        }
    }

    void renderCurrentPage(unsigned long nowMs) {
        if (gTouchCalibration.active) {
            renderTouchCalibration();
            return;
        }

        switch (gPageManager.page()) {
            case ui::AppPage::HOME_EYES:
                gHomeEyesRenderer.render(gMood, gEyesOpen, gSample, gAirQualityScore, gHomeEyesStyle);
                break;
            case ui::AppPage::HOME_DETAIL:
                gHomeDetailRenderer.render(
                    gSample,
                    gAirQualityScore,
                    gSensors.scdReady(),
                    gSensors.sgpReady(),
                    gSensors.sgpWarmingUp());
                break;
            case ui::AppPage::FOCUS:
                gFocusRenderer.render(
                    gPageManager.focusState(),
                    gPageManager.breakState(),
                    gPageManager.isFocusRunning(),
                    gPageManager.isFocusInBreak(),
                    gPageManager.isFocusComplete(),
                    gPageManager.isFocusSessionActive());
                break;
            case ui::AppPage::TIMER:
                gTimerRenderer.render(gPageManager.timerState());
                break;
            case ui::AppPage::STATUS:
                gStatusRenderer.render(
                    gPageManager.statusText(),
                    gPageManager.statusEffectMode(),
                    nowMs,
                    gAirQualityScore.overall);
                break;
            case ui::AppPage::FOCUS_SETTINGS:
            case ui::AppPage::TIMER_SETTINGS:
                gSettingsRenderer.render(gPageManager.settingsState(), gPageManager.page());
                break;
            case ui::AppPage::TIME_EDITOR:
                gTimeEditorRenderer.render(gPageManager.timeEditorState());
                break;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("Desk Companion booting...");
    if (display::begin()) {
        Serial.println("Display initialized.");
    }

    bool ok = gSensors.begin();
    if (!ok) {
        Serial.println("Sensor init failed (check wiring/power/SDA/SCL and sensor ID).");
    } else {
        Serial.println("Sensors initialized.");
    }

    gPageManager.begin();
    gPageManager.resetFocus();
    gPageManager.resetTimer();
    gTouchManager.begin();
    gMood = mood::resolveMood(air_quality::SeverityLevel::NORMAL);

    printHelp();
    Serial.println("Ready.");
    gForceRender = true;
}

void loop() {
    const unsigned long nowMs = millis();
    gSensors.update(nowMs);
    gPageManager.update(nowMs);
    processSerialCommands();

    if (nowMs - gLastTouchPollMs >= app::INPUT_POLL_MS) {
        gLastTouchPollMs = nowMs;
        if (gTouchCalibration.active) {
            updateTouchCalibration();
        } else {
            processTouchAction(gTouchManager.update(nowMs));
        }
    }

    const air_quality::AirSample& sample = gSensors.latestSample();
    const air_quality::AirQualityScore& score = gSensors.latestScore();
    if (sample.valid) {
        gSample = sample;
        gAirQualityScore = score;
        gMood = mood::resolveMood(score.overall);
    }

    if (nowMs - gLastBlinkMs >= gMood.blinkMs) {
        gLastBlinkMs = nowMs;
        gEyesOpen = !gEyesOpen;
    }

    const uint64_t signature = renderSignature(nowMs);
    if ((gForceRender || signature != gLastRenderSignature) &&
        (nowMs - gLastRenderMs >= 80)) {
        gLastRenderMs = nowMs;
        gLastRenderSignature = signature;
        gForceRender = false;
        renderCurrentPage(nowMs);
        display::present();
    }
}
