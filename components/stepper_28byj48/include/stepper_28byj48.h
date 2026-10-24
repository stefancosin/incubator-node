#ifndef STEPPER_28BYJ48_H
#define STEPPER_28BYJ48_H

#include "driver/gpio.h"

/* 28BYJ-48 (via ULN2003) half-step driver. Caller owns timing between
   steps (see MotorTask) - this only knows how to energize one step. */
class Stepper28BYJ48 {
public:
    void init(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4);

    /// Energizes the coils for the next half-step and advances the
    /// internal step index.
    /// @param forward true to advance the sequence, false to reverse it.
    void step(bool forward);

    /// De-energizes all coils - stops holding torque and current draw
    /// between moves.
    void stop();

private:
    gpio_num_t in1_ = GPIO_NUM_NC;
    gpio_num_t in2_ = GPIO_NUM_NC;
    gpio_num_t in3_ = GPIO_NUM_NC;
    gpio_num_t in4_ = GPIO_NUM_NC;
    int stepIndex_ = 0;

    void writeCoils(int a, int b, int c, int d);
};

#endif /* STEPPER_28BYJ48_H */
