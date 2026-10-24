#include "wifi_connect.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <cstring>

static const char *TAG = "wifi_connect";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

void WifiConnection::eventHandlerTrampoline(void *pArg, esp_event_base_t base, int32_t id, void *pData) {
    static_cast<WifiConnection *>(pArg)->handleEvent(base, id, pData);
}

void WifiConnection::handleEvent(esp_event_base_t base, int32_t id, void *pData) {

    if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_START)) {
        esp_wifi_connect();
    } else if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_DISCONNECTED)) {
        auto *pInfo = static_cast<wifi_event_sta_disconnected_t *>(pData);
        ESP_LOGW(TAG, "Disconnected, reason=%d", pInfo->reason);

        if (retryCount_ < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            retryCount_++;
            ESP_LOGW(TAG, "Retrying WiFi connection (%d/%d)", retryCount_, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(eventGroup_, WIFI_FAIL_BIT);
        }
    } else if ((base == IP_EVENT) && (id == IP_EVENT_STA_GOT_IP)) {
        retryCount_ = 0;
        xEventGroupSetBits(eventGroup_, WIFI_CONNECTED_BIT);
    }
}

esp_err_t WifiConnection::connectAndWait() {

    eventGroup_ = xEventGroupCreate();
    if (eventGroup_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initCfg));

    esp_event_handler_instance_t instanceWifi;
    esp_event_handler_instance_t instanceIp;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                          &WifiConnection::eventHandlerTrampoline,
                                                          this, &instanceWifi));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                          &WifiConnection::eventHandlerTrampoline,
                                                          this, &instanceIp));

    wifi_config_t wifiCfg = {};
    std::strncpy((char *) wifiCfg.sta.ssid, CONFIG_INCUBATOR_WIFI_SSID, sizeof(wifiCfg.sta.ssid) - 1);
    std::strncpy((char *) wifiCfg.sta.password, CONFIG_INCUBATOR_WIFI_PASSWORD, sizeof(wifiCfg.sta.password) - 1);
    wifiCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiCfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID \"%s\"", CONFIG_INCUBATOR_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(eventGroup_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "WiFi failed to connect after %d retries", WIFI_MAX_RETRY);
    return ESP_FAIL;
}
