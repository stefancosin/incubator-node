#ifndef INCUBATOR_EVENTS_H
#define INCUBATOR_EVENTS_H

#include <cstddef>

enum class IncubatorMode {
    Auto = 0,
    Manual,
};

enum class PeripheralId {
    Heater = 0,
    Cooler,
    Alarm,
    Motor,
    Count,
};

/* Everything that can change ControlTask's behavior arrives as one of
   these on its queue - sensor readings (from SensorTask) and commands
   (from MqttLink) are handled the same way: serialized through one inbox
   so ControlTask never has to reason about concurrent access */
enum class ControlEventType {
    SensorReading,
    ModeSet,
    ActuatorSet,
    TempLowSet,
    TempHighSet,
};

struct ControlEvent {
    ControlEventType type;
    union {
        struct {
            float temperature;
            float humidity;
            bool valid;
        } sensor;
        struct {
            IncubatorMode mode;
        } mode;
        struct {
            PeripheralId peripheral;
            bool on;
        } actuator;
        struct {
            float value;
        } tempThreshold;
    } data;
};

/* Commands sent to MotorTask, which owns the stepper motor exclusively */
enum class MotorCmdType {
    AutoTurn180,
    ManualOn,
    ManualOff,
};

struct MotorCmd {
    MotorCmdType type;
};

#endif /* INCUBATOR_EVENTS_H */
