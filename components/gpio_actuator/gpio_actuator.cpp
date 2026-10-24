#include "gpio_actuator.h"

void GpioActuator::init(gpio_num_t pin) {
    pin_ = pin;
    gpio_reset_pin(pin_);
    gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_, 0);
}

void GpioActuator::set(bool on) {
    gpio_set_level(pin_, on ? 1 : 0);
}
