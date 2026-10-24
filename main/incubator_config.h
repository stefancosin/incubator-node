#ifndef INCUBATOR_CONFIG_H
#define INCUBATOR_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c.h"

/* ---- Sensor ---- */
#define DHT11_GPIO                GPIO_NUM_4

/* ---- LCD (I2C, PCF8574 backpack) ---- */
#define LCD_I2C_PORT               I2C_NUM_0
#define LCD_I2C_ADDR               0x27
#define LCD_I2C_SDA_GPIO           GPIO_NUM_21
#define LCD_I2C_SCL_GPIO           GPIO_NUM_22

/* ---- Actuators - adjust to match actual wiring ---- */
#define HEATER_RELAY_GPIO          GPIO_NUM_19
#define COOLER_GPIO                GPIO_NUM_23
#define ALARM_GPIO                 GPIO_NUM_18

/* ---- Stepper (28BYJ-48 via ULN2003) ---- */
#define STEPPER_IN1_GPIO           GPIO_NUM_25
#define STEPPER_IN2_GPIO           GPIO_NUM_26
#define STEPPER_IN3_GPIO           GPIO_NUM_27
#define STEPPER_IN4_GPIO           GPIO_NUM_14
#define STEPPER_STEPS_180_DEG      2048
#define STEPPER_STEP_DELAY_MS      1

/* ---- AUTO mode thresholds - default values only, runtime-configurable
   via incubator/peripherals/temp_low|temp_high/set (see IncubatorState) ---- */
#define TEMP_LOW_C                 25.0f
#define TEMP_HIGH_C                30.0f

/* Sanity bounds for values coming in over MQTT - rejects e.g. a typo'd
   "250" that would leave the heater or cooler stuck on permanently */
#define TEMP_THRESHOLD_MIN_C       0.0f
#define TEMP_THRESHOLD_MAX_C       50.0f

/* ---- Timing ---- */
#define SENSOR_READ_INTERVAL_MS    2000
#define EGG_TURN_INTERVAL_MS       10000

/* DHT11 units occasionally miss a read even when wired correctly (a known
   quirk of the sensor) - require this many consecutive failures before
   showing "Sensor Error!" on the LCD, so one stray miss doesn't flash a
   fault screen over a still-valid last reading */
#define SENSOR_FAULT_THRESHOLD     3

/* ---- MQTT topic scheme - fixed by the RPi platform, do not change
   without updating rpi-platform/src/MqttClient.cpp to match ---- */
#define MQTT_TOPIC_SENSOR_PREFIX           "incubator/sensors/"
#define MQTT_TOPIC_PERIPHERAL_PREFIX       "incubator/peripherals/"
#define MQTT_TOPIC_PERIPHERAL_SET_SUFFIX   "/set"
#define MQTT_TOPIC_PERIPHERAL_STATE_SUFFIX "/state"
#define MQTT_TOPIC_PERIPHERAL_SET_FILTER   "incubator/peripherals/+/set"

/* "mode" is not a real peripheral - it rides the same
   incubator/peripherals/<name>/set|state topics (payload "auto"/"manual")
   so the RPi side needs zero changes to support the AUTO/MANUAL switch */
#define PERIPHERAL_NAME_HEATER     "heater"
#define PERIPHERAL_NAME_COOLER     "cooler"
#define PERIPHERAL_NAME_ALARM      "alarm"
#define PERIPHERAL_NAME_MOTOR      "motor"
#define PERIPHERAL_NAME_MODE       "mode"
#define PERIPHERAL_NAME_TEMP_LOW   "temp_low"
#define PERIPHERAL_NAME_TEMP_HIGH  "temp_high"

#define SENSOR_NAME_TEMPERATURE    "temperature"
#define SENSOR_NAME_HUMIDITY       "humidity"

#endif /* INCUBATOR_CONFIG_H */
