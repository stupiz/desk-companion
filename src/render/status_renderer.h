#pragma once

#include <Arduino.h>
#include "air_quality_model.h"

class StatusRenderer {
public:
    void render(const char* statusText, uint8_t effectMode, unsigned long nowMs,
        air_quality::SeverityLevel airSeverity);
};
