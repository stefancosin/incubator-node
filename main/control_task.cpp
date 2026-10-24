#include "control_task.h"
#include "incubator_config.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "control_task";

const char *ControlTask::peripheralName(PeripheralId id) {
    switch (id) {
        case PeripheralId::Heater: return PERIPHERAL_NAME_HEATER;
        case PeripheralId::Cooler: return PERIPHERAL_NAME_COOLER;
        case PeripheralId::Alarm:  return PERIPHERAL_NAME_ALARM;
        case PeripheralId::Motor:  return PERIPHERAL_NAME_MOTOR;
        default:                   return "unknown";
    }
}

GpioActuator *ControlTask::actuatorFor(PeripheralId id) {
    switch (id) {
        case PeripheralId::Heater: return &heater_;
        case PeripheralId::Cooler: return &cooler_;
        case PeripheralId::Alarm:  return &alarm_;
        default:                   return nullptr; /* motor is owned by MotorTask */
    }
}

void ControlTask::applyActuator(PeripheralId id, bool on) {

    GpioActuator *pActuator = actuatorFor(id);
    if (pActuator == nullptr) {
        return;
    }

    if (pState_->setActuator(id, on)) {
        pActuator->set(on);
        pMqtt_->publishPeripheralState(peripheralName(id), on);
        refreshLcd();
    }
}

/* A relay click (especially switching a mains-voltage load) can couple
   enough EMI into the I2C lines to corrupt not just the next byte or two
   but the HD44780 controller's own internal state (cursor position,
   4-bit/8-bit mode) - a plain reprint doesn't recover from that, so this
   redoes the full init sequence and then redraws whatever was last known
   to be true. Proper fix is physically routing the switched load's wiring
   away from SDA/SCL; this is the software-side mitigation until then. */
void ControlTask::refreshLcd() {

    vTaskDelay(pdMS_TO_TICKS(100)); /* let the switching transient settle first */
    lcd_.init(LCD_I2C_PORT, LCD_I2C_ADDR, LCD_I2C_SDA_GPIO, LCD_I2C_SCL_GPIO);

    float temperature = 0.0f;
    float humidity = 0.0f;
    bool valid = false;
    pState_->getSensor(temperature, humidity, valid);

    if (valid) {
        lcd_.writeTemperature(temperature);
        lcd_.writeHumidity(humidity);
    } else {
        lcd_.writeSensorFault();
    }
}

/* Same bang-bang bounds as the original firmware's loop():
   [LOW,HIGH] -> everything off; below LOW -> heater+alarm; above HIGH ->
   cooler+alarm. LOW/HIGH themselves are runtime-configurable (see
   handleTempLowSet/handleTempHighSet), not compile-time constants. */
void ControlTask::applyAutoControl(float temperature) {

    float low  = pState_->getTempLow();
    float high = pState_->getTempHigh();

    if ((temperature >= low) && (temperature <= high)) {
        applyActuator(PeripheralId::Heater, false);
        applyActuator(PeripheralId::Alarm, false);
        applyActuator(PeripheralId::Cooler, false);
    } else if (temperature < low) {
        applyActuator(PeripheralId::Heater, true);
        applyActuator(PeripheralId::Alarm, true);
        applyActuator(PeripheralId::Cooler, false);
    } else {
        applyActuator(PeripheralId::Heater, false);
        applyActuator(PeripheralId::Alarm, true);
        applyActuator(PeripheralId::Cooler, true);
    }
}

void ControlTask::handleSensorReading(float temperature, float humidity, bool valid) {

    if (!valid) {
        consecutiveSensorFailures_++;
        /* Below threshold: leave the LCD and AUTO control alone - the last
           good reading stays displayed/applied rather than flashing an
           error over a single stray miss */
        if (consecutiveSensorFailures_ >= SENSOR_FAULT_THRESHOLD) {
            pState_->setSensor(temperature, humidity, false);
            lcd_.writeSensorFault();
        }
        return;
    }

    consecutiveSensorFailures_ = 0;
    pState_->setSensor(temperature, humidity, true);

    lcd_.writeTemperature(temperature);
    lcd_.writeHumidity(humidity);

    if (pState_->getMode() == IncubatorMode::Auto) {
        applyAutoControl(temperature);
    }
}

void ControlTask::handleModeSet(IncubatorMode mode) {

    if (pState_->getMode() == mode) {
        return;
    }

    pState_->setMode(mode);
    pMqtt_->publishModeState(mode);
    ESP_LOGI(TAG, "Mode switched to %s", (mode == IncubatorMode::Auto) ? "auto" : "manual");

    /* Handing control to a human shouldn't leave heater/cooler/alarm stuck
       in whatever AUTO last decided - force them off so MANUAL starts from
       a known state. Re-entering AUTO doesn't need the mirror case: the
       next sensor reading re-evaluates applyAutoControl() from scratch.
       Motor isn't touched here - an in-progress turn is MotorTask's own
       concern, handled as MANUAL commands arrive. */
    if (mode == IncubatorMode::Manual) {
        applyActuator(PeripheralId::Heater, false);
        applyActuator(PeripheralId::Cooler, false);
        applyActuator(PeripheralId::Alarm, false);
    }
}

