#pragma once

#include <stdint.h>
#include <stdio.h>

namespace ui {

enum class AppPage : uint8_t {
    HOME_EYES = 0,
    HOME_DETAIL = 1,
    FOCUS = 2,
    TIMER = 3,
    STATUS = 4,
    FOCUS_SETTINGS = 5,
    TIMER_SETTINGS = 6,
    TIME_EDITOR = 7,
};

enum class FocusSettingsOption : uint8_t {
    FOCUS_PRESET = 0,
    BREAK_PRESET = 1,
    START = 2,
    COUNT = 3,
};

enum class TimerSettingsOption : uint8_t {
    TIMER_PRESET = 0,
    START = 1,
    COUNT = 2,
};

enum class TimeEditorTarget : uint8_t {
    NONE = 0,
    FOCUS_PRESET = 1,
    BREAK_PRESET = 2,
    TIMER_PRESET = 3,
};

struct CountdownState {
    uint32_t totalMs;
    uint32_t remainingMs;
    bool running;

    CountdownState() : totalMs(0), remainingMs(0), running(false) {}
};

struct SettingsState {
    uint32_t focusPresetSeconds = 25 * 60;
    uint32_t focusBreakSeconds = 5 * 60;
    uint32_t timerPresetSeconds = 5 * 60;
    uint8_t selectedOption = 0;
};

struct TimeEditorState {
    TimeEditorTarget target = TimeEditorTarget::NONE;
    AppPage returnPage = AppPage::FOCUS_SETTINGS;
    uint32_t draftSeconds = 0;
    uint8_t selectedField = 1;
};

class PageManager {
public:
    void begin();

    AppPage page() const;
    void setPage(AppPage nextPage);
    void nextPage();
    void previousPage();
    void toggleHomeMode();

    void update(unsigned long nowMs);

    const CountdownState& focusState() const;
    const CountdownState& breakState() const;
    const CountdownState& timerState() const;

    bool isFocusRunning() const;
    bool isFocusInBreak() const;
    bool isFocusSessionActive() const;
    bool isFocusComplete() const;
    bool isTimerRunning() const;
    bool isTimerSessionActive() const;

    void startFocus(uint32_t seconds);
    void pauseResumeFocus();
    void resetFocus();

    void startTimer(uint32_t seconds);
    void pauseResumeTimer();
    void resetTimer();

    const char* statusText() const;
    void nextStatus();
    void nextStatusEffect();
    uint8_t statusEffectMode() const;

    void enterFocusSettings();
    void enterTimerSettings();
    void exitSettings();
    void nextSettingsOption();
    void previousSettingsOption();
    void setSettingsOption(uint8_t option);
    void tapSettingsAction();
    void adjustCurrentSetting(int8_t delta);
    const SettingsState& settingsState() const;
    void openTimeEditorForOption(uint8_t option);
    void cancelTimeEditor();
    void saveTimeEditor();
    void setTimeEditorField(uint8_t field);
    void adjustTimeEditorField(int8_t delta);
    const TimeEditorState& timeEditorState() const;
    bool isInSettings() const;
    static bool isSettingsPage(AppPage page);
    static bool isTimeEditorPage(AppPage page);

private:
    enum class FocusPhase : uint8_t {
        IDLE = 0,
        FOCUS = 1,
        BREAK = 2,
        COMPLETE = 3,
    };

    void tickCountdown(CountdownState& state, unsigned long elapsedMs);
    static uint32_t clampPresetSeconds(uint32_t value, uint32_t min, uint32_t max, uint32_t step);
    static uint32_t nextPresetStep(uint32_t value, uint32_t step, uint32_t min, uint32_t max, int8_t delta);
    uint8_t focusSettingsOptionCount() const;
    uint8_t timerSettingsOptionCount() const;
    uint8_t settingsOptionCountForPage() const;
    bool isFocusStartOption() const;
    bool isTimerStartOption() const;
    TimeEditorTarget timeEditorTargetForOption(uint8_t option) const;
    uint32_t secondsForTimeEditorTarget(TimeEditorTarget target) const;
    static AppPage settingsPageForTimeEditorTarget(TimeEditorTarget target);
    void openTimeEditor(TimeEditorTarget target);
    void loadPersistedSettings();
    void persistSettings() const;

    AppPage page_ = AppPage::HOME_EYES;
    CountdownState focus_;
    CountdownState break_;
    CountdownState timer_;
    SettingsState settings_;
    TimeEditorState timeEditor_;
    unsigned long lastTickMs_ = 0;
    uint16_t statusIndex_ = 0;
    uint8_t statusEffectIndex_ = 0;
    FocusPhase focusPhase_ = FocusPhase::IDLE;
};

}  // namespace ui
