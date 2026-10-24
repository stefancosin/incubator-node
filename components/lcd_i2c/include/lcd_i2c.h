#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <cstdint>
#include "esp_err.h"
#include "driver/i2c.h"

class LcdI2c {
public:
    /// Initializes the I2C bus and puts the HD44780 (via a PCF8574
    /// backpack) into 4-bit, 2-line mode. Call once at startup.
    esp_err_t init(i2c_port_t port, uint8_t address, gpio_num_t sdaPin, gpio_num_t sclPin);

    void clear();
    void setCursor(uint8_t col, uint8_t row);
    void print(const char *pText);

    /* Higher-level screens mirroring the original firmware's lcd.cpp */
    void writeBootScreen();
    void writeSensorFault();
    void writeTemperature(float temperature);
    void writeHumidity(float humidity);

private:
    i2c_port_t port_ = I2C_NUM_0;
    uint8_t addr_ = 0;

    esp_err_t i2cWriteByte(uint8_t data);
    void pulseEnable(uint8_t data);
    void sendNibble(uint8_t nibble, bool isData);
    void sendByte(uint8_t value, bool isData);
    void command(uint8_t cmd);
    void writeData(uint8_t value);
};

#endif /* LCD_I2C_H */
