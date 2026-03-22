#pragma once

#include <Arduino.h>
#include "page_manager.h"

enum class SettingsHitType : uint8_t {
    None = 0,
    Start = 1,
    EditTime = 2,
};

struct SettingsHit {
    SettingsHitType type = SettingsHitType::None;
    uint8_t option = 0;
    int8_t delta = 0;
};

class SettingsRenderer {
public:
    void render(const ui::SettingsState& settings, ui::AppPage page);
    SettingsHit hitTest(int16_t x, int16_t y, ui::AppPage page) const;
};
