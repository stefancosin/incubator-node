#include "stepper_28byj48.h"

void Stepper28BYJ48::writeCoils(int a, int b, int c, int d) {
    gpio_set_level(in1_, a);
    gpio_set_level(in2_, b);
    gpio_set_level(in3_, c);
    gpio_set_level(in4_, d);
}

void Stepper28BYJ48::init(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4) {

    in1_ = in1;
    in2_ = in2;
    in3_ = in3;
    in4_ = in4;
    stepIndex_ = 0;

    const gpio_num_t pins[4] = {in1_, in2_, in3_, in4_};
    for (gpio_num_t pin : pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    }

    stop();
}

void Stepper28BYJ48::step(bool forward) {

    switch (stepIndex_) {
        case 0: writeCoils(1, 0, 0, 0); break;
        case 1: writeCoils(1, 1, 0, 0); break;
        case 2: writeCoils(0, 1, 0, 0); break;
        case 3: writeCoils(0, 1, 1, 0); break;
        case 4: writeCoils(0, 0, 1, 0); break;
        case 5: writeCoils(0, 0, 1, 1); break;
        case 6: writeCoils(0, 0, 0, 1); break;
        case 7: writeCoils(1, 0, 0, 1); break;
        default: writeCoils(0, 0, 0, 0); break;
    }

    stepIndex_ += forward ? 1 : -1;
    if (stepIndex_ > 7) {
        stepIndex_ = 0;
    } else if (stepIndex_ < 0) {
        stepIndex_ = 7;
    }
}

void Stepper28BYJ48::stop() {
    writeCoils(0, 0, 0, 0);
}
