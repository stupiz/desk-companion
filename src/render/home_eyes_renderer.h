#pragma once

#include <Arduino.h>
#include "mood_engine.h"
#include "air_quality_model.h"

enum class HomeEyesStyle : uint8_t {
    RobotVisor = 0,
    PixelRetro = 1,
};

class HomeEyesRenderer {
public:
    void render(
        const mood::MoodState& mood,
        bool eyesOpen,
        const air_quality::AirSample& sample,
        const air_quality::AirQualityScore& score,
        HomeEyesStyle style);
};
