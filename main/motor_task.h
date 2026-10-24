#ifndef MOTOR_TASK_H
#define MOTOR_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "stepper_28byj48.h"
#include "incubator_state.h"
#include "mqtt_link.h"

/// Exclusively owns the stepper motor. Runs commands received on its
/// queue (see MotorCmd) - AUTO 180-degree turns and MANUAL on/off.
/// Stepping happens on its own task instead of being polled from a shared
/// loop() like the original firmware, so a 2048-step turn never blocks
/// sensor reads or MQTT handling.
class MotorTask {
public:
    void start(QueueHandle_t motorQueue, IncubatorState& rState, MqttLink& rMqtt);

private:
    QueueHandle_t motorQueue_ = nullptr;
    IncubatorState *pState_ = nullptr;
    MqttLink *pMqtt_ = nullptr;
    Stepper28BYJ48 motor_;

    static void taskEntry(void *pvParameters);
    void run();

    void publishMotorState(bool on);
    void runAutoTurn180();
    void runManual();
};

#endif /* MOTOR_TASK_H */
