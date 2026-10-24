#include "mqtt_link.h"
#include "incubator_config.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char *TAG = "mqtt_link";

#define MAX_TOPIC_LEN   128
#define MAX_PAYLOAD_LEN 32
#define MAX_SEGMENTS    8

void MqttLink::handleIncomingMessage(const char *pTopic, int topicLen, const char *pData, int dataLen) {

    char topicBuf[MAX_TOPIC_LEN];
    char payloadBuf[MAX_PAYLOAD_LEN];

    if ((topicLen >= (int) sizeof(topicBuf)) || (dataLen >= (int) sizeof(payloadBuf))) {
        ESP_LOGW(TAG, "Incoming MQTT message too long, ignoring");
        return;
    }

    std::memcpy(topicBuf, pTopic, topicLen);
    topicBuf[topicLen] = '\0';
    std::memcpy(payloadBuf, pData, dataLen);
    payloadBuf[dataLen] = '\0';

    char *segments[MAX_SEGMENTS];
    int segmentCount = 0;
    char *token = std::strtok(topicBuf, "/");
    while ((token != nullptr) && (segmentCount < MAX_SEGMENTS)) {
        segments[segmentCount++] = token;
        token = std::strtok(nullptr, "/");
    }

    if ((segmentCount != 4) || (std::strcmp(segments[0], "incubator") != 0)
        || (std::strcmp(segments[1], "peripherals") != 0) || (std::strcmp(segments[3], "set") != 0)) {
        ESP_LOGW(TAG, "Unrecognized topic (%d segments)", segmentCount);
        return;
    }

    const char *name = segments[2];
    ControlEvent evt{};

    if (std::strcmp(name, PERIPHERAL_NAME_MODE) == 0) {
        if (std::strcmp(payloadBuf, "auto") == 0) {
            evt.type = ControlEventType::ModeSet;
            evt.data.mode.mode = IncubatorMode::Auto;
        } else if (std::strcmp(payloadBuf, "manual") == 0) {
            evt.type = ControlEventType::ModeSet;
            evt.data.mode.mode = IncubatorMode::Manual;
        } else {
            ESP_LOGW(TAG, "Invalid mode payload \"%s\"", payloadBuf);
            return;
        }
    } else if ((std::strcmp(name, PERIPHERAL_NAME_TEMP_LOW) == 0)
               || (std::strcmp(name, PERIPHERAL_NAME_TEMP_HIGH) == 0)) {
        char *pEnd = nullptr;
        float value = std::strtof(payloadBuf, &pEnd);
        if ((pEnd == payloadBuf) || (value < TEMP_THRESHOLD_MIN_C) || (value > TEMP_THRESHOLD_MAX_C)) {
            ESP_LOGW(TAG, "Invalid threshold payload \"%s\" for \"%s\"", payloadBuf, name);
            return;
        }

        evt.type = (std::strcmp(name, PERIPHERAL_NAME_TEMP_LOW) == 0)
                       ? ControlEventType::TempLowSet
                       : ControlEventType::TempHighSet;
        evt.data.tempThreshold.value = value;
    } else {
        PeripheralId peripheral;
        if (std::strcmp(name, PERIPHERAL_NAME_HEATER) == 0) {
            peripheral = PeripheralId::Heater;
        } else if (std::strcmp(name, PERIPHERAL_NAME_COOLER) == 0) {
            peripheral = PeripheralId::Cooler;
        } else if (std::strcmp(name, PERIPHERAL_NAME_ALARM) == 0) {
            peripheral = PeripheralId::Alarm;
        } else if (std::strcmp(name, PERIPHERAL_NAME_MOTOR) == 0) {
            peripheral = PeripheralId::Motor;
        } else {
            ESP_LOGW(TAG, "Unknown peripheral \"%s\"", name);
            return;
        }

        bool on;
        if (std::strcmp(payloadBuf, "on") == 0) {
            on = true;
        } else if (std::strcmp(payloadBuf, "off") == 0) {
            on = false;
        } else {
            ESP_LOGW(TAG, "Invalid actuator payload \"%s\"", payloadBuf);
            return;
        }

        evt.type = ControlEventType::ActuatorSet;
        evt.data.actuator.peripheral = peripheral;
        evt.data.actuator.on = on;
    }

    if (xQueueSend(controlQueue_, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "control queue full, dropped incoming MQTT command");
    }
}

void MqttLink::eventHandlerTrampoline(void *pHandlerArgs, esp_event_base_t /*base*/,
                                       int32_t eventId, void *pEventData) {
    static_cast<MqttLink *>(pHandlerArgs)->handleEvent(eventId, pEventData);
}

void MqttLink::handleEvent(int32_t eventId, void *pEventData) {

    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(pEventData);

    switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            esp_mqtt_client_subscribe(client_, MQTT_TOPIC_PERIPHERAL_SET_FILTER, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from broker");
            break;
        case MQTT_EVENT_DATA:
            handleIncomingMessage(event->topic, event->topic_len, event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error (type=%d)", event->error_handle->error_type);
            break;
        default:
            break;
    }
}

esp_err_t MqttLink::start(QueueHandle_t controlQueue) {

    controlQueue_ = controlQueue;

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = CONFIG_INCUBATOR_MQTT_BROKER_URI;
    cfg.credentials.client_id = CONFIG_INCUBATOR_MQTT_CLIENT_ID;

    client_ = esp_mqtt_client_init(&cfg);
    if (client_ == nullptr) {
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                    &MqttLink::eventHandlerTrampoline, this);

    return esp_mqtt_client_start(client_);
}

void MqttLink::publish(const char *pTopic, const char *pPayload) {
    if (client_ != nullptr) {
        esp_mqtt_client_publish(client_, pTopic, pPayload, 0, 1, false);
    }
}

void MqttLink::publishSensor(const char *pName, float value) {

    char topic[64];
    char payload[16];

    std::snprintf(topic, sizeof(topic), "%s%s", MQTT_TOPIC_SENSOR_PREFIX, pName);
    std::snprintf(payload, sizeof(payload), "%.1f", value);

    publish(topic, payload);
}

void MqttLink::publishPeripheralState(const char *pName, bool on) {

    char topic[64];
    std::snprintf(topic, sizeof(topic), "%s%s%s", MQTT_TOPIC_PERIPHERAL_PREFIX, pName,
                   MQTT_TOPIC_PERIPHERAL_STATE_SUFFIX);

    publish(topic, on ? "on" : "off");
}

void MqttLink::publishModeState(IncubatorMode mode) {

    char topic[64];
    std::snprintf(topic, sizeof(topic), "%s%s%s", MQTT_TOPIC_PERIPHERAL_PREFIX, PERIPHERAL_NAME_MODE,
                   MQTT_TOPIC_PERIPHERAL_STATE_SUFFIX);

    publish(topic, (mode == IncubatorMode::Auto) ? "auto" : "manual");
}

void MqttLink::publishPeripheralValue(const char *pName, float value) {

    char topic[64];
    char payload[16];

    std::snprintf(topic, sizeof(topic), "%s%s%s", MQTT_TOPIC_PERIPHERAL_PREFIX, pName,
                   MQTT_TOPIC_PERIPHERAL_STATE_SUFFIX);
    std::snprintf(payload, sizeof(payload), "%.1f", value);

    publish(topic, payload);
}
