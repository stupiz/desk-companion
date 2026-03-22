#pragma once

#include <Arduino.h>

#include "page_manager.h"

enum class TimeEditorHitType : uint8_t {
    None = 0,
    SelectField = 1,
    AdjustField = 2,
    Save = 3,
    Cancel = 4,
};

struct TimeEditorHit {
    TimeEditorHitType type = TimeEditorHitType::None;
    uint8_t field = 0;
    int8_t delta = 0;
};

class TimeEditorRenderer {
public:
    void render(const ui::TimeEditorState& state);
    TimeEditorHit hitTest(int16_t x, int16_t y, const ui::TimeEditorState& state) const;
};
