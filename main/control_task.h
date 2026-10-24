#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "gpio_actuator.h"
#include "lcd_i2c.h"
#include "incubator_state.h"
#include "mqtt_link.h"

/// Owns the LCD, heater relay, cooler and alarm, and runs the AUTO/MANUAL
/// state machine. Consumes ControlEvent from its queue (sensor readings
/// from SensorTask, commands from MqttLink) one at a time, so hardware
/// writes are never interleaved from two tasks. Forwards motor commands
/// to the motor queue rather than touching the stepper itself - MotorTask
/// is the sole owner of that hardware.
class ControlTask {
public:
    void start(QueueHandle_t controlQueue, QueueHandle_t motorQueue,
               IncubatorState& rState, MqttLink& rMqtt);

private:
    QueueHandle_t controlQueue_ = nullptr;
    QueueHandle_t motorQueue_ = nullptr;
    IncubatorState *pState_ = nullptr;
    MqttLink *pMqtt_ = nullptr;

    GpioActuator heater_;
    GpioActuator cooler_;
    GpioActuator alarm_;
    LcdI2c lcd_;
    TimerHandle_t eggTurnTimer_ = nullptr;
    int consecutiveSensorFailures_ = 0;

    static void taskEntry(void *pvParameters);
    void run();

    static void eggTurnTimerTrampoline(TimerHandle_t timer);
    void onEggTurnTimer();

    GpioActuator *actuatorFor(PeripheralId id);
    static const char *peripheralName(PeripheralId id);

    void applyActuator(PeripheralId id, bool on);
    void refreshLcd();
    void applyAutoControl(float temperature);
    void handleSensorReading(float temperature, float humidity, bool valid);
    void handleModeSet(IncubatorMode mode);
    void handleActuatorSet(PeripheralId peripheral, bool on);
    void handleTempLowSet(float value);
    void handleTempHighSet(float value);
};

#endif /* CONTROL_TASK_H */
