#include <string.h>
#include "driver/i2c.h"
#include "ssd1309.h"
#include "smallfont.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define I2C_MASTER_NUM              I2C_NUM_0
#define OLED_RESET_GPIO             GPIO_NUM_23
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_FREQ_HZ          400000
#define OLED_ADDR                   0x3C
#define OLED_WIDTH                  128
#define OLED_HEIGHT                 64
#define OLED_BUF_SIZE               (OLED_WIDTH * OLED_HEIGHT / 8)
//Each buffer is 8 pixels vertically from bottom to top in order to create a "page"
static uint8_t oled_buffer[OLED_BUF_SIZE];

//Initialize I2C via ESP-IDFs standard HAL implementation
void i2c_master_init() {
    i2c_config_t conf = {  
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

//Send a command to the Display by initializing with 0x00 (datasheet), and then whatever Hex command
void ssd1309_cmd(uint8_t cmd) {
    uint8_t buffer[2] = {0x00, cmd}; // 0x00 = command stream (not data)
    i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR, buffer, 2, 1000 / portTICK_PERIOD_MS);
}

void ssd1309_init(void) {
    i2c_master_init();

    const uint8_t init_sequence[] = {
        0xAE, // Display off
        0xD5, 0x80, // Set display clock
        0xA8, 0x3F, // Multiplex
        0xD3, 0x00, // Display offset
        0x40, // Start line
        0x8D, 0x14, // Charge pump
        0x20, 0x00, // Memory mode: horizontal
        0xA1, // Seg remap
        0xC8, // COM scan dec
        0xDA, 0x12, // COM pins
        0b10000001, 0x0F, // Contrast
        0xD9, 0xF1, // Precharge
        0xDB, 0x40, // VCOM detect
        0xA4, // Resume RAM
        0xA6, // Normal display
        0xAF  // Display ON
    };

    for (int i = 0; i < sizeof(init_sequence); i++) {
        ssd1309_cmd(init_sequence[i]);
    }
}

void ssd1309_clear(void) {
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

void ssd1309_draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) return;
    const uint8_t *glyph = font8x8_basic[c - 32];

    for (int col = 0; col < 8; col++) {
    uint8_t column_data = 0;
    for (int row = 0; row < 8; row++) {
        column_data |= ((glyph[row] >> col) & 0x01) << row;
    }

    // Flip horizontal draw order:
    int index = x + (y * 128) + (7 - col);  // Flip col
    if (index < OLED_BUF_SIZE) {
        oled_buffer[index] = column_data;
    }
  }
}

void ssd1309_draw_text(int x, int y, const char *text) {
    while (*text) {
        ssd1309_draw_char(x, y, *text++);
        x += 8;
    }
}

void ssd1309_draw_hline(int x_start, int x_end, int y) {
    if (y < 0 || y >= OLED_HEIGHT) return;
    if (x_start > x_end) {
        int tmp = x_start;
        x_start = x_end;
        x_end = tmp;
    }

    if (x_start < 0) x_start = 0;
    if (x_end >= OLED_WIDTH) x_end = OLED_WIDTH - 1;

    int page = y / 8;
    uint8_t bit_mask = 1 << (y % 8);

    for (int x = x_start; x <= x_end; x++) {
        int index = page * OLED_WIDTH + x;
        if (index >= 0 && index < OLED_BUF_SIZE) {
            oled_buffer[index] |= bit_mask;
        }
    }
}


void ssd1309_reset(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OLED_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(OLED_RESET_GPIO, 0);  // Reset low
    vTaskDelay(pdMS_TO_TICKS(50));       // Hold low 50ms
    gpio_set_level(OLED_RESET_GPIO, 1);  // Reset high
    vTaskDelay(pdMS_TO_TICKS(50));       // Wait for OLED to boot
}

void ssd1309_display(void) {
    for (uint8_t page = 0; page < 8; page++) {
        ssd1309_cmd(0xB0 + page);  // Set page address
        ssd1309_cmd(0x00);         // Set lower column address
        ssd1309_cmd(0x10);         // Set higher column address

        uint8_t control = 0x40; // data stream
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, control, true);
        i2c_master_write(cmd, &oled_buffer[OLED_WIDTH * page], OLED_WIDTH, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
    }
}
