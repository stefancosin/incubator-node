#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

class WifiConnection {
public:
    /// Brings up WiFi station mode and blocks until either an IP is
    /// obtained or connection retries are exhausted.
    /// @return ESP_OK once connected, ESP_FAIL if retries were exhausted.
    esp_err_t connectAndWait();

private:
    EventGroupHandle_t eventGroup_ = nullptr;
    int retryCount_ = 0;

    static void eventHandlerTrampoline(void *pArg, esp_event_base_t base, int32_t id, void *pData);
    void handleEvent(esp_event_base_t base, int32_t id, void *pData);
};

#endif /* WIFI_CONNECT_H */
