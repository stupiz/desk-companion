#pragma once

#include <stdint.h>
#include <math.h>

namespace air_quality {

enum class SeverityLevel : uint8_t {
    GREAT = 1,
    NORMAL = 2,
    STUFFY = 3,
    BAD = 4,
};

struct AirSample {
    uint16_t co2ppm;
    float temperatureC;
    float humidityPct;
    uint16_t vocIndex;
    bool valid;
    uint32_t timestampMs;

    AirSample()
        : co2ppm(0), temperatureC(NAN), humidityPct(NAN), vocIndex(0), valid(false), timestampMs(0) {}
};

struct AirQualityScore {
    SeverityLevel co2Severity = SeverityLevel::NORMAL;
    SeverityLevel vocSeverity = SeverityLevel::NORMAL;
    SeverityLevel tempSeverity = SeverityLevel::NORMAL;
    SeverityLevel humiditySeverity = SeverityLevel::NORMAL;
    SeverityLevel overall = SeverityLevel::NORMAL;
    bool valid = false;
};

const char* severityLabel(SeverityLevel level);
uint8_t severityToInt(SeverityLevel level);

SeverityLevel severityFromCo2(uint16_t co2ppm);
SeverityLevel severityFromVoc(uint16_t vocIndex);
SeverityLevel severityFromTemperature(float temperatureC);
SeverityLevel severityFromHumidity(float humidityPct);

AirQualityScore evaluateAirQuality(const AirSample& sample);

}  // namespace air_quality
