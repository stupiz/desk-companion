#include "air_quality_model.h"

namespace air_quality {

const char* severityLabel(SeverityLevel level) {
    switch (level) {
        case SeverityLevel::GREAT:
            return "GREAT";
        case SeverityLevel::NORMAL:
            return "NORMAL";
        case SeverityLevel::STUFFY:
            return "STUFFY";
        case SeverityLevel::BAD:
            return "BAD";
    }
    return "UNKNOWN";
}

uint8_t severityToInt(SeverityLevel level) {
    return static_cast<uint8_t>(level);
}

static SeverityLevel maxSeverity(SeverityLevel a, SeverityLevel b) {
    return (severityToInt(a) >= severityToInt(b)) ? a : b;
}

SeverityLevel severityFromCo2(uint16_t co2ppm) {
    if (co2ppm >= 1401) {
        return SeverityLevel::BAD;
    }
    if (co2ppm >= 1001) {
        return SeverityLevel::STUFFY;
    }
    if (co2ppm >= 801) {
        return SeverityLevel::NORMAL;
    }
    return SeverityLevel::GREAT;
}

SeverityLevel severityFromVoc(uint16_t vocIndex) {
    if (vocIndex >= 250) {
        return SeverityLevel::BAD;
    }
    if (vocIndex >= 150) {
        return SeverityLevel::STUFFY;
    }
    if (vocIndex >= 100) {
        return SeverityLevel::NORMAL;
    }
    return SeverityLevel::GREAT;
}

SeverityLevel severityFromTemperature(float temperatureC) {
    if (!isfinite(temperatureC)) {
        return SeverityLevel::NORMAL;
    }
    if (temperatureC < 18.0f || temperatureC >= 30.0f) {
        return SeverityLevel::BAD;
    }
    if ((temperatureC >= 28.0f && temperatureC <= 29.0f) ||
        (temperatureC >= 18.0f && temperatureC <= 19.0f)) {
        return SeverityLevel::STUFFY;
    }
    if ((temperatureC >= 20.0f && temperatureC <= 21.0f) ||
        (temperatureC >= 26.0f && temperatureC <= 27.0f)) {
        return SeverityLevel::NORMAL;
    }
    return SeverityLevel::GREAT;
}

SeverityLevel severityFromHumidity(float humidityPct) {
    if (!isfinite(humidityPct)) {
        return SeverityLevel::NORMAL;
    }
    if (humidityPct < 20.0f || humidityPct > 70.0f) {
        return SeverityLevel::BAD;
    }
    if ((humidityPct >= 61.0f && humidityPct <= 70.0f) ||
        (humidityPct >= 20.0f && humidityPct <= 29.0f)) {
        return SeverityLevel::STUFFY;
    }
    if ((humidityPct >= 30.0f && humidityPct <= 39.0f) ||
        (humidityPct >= 51.0f && humidityPct <= 60.0f)) {
        return SeverityLevel::NORMAL;
    }
    return SeverityLevel::GREAT;
}

AirQualityScore evaluateAirQuality(const AirSample& sample) {
    AirQualityScore score;
    if (!sample.valid) {
        score.valid = false;
        return score;
    }

    score.co2Severity = severityFromCo2(sample.co2ppm);
    score.vocSeverity = severityFromVoc(sample.vocIndex);
    score.tempSeverity = severityFromTemperature(sample.temperatureC);
    score.humiditySeverity = severityFromHumidity(sample.humidityPct);

    SeverityLevel baseSeverity = maxSeverity(score.co2Severity, score.vocSeverity);
    SeverityLevel comfortSeverity = maxSeverity(score.tempSeverity, score.humiditySeverity);

    // Use CO2 + VOC as base, then apply comfort adjustment up to +1/-0.
    uint8_t adjust = 0;
    if (comfortSeverity >= SeverityLevel::STUFFY) {
        adjust = (comfortSeverity == SeverityLevel::BAD) ? 2 : 1;
    }

    uint8_t finalSeverity = severityToInt(baseSeverity) + adjust;
    if (finalSeverity < 1) {
        finalSeverity = 1;
    } else if (finalSeverity > 4) {
        finalSeverity = 4;
    }

    score.overall = static_cast<SeverityLevel>(finalSeverity);
    score.valid = true;
    return score;
}

}  // namespace air_quality
