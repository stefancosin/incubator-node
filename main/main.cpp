#include "incubator_state.h"
#include "incubator_events.h"
#include "wifi_connect.h"
#include "mqtt_link.h"
#include "sensor_task.h"
#include "control_task.h"
#include "motor_task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "main";

/* ESP-IDF's startup code looks up app_main() with C linkage - this is the
   C++ project's equivalent of main(), there is no separate int main(). */
extern "C" void app_main(void) {

    static IncubatorState state;
    ESP_ERROR_CHECK(state.init());

    esp_err_t nvsErr = nvs_flash_init();
    if ((nvsErr == ESP_ERR_NVS_NO_FREE_PAGES) || (nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsErr = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsErr);

    static WifiConnection wifi;
    ESP_ERROR_CHECK(wifi.connectAndWait());

    /* controlQueue: SensorTask + MqttLink -> ControlTask
       motorQueue:   ControlTask (forwarding MqttLink commands) -> MotorTask
       These two queues are the only synchronization these tasks need - no
       shared hardware state is ever touched from more than one task. */
    QueueHandle_t controlQueue = xQueueCreate(10, sizeof(ControlEvent));
    QueueHandle_t motorQueue = xQueueCreate(5, sizeof(MotorCmd));

    static MqttLink mqtt;
    ESP_ERROR_CHECK(mqtt.start(controlQueue));

    /* static: these tasks run forever, so the objects backing them must
       outlive app_main()'s stack frame without resorting to heap alloc */
    static ControlTask controlTask;
    static MotorTask motorTask;
    static SensorTask sensorTask;

    controlTask.start(controlQueue, motorQueue, state, mqtt);
    motorTask.start(motorQueue, state, mqtt);
    sensorTask.start(controlQueue, mqtt);

    ESP_LOGI(TAG, "Incubator node started");
}
