#include "sensor_manager.h"

namespace {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
constexpr uint8_t kScd41Address = SCD40_I2C_ADDR_62;

struct SensorBusCandidate {
    TwoWire* bus;
    uint8_t sda;
    uint8_t scl;
    const char* label;
};

bool probeI2cAddress(TwoWire& bus, uint8_t address) {
    bus.beginTransmission(address);
    return bus.endTransmission() == 0;
}

void printAddressHex(uint8_t address) {
    Serial.print("0x");
    if (address < 0x10) {
        Serial.print('0');
    }
    Serial.print(address, HEX);
}

void dumpI2cBusScan(TwoWire& bus, const char* label) {
    bool foundAny = false;

    Serial.print("[I2C] ");
    Serial.print(label);
    Serial.print(" devices:");

    for (uint8_t address = 0x08; address < 0x78; ++address) {
        if (!probeI2cAddress(bus, address)) {
            continue;
        }
        Serial.print(' ');
        printAddressHex(address);
        foundAny = true;
    }

    if (!foundAny) {
        Serial.print(" none");
    }
    Serial.println();
}
#endif
}

bool SensorManager::begin() {
    return rescan();
}

bool SensorManager::rescan() {
    resetState();

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
    const SensorBusCandidate candidates[] = {
        {&Wire, 15, 10, "Wire(15,10)"},
        {&Wire1, 43, 44, "Wire1(43,44)"},
    };

    for (const SensorBusCandidate& candidate : candidates) {
        candidate.bus->begin(candidate.sda, candidate.scl);
        candidate.bus->setClock(100000);
        delay(10);
        dumpI2cBusScan(*candidate.bus, candidate.label);
    }

    for (const SensorBusCandidate& candidate : candidates) {
        if (!probeI2cAddress(*candidate.bus, kScd41Address)) {
            Serial.print("No SCD41 ACK on ");
            Serial.println(candidate.label);
            continue;
        }

        if (initScd41OnBus(*candidate.bus, candidate.label)) {
            break;
        }
    }
#else
    sensorBus_->begin(app::I2C_SDA_PIN, app::I2C_SCL_PIN);
    scd4x_.begin(*sensorBus_, SCD40_I2C_ADDR_62);
    scdReady_ = scd4x_.startPeriodicMeasurement() == 0;
    if (scdReady_) {
        sgpReady_ = sgp40_.begin(sensorBus_);
    }
#endif

    if (scdReady_) {
    } else {
#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
        Serial.println("SCD41 init failed on all known QWIIC buses.");
#else
        Serial.println("SCD41 init failed.");
#endif
    }

    if (sgpReady_) {
        Serial.print("SGP40 detected on ");
        Serial.println(activeBusLabel_);
    } else if (scdReady_) {
        Serial.print("SGP40 not detected on ");
        Serial.print(activeBusLabel_);
        Serial.println(", continuing without VOC readings.");
    } else {
        Serial.println("SGP40 not detected, continuing without VOC readings.");
    }

    if (!scdReady_) {
        return false;
    }

    ready_ = true;
    lastSample_.valid = false;
    lastSample_.vocIndex = 0;
    lastScore_ = air_quality::evaluateAirQuality(lastSample_);
    return true;
}

void SensorManager::resetState() {
    ready_ = false;
    scdReady_ = false;
    sgpReady_ = false;
    lastReadMs_ = 0;
    sensorBus_ = &Wire;
    activeBusLabel_ = "Wire";
    lastSample_ = air_quality::AirSample{};
    lastSample_.vocIndex = 0;
    lastScore_ = air_quality::evaluateAirQuality(lastSample_);
}

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
bool SensorManager::initScd41OnBus(TwoWire& bus, const char* label) {
    scd4x_.begin(bus, kScd41Address);
    scd4x_.wakeUp();

    const int16_t stopError = scd4x_.stopPeriodicMeasurement();
    if (stopError != 0) {
        Serial.print("SCD41 stopPeriodicMeasurement on ");
        Serial.print(label);
        Serial.print(" returned ");
        Serial.println(stopError);
    }

    const int16_t reinitError = scd4x_.reinit();
    if (reinitError != 0) {
        Serial.print("SCD41 reinit on ");
        Serial.print(label);
        Serial.print(" returned ");
        Serial.println(reinitError);
    }

    uint64_t serialNumber = 0;
    const int16_t serialError = scd4x_.getSerialNumber(serialNumber);
    if (serialError != 0) {
        Serial.print("SCD41 getSerialNumber failed on ");
        Serial.print(label);
        Serial.print(" error=");
        Serial.println(serialError);
        return false;
    }

    const int16_t startError = scd4x_.startPeriodicMeasurement();
    if (startError != 0) {
        Serial.print("SCD41 startPeriodicMeasurement failed on ");
        Serial.print(label);
        Serial.print(" error=");
        Serial.println(startError);
        return false;
    }

    sensorBus_ = &bus;
    activeBusLabel_ = label;
    scdReady_ = true;
    sgpReady_ = sgp40_.begin(&bus);

    Serial.print("SCD41 detected on ");
    Serial.print(label);
    Serial.print(" serial=0x");
    Serial.print(static_cast<uint32_t>(serialNumber >> 32), HEX);
    Serial.println(static_cast<uint32_t>(serialNumber & 0xFFFFFFFF), HEX);
    return true;
}
#endif

void SensorManager::update(unsigned long nowMs) {
    if (!ready_) {
        return;
    }

    if (nowMs - lastReadMs_ < app::SENSOR_READ_INTERVAL_MS && lastReadMs_ != 0) {
        return;
    }
    readSensors();
    lastReadMs_ = nowMs;
}

void SensorManager::readSensors() {
    bool gotFreshData = false;
    bool scdReady = false;

    if (scd4x_.getDataReadyStatus(scdReady) == 0 && scdReady) {
        uint16_t co2 = 0;
        float tempC = 0.0f;
        float hum = 0.0f;

        if (scd4x_.readMeasurement(co2, tempC, hum) == 0) {
            lastSample_.co2ppm = co2;
            lastSample_.temperatureC = tempC;
            lastSample_.humidityPct = hum;
            gotFreshData = true;
        }
    }

    if (gotFreshData) {
        if (sgpReady_) {
            lastSample_.vocIndex = static_cast<uint16_t>(sgp40_.measureVocIndex(
                lastSample_.temperatureC, lastSample_.humidityPct));
        } else {
            lastSample_.vocIndex = 0;
        }
        lastSample_.valid = true;
        lastSample_.timestampMs = millis();
        lastScore_ = air_quality::evaluateAirQuality(lastSample_);
        Serial.print("[SENSOR] bus=");
        Serial.print(activeBusLabel_);
        Serial.print(" co2=");
        Serial.print(lastSample_.co2ppm);
        Serial.print(" temp=");
        Serial.print(lastSample_.temperatureC, 1);
        Serial.print("C hum=");
        Serial.print(lastSample_.humidityPct, 1);
        Serial.print("% voc=");
        Serial.println(lastSample_.vocIndex);
    }
}

const air_quality::AirSample& SensorManager::latestSample() const {
    return lastSample_;
}

const air_quality::AirQualityScore& SensorManager::latestScore() const {
    return lastScore_;
}

bool SensorManager::isReady() const {
    return ready_;
}

bool SensorManager::scdReady() const {
    return scdReady_;
}

bool SensorManager::sgpReady() const {
    return sgpReady_;
}
