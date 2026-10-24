#include "sensor_task.h"
#include "incubator_config.h"
#include "incubator_events.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "sensor_task";

void SensorTask::taskEntry(void *pvParameters) {
    static_cast<SensorTask *>(pvParameters)->run();
}

void SensorTask::run() {

    dht11_.init(DHT11_GPIO);

    for (;;) {
        float humidity = 0.0f;
        float temperature = 0.0f;
        esp_err_t err = dht11_.read(humidity, temperature);
        bool valid = (err == ESP_OK);

        if (!valid) {
            ESP_LOGW(TAG, "DHT11 read failed: %s", esp_err_to_name(err));
        } else {
            pMqtt_->publishSensor(SENSOR_NAME_TEMPERATURE, temperature);
            pMqtt_->publishSensor(SENSOR_NAME_HUMIDITY, humidity);
        }

        ControlEvent evt{};
        evt.type = ControlEventType::SensorReading;
        evt.data.sensor.temperature = temperature;
        evt.data.sensor.humidity = humidity;
        evt.data.sensor.valid = valid;

        if (xQueueSend(controlQueue_, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "control queue full, dropped sensor reading");
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

void SensorTask::start(QueueHandle_t controlQueue, MqttLink& rMqtt) {
    controlQueue_ = controlQueue;
    pMqtt_ = &rMqtt;

    /* Pinned to core 1, away from core 0 where the WiFi driver runs - the
       DHT11 read's bit-banging is microsecond-sensitive, and standalone
       testing showed 29/29 clean reads with no WiFi running vs. frequent
       timeouts with the full firmware (WiFi+MQTT+motor) active, so this
       reduces contention on the core actually doing the timing-critical
       work. Doesn't fix electrical noise from WiFi TX bursts or the motor
       - see the decoupling capacitor recommendation for that. */
    xTaskCreatePinnedToCore(&SensorTask::taskEntry, "sensor_task", 4096, this, 5, nullptr, 1);
}
