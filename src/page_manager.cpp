#include "page_manager.h"

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#endif

#include "config/app_config.h"
namespace ui {

namespace {
    const char* kStatusLabels[] = {
        "BUSY",
        "BREAK",
        "MEETING",
    };
    constexpr uint8_t kStatusCount = sizeof(kStatusLabels) / sizeof(kStatusLabels[0]);
    constexpr uint8_t kStatusEffectCount = 3;
    constexpr uint8_t kMainPageCount = 5;
#if defined(ARDUINO_ARCH_ESP32)
    constexpr char kPrefsNamespace[] = "deskcomp";
    constexpr char kFocusSecondsKey[] = "focus_sec";
    constexpr char kBreakSecondsKey[] = "break_sec";
    constexpr char kTimerSecondsKey[] = "timer_sec";
#endif

    uint32_t clampStoredSeconds(uint32_t value, uint32_t fallback) {
        if (value > 86399U) {
            return fallback;
        }
        return value;
    }

}

void PageManager::begin() {
    lastTickMs_ = millis();
    page_ = AppPage::HOME_EYES;
    settings_.focusPresetSeconds = app::FOCUS_DEFAULT_SECONDS;
    settings_.focusBreakSeconds = app::FOCUS_BREAK_DEFAULT_SECONDS;
    settings_.timerPresetSeconds = app::TIMER_DEFAULT_SECONDS;
    settings_.selectedOption = 0;
    timeEditor_.target = TimeEditorTarget::NONE;
    timeEditor_.returnPage = AppPage::FOCUS_SETTINGS;
    timeEditor_.draftSeconds = 0;
    timeEditor_.selectedField = 1;
    statusIndex_ = 0;
    statusEffectIndex_ = 0;
    focusPhase_ = FocusPhase::IDLE;
    focus_.remainingMs = 0;
    focus_.running = false;
    break_.remainingMs = 0;
    break_.running = false;
    loadPersistedSettings();
}

AppPage PageManager::page() const {
    return page_;
}

void PageManager::setPage(AppPage nextPage) {
    if (isInSettings() && isSettingsPage(nextPage)) {
        return;
    }
    page_ = nextPage;
}

void PageManager::nextPage() {
    if (isInSettings()) {
        return;
    }
    const uint8_t current = static_cast<uint8_t>(page_);
    const uint8_t next = (current == (kMainPageCount - 1)) ? 0 : current + 1;
    setPage(static_cast<AppPage>(next));
}

void PageManager::previousPage() {
    if (isInSettings()) {
        return;
    }
    const uint8_t current = static_cast<uint8_t>(page_);
    const uint8_t prev = (current == 0) ? (kMainPageCount - 1) : (current - 1);
    setPage(static_cast<AppPage>(prev));
}

void PageManager::toggleHomeMode() {
    if (page_ == AppPage::HOME_EYES) {
        page_ = AppPage::HOME_DETAIL;
    } else if (page_ == AppPage::HOME_DETAIL) {
        page_ = AppPage::HOME_EYES;
    }
}

void PageManager::update(unsigned long nowMs) {
    if (!lastTickMs_) {
        lastTickMs_ = nowMs;
        return;
    }

    const unsigned long elapsedMs = nowMs - lastTickMs_;
    if (focusPhase_ == FocusPhase::FOCUS) {
        tickCountdown(focus_, elapsedMs);
        if (!focus_.running && focus_.remainingMs == 0) {
            focusPhase_ = FocusPhase::BREAK;
            break_.running = break_.totalMs > 0;
            if (!break_.running) {
                focusPhase_ = FocusPhase::COMPLETE;
            }
        }
    } else if (focusPhase_ == FocusPhase::BREAK) {
        tickCountdown(break_, elapsedMs);
        if (!break_.running && break_.remainingMs == 0) {
            focusPhase_ = FocusPhase::COMPLETE;
        }
    }

    tickCountdown(timer_, elapsedMs);
    lastTickMs_ = nowMs;
}

void PageManager::tickCountdown(CountdownState& state, unsigned long elapsedMs) {
    if (!state.running || state.remainingMs == 0) {
        if (state.remainingMs == 0) {
            state.running = false;
        }
        return;
    }

    if (elapsedMs >= state.remainingMs) {
        state.remainingMs = 0;
        state.running = false;
    } else {
        state.remainingMs -= static_cast<uint32_t>(elapsedMs);
    }
}

const CountdownState& PageManager::focusState() const {
    return focus_;
}

const CountdownState& PageManager::breakState() const {
    return break_;
}

const CountdownState& PageManager::timerState() const {
    return timer_;
}

bool PageManager::isFocusRunning() const {
    if (focusPhase_ == FocusPhase::FOCUS) {
        return focus_.running;
    }
    if (focusPhase_ == FocusPhase::BREAK) {
        return break_.running;
    }
    return false;
}

bool PageManager::isFocusInBreak() const {
    return focusPhase_ == FocusPhase::BREAK;
}

bool PageManager::isFocusSessionActive() const {
    return focusPhase_ == FocusPhase::FOCUS || focusPhase_ == FocusPhase::BREAK;
}

bool PageManager::isFocusComplete() const {
    return focusPhase_ == FocusPhase::COMPLETE;
}

bool PageManager::isTimerRunning() const {
    return timer_.running;
}

bool PageManager::isTimerSessionActive() const {
    return timer_.remainingMs > 0;
}

bool PageManager::isSettingsPage(AppPage page) {
    return page == AppPage::FOCUS_SETTINGS || page == AppPage::TIMER_SETTINGS;
}

bool PageManager::isTimeEditorPage(AppPage page) {
    return page == AppPage::TIME_EDITOR;
}

bool PageManager::isInSettings() const {
    return isSettingsPage(page_) || isTimeEditorPage(page_);
}

void PageManager::enterFocusSettings() {
    if (page_ == AppPage::FOCUS) {
        settings_.selectedOption = 0;
        page_ = AppPage::FOCUS_SETTINGS;
    }
}

void PageManager::enterTimerSettings() {
    if (page_ == AppPage::TIMER) {
        settings_.selectedOption = 0;
        page_ = AppPage::TIMER_SETTINGS;
    }
}

void PageManager::exitSettings() {
    if (page_ == AppPage::FOCUS_SETTINGS) {
        page_ = AppPage::FOCUS;
        return;
    }
    if (page_ == AppPage::TIMER_SETTINGS) {
        page_ = AppPage::TIMER;
        return;
    }
    if (page_ == AppPage::TIME_EDITOR) {
        cancelTimeEditor();
    }
}

void PageManager::nextSettingsOption() {
    if (!isInSettings()) {
        return;
    }
    const uint8_t optionCount = settingsOptionCountForPage();
    if (optionCount == 0) {
        return;
    }
    settings_.selectedOption = static_cast<uint8_t>((settings_.selectedOption + 1) % optionCount);
}

void PageManager::nextStatus() {
    if (kStatusCount > 0) {
        statusIndex_ = static_cast<uint16_t>((statusIndex_ + 1) % kStatusCount);
    }
}

void PageManager::nextStatusEffect() {
    if (kStatusEffectCount > 0) {
        statusEffectIndex_ = static_cast<uint8_t>((statusEffectIndex_ + 1) % kStatusEffectCount);
    }
}

uint8_t PageManager::statusEffectMode() const {
    return statusEffectIndex_;
}

void PageManager::previousSettingsOption() {
    if (!isInSettings()) {
        return;
    }
    const uint8_t optionCount = settingsOptionCountForPage();
    if (optionCount == 0) {
        return;
    }

    if (settings_.selectedOption == 0) {
        settings_.selectedOption = static_cast<uint8_t>(optionCount - 1);
    } else {
        --settings_.selectedOption;
    }
}

void PageManager::setSettingsOption(uint8_t option) {
    if (!isInSettings()) {
        return;
    }

    const uint8_t optionCount = settingsOptionCountForPage();
    if (option >= optionCount) {
        return;
    }

    settings_.selectedOption = option;
}

TimeEditorTarget PageManager::timeEditorTargetForOption(uint8_t option) const {
    if (page_ == AppPage::FOCUS_SETTINGS) {
        if (option == static_cast<uint8_t>(FocusSettingsOption::FOCUS_PRESET)) {
            return TimeEditorTarget::FOCUS_PRESET;
        }
        if (option == static_cast<uint8_t>(FocusSettingsOption::BREAK_PRESET)) {
            return TimeEditorTarget::BREAK_PRESET;
        }
    }

    if (page_ == AppPage::TIMER_SETTINGS &&
        option == static_cast<uint8_t>(TimerSettingsOption::TIMER_PRESET)) {
        return TimeEditorTarget::TIMER_PRESET;
    }

    return TimeEditorTarget::NONE;
}

uint32_t PageManager::secondsForTimeEditorTarget(TimeEditorTarget target) const {
    switch (target) {
        case TimeEditorTarget::FOCUS_PRESET:
            return settings_.focusPresetSeconds;
        case TimeEditorTarget::BREAK_PRESET:
            return settings_.focusBreakSeconds;
        case TimeEditorTarget::TIMER_PRESET:
            return settings_.timerPresetSeconds;
        case TimeEditorTarget::NONE:
        default:
            return 0;
    }
}

AppPage PageManager::settingsPageForTimeEditorTarget(TimeEditorTarget target) {
    switch (target) {
        case TimeEditorTarget::FOCUS_PRESET:
        case TimeEditorTarget::BREAK_PRESET:
            return AppPage::FOCUS_SETTINGS;
        case TimeEditorTarget::TIMER_PRESET:
            return AppPage::TIMER_SETTINGS;
        case TimeEditorTarget::NONE:
        default:
            return AppPage::FOCUS_SETTINGS;
    }
}

void PageManager::openTimeEditor(TimeEditorTarget target) {
    if (target == TimeEditorTarget::NONE) {
        return;
    }

    timeEditor_.target = target;
    timeEditor_.returnPage = settingsPageForTimeEditorTarget(target);
    timeEditor_.draftSeconds = secondsForTimeEditorTarget(target);
    timeEditor_.selectedField = (timeEditor_.draftSeconds >= 3600U) ? 0 : 1;
    page_ = AppPage::TIME_EDITOR;
}

void PageManager::openTimeEditorForOption(uint8_t option) {
    if (!isSettingsPage(page_)) {
        return;
    }

    const TimeEditorTarget target = timeEditorTargetForOption(option);
    if (target == TimeEditorTarget::NONE) {
        return;
    }

    settings_.selectedOption = option;
    openTimeEditor(target);
}

void PageManager::cancelTimeEditor() {
    if (page_ != AppPage::TIME_EDITOR) {
        return;
    }

    const AppPage returnPage = timeEditor_.returnPage;
    timeEditor_.target = TimeEditorTarget::NONE;
    timeEditor_.draftSeconds = 0;
    timeEditor_.selectedField = 1;
    page_ = returnPage;
}

void PageManager::saveTimeEditor() {
    if (page_ != AppPage::TIME_EDITOR) {
        return;
    }

    bool changed = false;
    switch (timeEditor_.target) {
        case TimeEditorTarget::FOCUS_PRESET:
            if (settings_.focusPresetSeconds != timeEditor_.draftSeconds) {
                settings_.focusPresetSeconds = timeEditor_.draftSeconds;
                changed = true;
            }
            break;
        case TimeEditorTarget::BREAK_PRESET:
            if (settings_.focusBreakSeconds != timeEditor_.draftSeconds) {
                settings_.focusBreakSeconds = timeEditor_.draftSeconds;
                changed = true;
            }
            break;
        case TimeEditorTarget::TIMER_PRESET:
            if (settings_.timerPresetSeconds != timeEditor_.draftSeconds) {
                settings_.timerPresetSeconds = timeEditor_.draftSeconds;
                changed = true;
            }
            break;
        case TimeEditorTarget::NONE:
        default:
            break;
    }

    if (changed) {
        persistSettings();
    }
    cancelTimeEditor();
}

void PageManager::setTimeEditorField(uint8_t field) {
    if (page_ != AppPage::TIME_EDITOR || field > 2) {
        return;
    }
    timeEditor_.selectedField = field;
}

void PageManager::adjustTimeEditorField(int8_t delta) {
    if (page_ != AppPage::TIME_EDITOR || delta == 0) {
        return;
    }

    uint8_t hours = static_cast<uint8_t>(timeEditor_.draftSeconds / 3600U);
    uint8_t minutes = static_cast<uint8_t>((timeEditor_.draftSeconds / 60U) % 60U);
    uint8_t seconds = static_cast<uint8_t>(timeEditor_.draftSeconds % 60U);

    switch (timeEditor_.selectedField) {
        case 0:
            if (delta > 0) {
                if (hours < 23U) {
                    ++hours;
                }
            } else if (hours > 0U) {
                --hours;
            }
            break;
        case 1:
            if (delta > 0) {
                if (minutes < 59U) {
                    ++minutes;
                }
            } else if (minutes > 0U) {
                --minutes;
            }
            break;
        case 2:
            if (delta > 0) {
                if (seconds < 59U) {
                    ++seconds;
                }
            } else if (seconds > 0U) {
                --seconds;
            }
            break;
        default:
            break;
    }

    timeEditor_.draftSeconds = static_cast<uint32_t>(hours) * 3600U +
                               static_cast<uint32_t>(minutes) * 60U +
                               static_cast<uint32_t>(seconds);
}

uint8_t PageManager::focusSettingsOptionCount() const {
    return static_cast<uint8_t>(FocusSettingsOption::COUNT);
}

uint8_t PageManager::timerSettingsOptionCount() const {
    return static_cast<uint8_t>(TimerSettingsOption::COUNT);
}

uint8_t PageManager::settingsOptionCountForPage() const {
    if (page_ == AppPage::FOCUS_SETTINGS) {
        return focusSettingsOptionCount();
    }
    if (page_ == AppPage::TIMER_SETTINGS) {
        return timerSettingsOptionCount();
    }
    return 0;
}

bool PageManager::isFocusStartOption() const {
    return page_ == AppPage::FOCUS_SETTINGS &&
           settings_.selectedOption == static_cast<uint8_t>(FocusSettingsOption::START);
}

bool PageManager::isTimerStartOption() const {
    return page_ == AppPage::TIMER_SETTINGS &&
           settings_.selectedOption == static_cast<uint8_t>(TimerSettingsOption::START);
}

void PageManager::tapSettingsAction() {
    if (!isInSettings()) {
        return;
    }
    if (page_ == AppPage::FOCUS_SETTINGS && isFocusStartOption()) {
        startFocus(settings_.focusPresetSeconds);
        exitSettings();
        return;
    }
    if (page_ == AppPage::TIMER_SETTINGS && isTimerStartOption()) {
        startTimer(settings_.timerPresetSeconds);
        exitSettings();
        return;
    }
    nextSettingsOption();
}

uint32_t PageManager::clampPresetSeconds(uint32_t value, uint32_t min, uint32_t max, uint32_t step) {
    if (step == 0) {
        step = 1;
    }

    uint32_t clampedMin = min;
    uint32_t clampedMax = max;
    if (clampedMin > clampedMax) {
        const uint32_t tmp = clampedMin;
        clampedMin = clampedMax;
        clampedMax = tmp;
    }

    if (value < clampedMin) {
        return clampedMin;
    }
    if (value > clampedMax) {
        return clampedMax;
    }

    const uint32_t delta = value - clampedMin;
    return ((delta / step) * step + clampedMin);
}

uint32_t PageManager::nextPresetStep(uint32_t value, uint32_t step, uint32_t min, uint32_t max, int8_t delta) {
    if (delta > 0) {
        uint32_t next = value;
        if (next + step <= max) {
            next += step;
        }
        return clampPresetSeconds(next, min, max, step);
    }
    if (delta < 0) {
        if (value <= min + step) {
            return min;
        }
        return clampPresetSeconds(value - step, min, max, step);
    }
    return clampPresetSeconds(value, min, max, step);
}

void PageManager::adjustCurrentSetting(int8_t delta) {
    if (!isInSettings() || delta == 0) {
        return;
    }

    bool changed = false;
    if (page_ == AppPage::FOCUS_SETTINGS) {
        switch (static_cast<FocusSettingsOption>(settings_.selectedOption)) {
            case FocusSettingsOption::FOCUS_PRESET:
                {
                    const uint32_t next = nextPresetStep(
                        settings_.focusPresetSeconds, 300, 300, 1800, delta);
                    if (next != settings_.focusPresetSeconds) {
                        settings_.focusPresetSeconds = next;
                        changed = true;
                    }
                }
                break;
            case FocusSettingsOption::BREAK_PRESET:
                {
                    const uint32_t next = nextPresetStep(
                        settings_.focusBreakSeconds, 60, 60, 1200, delta);
                    if (next != settings_.focusBreakSeconds) {
                        settings_.focusBreakSeconds = next;
                        changed = true;
                    }
                }
                break;
            default:
                break;
        }
        if (changed) {
            persistSettings();
        }
        return;
    }

    if (page_ == AppPage::TIMER_SETTINGS) {
        switch (static_cast<TimerSettingsOption>(settings_.selectedOption)) {
            case TimerSettingsOption::TIMER_PRESET:
                {
                    const uint32_t next = nextPresetStep(
                        settings_.timerPresetSeconds, 60, 60, 3600, delta);
                    if (next != settings_.timerPresetSeconds) {
                        settings_.timerPresetSeconds = next;
                        changed = true;
                    }
                }
                break;
            case TimerSettingsOption::START:
                break;
            default:
                break;
        }
        if (changed) {
            persistSettings();
        }
    }
}

void PageManager::loadPersistedSettings() {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, true)) {
        return;
    }

    settings_.focusPresetSeconds = clampStoredSeconds(
        prefs.getUInt(kFocusSecondsKey, settings_.focusPresetSeconds),
        app::FOCUS_DEFAULT_SECONDS);
    settings_.focusBreakSeconds = clampStoredSeconds(
        prefs.getUInt(kBreakSecondsKey, settings_.focusBreakSeconds),
        app::FOCUS_BREAK_DEFAULT_SECONDS);
    settings_.timerPresetSeconds = clampStoredSeconds(
        prefs.getUInt(kTimerSecondsKey, settings_.timerPresetSeconds),
        app::TIMER_DEFAULT_SECONDS);
    prefs.end();
