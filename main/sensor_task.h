#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "dht11.h"
#include "mqtt_link.h"

/// Reads the DHT11 every SENSOR_READ_INTERVAL_MS, publishes the raw
/// readings to MQTT, and forwards a SensorReading ControlEvent to
/// ControlTask's queue for it to act on (AUTO logic, LCD update).
class SensorTask {
public:
    void start(QueueHandle_t controlQueue, MqttLink& rMqtt);

private:
    QueueHandle_t controlQueue_ = nullptr;
    MqttLink *pMqtt_ = nullptr;
    Dht11 dht11_;

    static void taskEntry(void *pvParameters);
    void run();
};

#endif /* SENSOR_TASK_H */