void ControlTask::handleActuatorSet(PeripheralId peripheral, bool on) {

    if (peripheral == PeripheralId::Motor) {
        MotorCmd cmd{on ? MotorCmdType::ManualOn : MotorCmdType::ManualOff};
        if (xQueueSend(motorQueue_, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "motor queue full, dropped manual command");
        }
        return;
    }

    if (pState_->getMode() != IncubatorMode::Manual) {
        ESP_LOGW(TAG, "Ignoring \"%s\" command - not in MANUAL mode", peripheralName(peripheral));
        return;
    }

    applyActuator(peripheral, on);
}

void ControlTask::handleTempLowSet(float value) {

    if (value >= pState_->getTempHigh()) {
        ESP_LOGW(TAG, "Rejecting temp_low=%.1f - not below current temp_high", value);
        return;
    }

    pState_->setTempLow(value);
    pMqtt_->publishPeripheralValue(PERIPHERAL_NAME_TEMP_LOW, value);
    ESP_LOGI(TAG, "temp_low set to %.1f", value);
}

void ControlTask::handleTempHighSet(float value) {

    if (value <= pState_->getTempLow()) {
        ESP_LOGW(TAG, "Rejecting temp_high=%.1f - not above current temp_low", value);
        return;
    }

    pState_->setTempHigh(value);
    pMqtt_->publishPeripheralValue(PERIPHERAL_NAME_TEMP_HIGH, value);
    ESP_LOGI(TAG, "temp_high set to %.1f", value);
}

/* Runs in the Timer Service task context (a real FreeRTOS task, not an
   ISR), so plain xQueueSend is correct here - no *FromISR variant needed */
void ControlTask::eggTurnTimerTrampoline(TimerHandle_t timer) {
    static_cast<ControlTask *>(pvTimerGetTimerID(timer))->onEggTurnTimer();
}

void ControlTask::onEggTurnTimer() {

    if (pState_->getMode() != IncubatorMode::Auto) {
        return;
    }

    MotorCmd cmd{MotorCmdType::AutoTurn180};
    xQueueSend(motorQueue_, &cmd, 0);
}

void ControlTask::taskEntry(void *pvParameters) {
    static_cast<ControlTask *>(pvParameters)->run();
}

void ControlTask::run() {

    ControlEvent evt;

    heater_.init(HEATER_RELAY_GPIO);
    cooler_.init(COOLER_GPIO);
    alarm_.init(ALARM_GPIO);

    /* GpioActuator::init() drives the pin low, and IncubatorState starts
       every actuator false/Auto to match - but MQTT state topics are only
       published on a real transition, so if an actuator never happens to
       flip this session, the RPi's retained value could still be
       whatever a much earlier session last published. Publish the known
       startup state unconditionally so the dashboard is accurate from
       the first second, not just after the first real change. */
    pMqtt_->publishPeripheralState(PERIPHERAL_NAME_HEATER, false);
    pMqtt_->publishPeripheralState(PERIPHERAL_NAME_COOLER, false);
    pMqtt_->publishPeripheralState(PERIPHERAL_NAME_ALARM, false);
    pMqtt_->publishModeState(IncubatorMode::Auto);
    pMqtt_->publishPeripheralValue(PERIPHERAL_NAME_TEMP_LOW, pState_->getTempLow());
    pMqtt_->publishPeripheralValue(PERIPHERAL_NAME_TEMP_HIGH, pState_->getTempHigh());

    lcd_.init(LCD_I2C_PORT, LCD_I2C_ADDR, LCD_I2C_SDA_GPIO, LCD_I2C_SCL_GPIO);
    lcd_.writeBootScreen();
    vTaskDelay(pdMS_TO_TICKS(5000));
    lcd_.clear();

    eggTurnTimer_ = xTimerCreate("egg_turn", pdMS_TO_TICKS(EGG_TURN_INTERVAL_MS),
                                  pdTRUE, this, &ControlTask::eggTurnTimerTrampoline);
    xTimerStart(eggTurnTimer_, 0);

    for (;;) {
        if (xQueueReceive(controlQueue_, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.type) {
                case ControlEventType::SensorReading:
                    handleSensorReading(evt.data.sensor.temperature, evt.data.sensor.humidity,
                                         evt.data.sensor.valid);
                    break;
                case ControlEventType::ModeSet:
                    handleModeSet(evt.data.mode.mode);
                    break;
                case ControlEventType::ActuatorSet:
                    handleActuatorSet(evt.data.actuator.peripheral, evt.data.actuator.on);
                    break;
                case ControlEventType::TempLowSet:
                    handleTempLowSet(evt.data.tempThreshold.value);
                    break;
                case ControlEventType::TempHighSet:
                    handleTempHighSet(evt.data.tempThreshold.value);
                    break;
            }
        }
    }
}

void ControlTask::start(QueueHandle_t controlQueue, QueueHandle_t motorQueue,
                         IncubatorState& rState, MqttLink& rMqtt) {
    controlQueue_ = controlQueue;
    motorQueue_ = motorQueue;
    pState_ = &rState;
    pMqtt_ = &rMqtt;
    xTaskCreate(&ControlTask::taskEntry, "control_task", 4096, this, 5, nullptr);
}
