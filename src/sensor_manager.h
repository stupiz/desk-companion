#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config/app_config.h"
#include "air_quality_model.h"

#include <SensirionI2cScd4x.h>
#include <Adafruit_SGP40.h>

class SensorManager {
public:
    bool begin();
    bool rescan();
    void update(unsigned long nowMs);
    const air_quality::AirSample& latestSample() const;
    const air_quality::AirQualityScore& latestScore() const;
    bool isReady() const;
    bool scdReady() const;
    bool sgpReady() const;
    bool sgpWarmingUp() const;

private:
    void resetState();
    void pollScd41();
    void pollSgp40();
    void refreshScore();
    void logSample();
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    bool initScd41OnBus(TwoWire& bus, const char* label);
#endif

private:
    air_quality::AirSample lastSample_;
    air_quality::AirQualityScore lastScore_;

    unsigned long lastScdPollMs_ = 0;
    unsigned long lastSgpPollMs_ = 0;
    unsigned long lastLogMs_ = 0;
    uint16_t sgpSampleCount_ = 0;
    bool ready_ = false;
    bool scdReady_ = false;
    bool sgpReady_ = false;
    TwoWire* sensorBus_ = &Wire;
    const char* activeBusLabel_ = "Wire";

    SensirionI2cScd4x scd4x_;
    Adafruit_SGP40 sgp40_;
};
