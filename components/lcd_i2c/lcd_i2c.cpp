#include "lcd_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include <cstdio>

/* PCF8574 backpack pin mapping used by every "LCD1602 I2C" module
   (the same one the LiquidCrystal_I2C Arduino library targets):
   P0=RS  P1=RW  P2=EN  P3=Backlight  P4-P7=D4-D7 */
#define LCD_BIT_RS         0x01
#define LCD_BIT_RW         0x02
#define LCD_BIT_ENABLE     0x04
#define LCD_BIT_BACKLIGHT  0x08

#define LCD_CMD_CLEAR         0x01
#define LCD_CMD_ENTRY_MODE    0x06
#define LCD_CMD_DISPLAY_ON    0x0C
#define LCD_CMD_FUNCTION_SET  0x28 /* 4-bit, 2 line, 5x8 dots */
#define LCD_CMD_DISPLAY_OFF   0x08

#define LCD_COLS 16

/* Bit-bangs SDA/SCL by hand to recover a wedged I2C bus before handing the
   pins back to the I2C peripheral. EMI from a nearby relay/mains load can
   corrupt a byte mid-transaction, and if that happens while the PCF8574
   is mid-clock-cycle it can be left holding SDA low waiting for clocks
   that never come the normal way - i2c_driver_install() alone does not
   undo that, it just reconfigures the peripheral on top of a bus that's
   still stuck. Standard recovery: toggle SCL (with SDA released) up to 9
   times so a wedged slave finishes clocking out whatever it was holding,
   then issue a manual STOP condition. */
static void i2cBusRecover(gpio_num_t sdaPin, gpio_num_t sclPin) {

    gpio_set_direction(sdaPin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_direction(sclPin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(sdaPin, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(sclPin, GPIO_PULLUP_ONLY);

    gpio_set_level(sdaPin, 1);
    gpio_set_level(sclPin, 1);
    esp_rom_delay_us(5);

    for (int i = 0; i < 9; i++) {
        if (gpio_get_level(sdaPin) == 1) {
            break; /* slave already released the bus */
        }
        gpio_set_level(sclPin, 0);
        esp_rom_delay_us(5);
        gpio_set_level(sclPin, 1);
        esp_rom_delay_us(5);
    }

    /* Manual STOP: SDA low-to-high while SCL is held high */
    gpio_set_level(sdaPin, 0);
    esp_rom_delay_us(5);
    gpio_set_level(sclPin, 1);
    esp_rom_delay_us(5);
    gpio_set_level(sdaPin, 1);
    esp_rom_delay_us(5);
}

esp_err_t LcdI2c::i2cWriteByte(uint8_t data) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr_ << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port_, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return ret;
}

void LcdI2c::pulseEnable(uint8_t data) {
    i2cWriteByte(data | LCD_BIT_ENABLE);
    esp_rom_delay_us(1);
    i2cWriteByte(data & (uint8_t) ~LCD_BIT_ENABLE);
    esp_rom_delay_us(50);
}

/* Backlight is wired into every byte we send, since the backpack has no
   separate "keep backlight on" latch - matches lcd.backlight() being
   called once at init in the original firmware (we just never clear it) */
void LcdI2c::sendNibble(uint8_t nibble, bool isData) {
    uint8_t data = (uint8_t)((nibble & 0xF0) | LCD_BIT_BACKLIGHT | (isData ? LCD_BIT_RS : 0));
    i2cWriteByte(data);
    pulseEnable(data);
}

void LcdI2c::sendByte(uint8_t value, bool isData) {
    sendNibble(value & 0xF0, isData);
    sendNibble((uint8_t)(value << 4) & 0xF0, isData);
}

void LcdI2c::command(uint8_t cmd) {
    sendByte(cmd, false);
}

void LcdI2c::writeData(uint8_t value) {
    sendByte(value, true);
}

esp_err_t LcdI2c::init(i2c_port_t port, uint8_t address, gpio_num_t sdaPin, gpio_num_t sclPin) {

    port_ = port;
    addr_ = address;

    /* Harmless if this is the very first init (nothing installed yet,
       delete just no-ops with an error we ignore) - but essential on a
       re-init after suspected EMI corruption, since a stuck bus needs
       manual recovery before the peripheral is reconfigured on top of it */
    i2c_driver_delete(port_);
    i2cBusRecover(sdaPin, sclPin);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sdaPin;
    conf.scl_io_num = sclPin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;

    esp_err_t ret = i2c_param_config(port_, &conf);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2c_driver_install(port_, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); /* HD44780 power-on settle time */

    /* Forces the display into 4-bit mode regardless of its current state -
       this exact three-nibbles-then-switch sequence is straight out of the
       HD44780 datasheet's "initializing by instruction" flow */
    sendNibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(5));
    sendNibble(0x30, false);
    esp_rom_delay_us(150);
    sendNibble(0x30, false);
    sendNibble(0x20, false);

    command(LCD_CMD_FUNCTION_SET);
    command(LCD_CMD_DISPLAY_OFF);
    command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
    command(LCD_CMD_ENTRY_MODE);
    command(LCD_CMD_DISPLAY_ON);

    return ESP_OK;
}

void LcdI2c::clear() {
    command(LCD_CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2)); /* clear needs longer to complete than other commands */
}

void LcdI2c::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t rowOffsets[] = {0x00, 0x40};
    command((uint8_t)(0x80 | (col + rowOffsets[row & 0x01])));
}

void LcdI2c::print(const char *pText) {
    for (const char *p = pText; *p != '\0'; p++) {
        writeData((uint8_t) *p);
    }
}

void LcdI2c::writeBootScreen() {
    clear();
    setCursor(6, 0);
    print("Smart");
    setCursor(4, 1);
    print("Incubator");
}

void LcdI2c::writeSensorFault() {
    clear();
    setCursor(0, 0);
    print("Sensor Error!");
}

void LcdI2c::writeTemperature(float temperature) {
    char content[LCD_COLS + 1];
    /* Adjacent string literals, not one "\xDFC" - a hex escape consumes
       every following hex digit, and 'C' is a valid one, so "\xDFC" would
       parse as the single out-of-range escape 0xDFC instead of 0xDF + 'C' */
    snprintf(content, sizeof(content), "Temp: %.1f" "\xDF" "C", temperature);

    /* Left-padded to the full row width - without this, a shorter string
       (e.g. after "Sensor Error!") leaves trailing characters from
       whatever was previously on this row still on screen */
    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-*s", LCD_COLS, content);

    setCursor(0, 0);
    print(buf);
}

void LcdI2c::writeHumidity(float humidity) {
    char content[LCD_COLS + 1];
    snprintf(content, sizeof(content), "Hum: %.1f%%", humidity);

    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-*s", LCD_COLS, content);

    setCursor(0, 1);
    print(buf);
}
