#ifndef MQTT_LINK_H
#define MQTT_LINK_H

#include "esp_err.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "incubator_events.h"

class MqttLink {
public:
    /// Connects to the broker (see CONFIG_INCUBATOR_MQTT_BROKER_URI) and
    /// subscribes to incubator/peripherals/+/set. Every recognized
    /// incoming command is translated into a ControlEvent and pushed onto
    /// controlQueue for ControlTask to process - this is the only thing
    /// the MQTT event handler does, so ControlTask remains the sole place
    /// actuator/mode decisions are made.
    esp_err_t start(QueueHandle_t controlQueue);

    void publishSensor(const char *pName, float value);
    void publishPeripheralState(const char *pName, bool on);
    void publishModeState(IncubatorMode mode);
    void publishPeripheralValue(const char *pName, float value);

private:
    esp_mqtt_client_handle_t client_ = nullptr;
    QueueHandle_t controlQueue_ = nullptr;

    void publish(const char *pTopic, const char *pPayload);
    void handleIncomingMessage(const char *pTopic, int topicLen, const char *pData, int dataLen);

    static void eventHandlerTrampoline(void *pHandlerArgs, esp_event_base_t base,
                                        int32_t eventId, void *pEventData);
    void handleEvent(int32_t eventId, void *pEventData);
};

#endif /* MQTT_LINK_H */
