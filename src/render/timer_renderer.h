#pragma once

#include <Arduino.h>
#include "page_manager.h"

class TimerRenderer {
public:
    void render(const ui::CountdownState& state);
};
