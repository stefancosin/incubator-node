#include "dht11.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "dht11";

/* Generous per-edge timeout - real DHT11 edges are 20-80us apart, so 100us
   without the expected transition means the sensor isn't responding */
#define EDGE_TIMEOUT_US 100

void Dht11::init(gpio_num_t pin) {
    pin_ = pin;

    gpio_reset_pin(pin_);
    gpio_set_direction(pin_, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(pin_, GPIO_PULLUP_ONLY);
    gpio_set_level(pin_, 1);
}

esp_err_t Dht11::waitForLevel(int level, int timeoutUs) {

    int elapsed = 0;

    while (gpio_get_level(pin_) != level) {
        if (elapsed >= timeoutUs) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(1);
        elapsed++;
    }

    return ESP_OK;
}

esp_err_t Dht11::read(float& rOutHumidity, float& rOutTemperature) {

    uint8_t data[5] = {0, 0, 0, 0, 0};
    esp_err_t ret = ESP_OK;

    /* Start signal: pull low >=18ms, then release and switch to input so
       the sensor can drive the line for its response + the 40 data bits */
    gpio_set_direction(pin_, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin_, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    /* The whole handshake below is a few hundred microseconds of tight
       edge timing - a task switch or interrupt landing in the middle
       would misread a bit, so this section runs with interrupts/scheduler
       held off on this core (matches how every bit-banged DHT11/DHT22
       driver on ESP32 does this). Kept as short as possible. */
    portENTER_CRITICAL(&mux_);

    gpio_set_level(pin_, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin_, GPIO_MODE_INPUT);

    /* Sensor response: 80us low, then 80us high */
    if (waitForLevel(0, EDGE_TIMEOUT_US) != ESP_OK) { ret = ESP_ERR_TIMEOUT; goto done; }
    if (waitForLevel(1, EDGE_TIMEOUT_US) != ESP_OK) { ret = ESP_ERR_TIMEOUT; goto done; }
    if (waitForLevel(0, EDGE_TIMEOUT_US) != ESP_OK) { ret = ESP_ERR_TIMEOUT; goto done; }

    /* 40 data bits: each starts with a 50us low, followed by a high pulse
       whose length encodes the bit - ~26-28us for 0, ~70us for 1 */
    for (int i = 0; i < 40; i++) {
        if (waitForLevel(1, EDGE_TIMEOUT_US) != ESP_OK) { ret = ESP_ERR_TIMEOUT; goto done; }

        int64_t highStart = esp_timer_get_time();
        if (waitForLevel(0, EDGE_TIMEOUT_US) != ESP_OK) { ret = ESP_ERR_TIMEOUT; goto done; }
        int64_t highDurationUs = esp_timer_get_time() - highStart;

        data[i / 8] <<= 1;
        if (highDurationUs > 40) {
            data[i / 8] |= 1;
        }
    }

done:
    gpio_set_direction(pin_, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(pin_, 1);
    portEXIT_CRITICAL(&mux_);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);

    /* Temporary while chasing the "reads succeed but humidity=0" report -
       shows exactly what the sensor sent (checksum pass or fail) instead
       of just the parsed result, since a coincidental checksum match on
       a misread byte would otherwise look identical to a genuine 0% */
    ESP_LOGI(TAG, "raw bytes: %02X %02X %02X %02X %02X (checksum %s)",
             data[0], data[1], data[2], data[3], data[4],
             (checksum == data[4]) ? "OK" : "MISMATCH");

    if (checksum != data[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    /* DHT11 (unlike DHT22) only has integer resolution - the second byte
       of each pair is always 0, so only data[0]/data[2] matter */
    rOutHumidity    = (float) data[0];
    rOutTemperature = (float) data[2];

    return ESP_OK;
}