#endif
}

void PageManager::persistSettings() const {
#if defined(ARDUINO_ARCH_ESP32)
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) {
        return;
    }

    prefs.putUInt(kFocusSecondsKey, settings_.focusPresetSeconds);
    prefs.putUInt(kBreakSecondsKey, settings_.focusBreakSeconds);
    prefs.putUInt(kTimerSecondsKey, settings_.timerPresetSeconds);
    prefs.end();
#endif
}

const SettingsState& PageManager::settingsState() const {
    return settings_;
}

const TimeEditorState& PageManager::timeEditorState() const {
    return timeEditor_;
}

void PageManager::startFocus(uint32_t seconds) {
    const uint32_t focusSeconds = (seconds == 0 ? settings_.focusPresetSeconds : seconds);
    focus_.totalMs = focusSeconds * 1000;
    focus_.remainingMs = focus_.totalMs;
    focus_.running = focus_.totalMs > 0;

    break_.totalMs = settings_.focusBreakSeconds * 1000;
    break_.remainingMs = break_.totalMs;
    break_.running = false;

    if (focus_.running) {
        focusPhase_ = FocusPhase::FOCUS;
        return;
    }

    if (break_.totalMs > 0) {
        break_.running = true;
        focusPhase_ = FocusPhase::BREAK;
    } else {
        focusPhase_ = FocusPhase::COMPLETE;
    }
}

