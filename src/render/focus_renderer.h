#pragma once

#include <Arduino.h>
#include "page_manager.h"

class FocusRenderer {
public:
    void render(const ui::CountdownState& focusState,
                const ui::CountdownState& breakState,
                bool running,
                bool breakMode,
                bool complete,
                bool active);
};
