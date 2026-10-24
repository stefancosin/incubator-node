#ifndef GPIO_ACTUATOR_H
#define GPIO_ACTUATOR_H

#include "driver/gpio.h"

/* Generic on/off GPIO actuator - shared by every peripheral that is just a
   pin driven high/low (relay, cooler, alarm all were byte-for-byte
   identical in the original firmware, so one class replaces three). */
class GpioActuator {
public:
    void init(gpio_num_t pin);
    void set(bool on);

private:
    gpio_num_t pin_ = GPIO_NUM_NC;
};

#endif /* GPIO_ACTUATOR_H */
