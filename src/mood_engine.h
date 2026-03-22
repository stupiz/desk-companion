#pragma once

#include <stdint.h>
#include "air_quality_model.h"

namespace mood {

enum class EyeMood : uint8_t {
    GREAT,
    NORMAL,
    STUFFY,
    BAD,
};

struct MoodState {
    air_quality::SeverityLevel severity = air_quality::SeverityLevel::NORMAL;
    EyeMood eyeMood = EyeMood::NORMAL;
    const char* eyesOpen = "O O";
    const char* eyesClosed = "- -";
    const char* mouth = "_";
    const char* label = "NORMAL";
    uint16_t displayColor565 = 0x07E0;
    unsigned long blinkMs = 900;
};

MoodState resolveMood(air_quality::SeverityLevel severity);
const char* eyeMoodName(EyeMood mood);
const char* severityToText(air_quality::SeverityLevel severity);

}  // namespace mood
