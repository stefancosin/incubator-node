#ifndef DHT11_H
#define DHT11_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

class Dht11 {
public:
    /// Configures the data pin. Call once at startup.
    void init(gpio_num_t pin);

    /// Runs one full read cycle (start pulse, sensor response, 40 data
    /// bits) and validates the checksum.
    /// @return ESP_OK on success, ESP_ERR_TIMEOUT if the sensor didn't
    ///         respond in time, ESP_ERR_INVALID_CRC on checksum mismatch.
    esp_err_t read(float& rOutHumidity, float& rOutTemperature);

private:
    gpio_num_t pin_ = GPIO_NUM_NC;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    esp_err_t waitForLevel(int level, int timeoutUs);
};

#endif /* DHT11_H */