void PageManager::pauseResumeFocus() {
    if (focusPhase_ == FocusPhase::FOCUS && focus_.remainingMs > 0) {
        focus_.running = !focus_.running;
        return;
    }
    if (focusPhase_ == FocusPhase::BREAK && break_.remainingMs > 0) {
        break_.running = !break_.running;
    }
}

void PageManager::resetFocus() {
    focus_.totalMs = 0;
    focus_.remainingMs = 0;
    focus_.running = false;

    break_.totalMs = 0;
    break_.remainingMs = 0;
    break_.running = false;
    focusPhase_ = FocusPhase::IDLE;
}

void PageManager::startTimer(uint32_t seconds) {
    timer_.totalMs = seconds * 1000;
    timer_.remainingMs = timer_.totalMs;
    timer_.running = true;
}

void PageManager::pauseResumeTimer() {
    timer_.running = !timer_.running;
}

void PageManager::resetTimer() {
    timer_.totalMs = 0;
    timer_.remainingMs = 0;
    timer_.running = false;
}

const char* PageManager::statusText() const {
    if (kStatusCount == 0) {
        return "";
    }
    if (statusIndex_ >= kStatusCount) {
        return kStatusLabels[0];
    }
    return kStatusLabels[statusIndex_];
}

}  // namespace ui
