#include "incubator_state.h"

esp_err_t IncubatorState::init() {

    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

IncubatorMode IncubatorState::getMode() {

    xSemaphoreTake(mutex_, portMAX_DELAY);
    IncubatorMode mode = mode_;
    xSemaphoreGive(mutex_);

    return mode;
}

void IncubatorState::setMode(IncubatorMode mode) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mode_ = mode;
    xSemaphoreGive(mutex_);
}

void IncubatorState::setSensor(float temperature, float humidity, bool valid) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    temperature_ = temperature;
    humidity_ = humidity;
    sensorValid_ = valid;
    xSemaphoreGive(mutex_);
}

void IncubatorState::getSensor(float& rOutTemperature, float& rOutHumidity, bool& rOutValid) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    rOutTemperature = temperature_;
    rOutHumidity = humidity_;
    rOutValid = sensorValid_;
    xSemaphoreGive(mutex_);
}

bool IncubatorState::setActuator(PeripheralId peripheral, bool on) {

    size_t index = static_cast<size_t>(peripheral);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool changed = (actuatorOn_[index] != on);
    actuatorOn_[index] = on;
    xSemaphoreGive(mutex_);

    return changed;
}

bool IncubatorState::getActuator(PeripheralId peripheral) {

    size_t index = static_cast<size_t>(peripheral);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool on = actuatorOn_[index];
    xSemaphoreGive(mutex_);

    return on;
}

float IncubatorState::getTempLow() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    float value = tempLow_;
    xSemaphoreGive(mutex_);
    return value;
}

void IncubatorState::setTempLow(float value) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    tempLow_ = value;
    xSemaphoreGive(mutex_);
}

float IncubatorState::getTempHigh() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    float value = tempHigh_;
    xSemaphoreGive(mutex_);
    return value;
}

void IncubatorState::setTempHigh(float value) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    tempHigh_ = value;
    xSemaphoreGive(mutex_);
}
