#ifndef INCUBATOR_STATE_H
#define INCUBATOR_STATE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "incubator_events.h"
#include "incubator_config.h"

/* Mutex-protected snapshot of "what the incubator is currently doing" -
   the single source of truth every task reads/writes through instead of
   touching plain globals, so a read from one task can never race a write
   from another. ControlTask and MotorTask are the only writers (each owns
   a disjoint set of peripherals); everyone else only reads. */
class IncubatorState {
public:
    esp_err_t init();

    IncubatorMode getMode();
    void setMode(IncubatorMode mode);

    void setSensor(float temperature, float humidity, bool valid);
    void getSensor(float& rOutTemperature, float& rOutHumidity, bool& rOutValid);

    /// @return true if this call actually changed the stored value -
    ///         callers use this to only publish MQTT/apply hardware
    ///         writes on real transitions, not on every re-evaluation of
    ///         the same decision.
    bool setActuator(PeripheralId peripheral, bool on);
    bool getActuator(PeripheralId peripheral);

    float getTempLow();
    void setTempLow(float value);
    float getTempHigh();
    void setTempHigh(float value);

private:
    SemaphoreHandle_t mutex_ = nullptr;

    IncubatorMode mode_ = IncubatorMode::Auto;
    float temperature_ = 0.0f;
    float humidity_ = 0.0f;
    bool sensorValid_ = false;
    bool actuatorOn_[static_cast<size_t>(PeripheralId::Count)] = {};
    float tempLow_ = TEMP_LOW_C;
    float tempHigh_ = TEMP_HIGH_C;
};

#endif /* INCUBATOR_STATE_H */
