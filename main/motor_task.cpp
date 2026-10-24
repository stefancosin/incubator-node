#include "motor_task.h"
#include "incubator_config.h"
#include "freertos/task.h"

void MotorTask::publishMotorState(bool on) {
    if (pState_->setActuator(PeripheralId::Motor, on)) {
        pMqtt_->publishPeripheralState(PERIPHERAL_NAME_MOTOR, on);
    }
}

/* Polls motorQueue_ between steps (non-blocking) so a ManualOff arriving
   mid-turn interrupts it immediately instead of waiting out the remaining
   steps - manual control always wins over an in-progress AUTO turn. */
void MotorTask::runAutoTurn180() {

    publishMotorState(true);

    for (int i = 0; i < STEPPER_STEPS_180_DEG; i++) {
        motor_.step(true);
        vTaskDelay(pdMS_TO_TICKS(STEPPER_STEP_DELAY_MS));

        MotorCmd cmd;
        if (xQueueReceive(motorQueue_, &cmd, 0) == pdTRUE) {
            if (cmd.type == MotorCmdType::ManualOff) {
                break;
            }
            /* ManualOn or another AutoTurn180 while a turn is already in
               progress - nothing to do, keep stepping */
        }
    }

    motor_.stop();
    publishMotorState(false);
}

/* Runs continuously until ManualOff arrives - lets a MANUAL-mode operator
   spin the conveyor freely rather than being limited to a single
   180-degree pulse. */
void MotorTask::runManual() {

    publishMotorState(true);

    MotorCmd cmd;
    bool running = true;
    while (running) {
        motor_.step(true);
        if (xQueueReceive(motorQueue_, &cmd, pdMS_TO_TICKS(STEPPER_STEP_DELAY_MS)) == pdTRUE) {
            if (cmd.type == MotorCmdType::ManualOff) {
                running = false;
            }
        }
    }

    motor_.stop();
    publishMotorState(false);
}

void MotorTask::taskEntry(void *pvParameters) {
    static_cast<MotorTask *>(pvParameters)->run();
}

void MotorTask::run() {

    motor_.init(STEPPER_IN1_GPIO, STEPPER_IN2_GPIO, STEPPER_IN3_GPIO, STEPPER_IN4_GPIO);

    /* Same reasoning as ControlTask's startup publish - state is only
       published on a real transition, so publish the known-off startup
       state unconditionally rather than leaving a stale retained value */
    pMqtt_->publishPeripheralState(PERIPHERAL_NAME_MOTOR, false);

    MotorCmd cmd;
    for (;;) {
        if (xQueueReceive(motorQueue_, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {
                case MotorCmdType::AutoTurn180:
                    runAutoTurn180();
                    break;
                case MotorCmdType::ManualOn:
                    runManual();
                    break;
                case MotorCmdType::ManualOff:
                    /* Already idle - stray OFF with nothing running, ignore */
                    break;
            }
        }
    }
}

void MotorTask::start(QueueHandle_t motorQueue, IncubatorState& rState, MqttLink& rMqtt) {
    motorQueue_ = motorQueue;
    pState_ = &rState;
    pMqtt_ = &rMqtt;
    xTaskCreate(&MotorTask::taskEntry, "motor_task", 4096, this, 5, nullptr);
}
