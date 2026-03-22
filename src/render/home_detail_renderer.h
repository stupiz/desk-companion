#pragma once

#include <Arduino.h>
#include "air_quality_model.h"

class HomeDetailRenderer {
public:
    void render(const air_quality::AirSample& sample,
                const air_quality::AirQualityScore& score,
                bool scdOnline,
                bool sgpOnline);
};
