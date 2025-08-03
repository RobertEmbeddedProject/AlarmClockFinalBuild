#include <stdio.h>
#include "mp3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

//DFPlayer Constants
#define CMD_PLAY_NEXT      0x01
#define CMD_PLAY_PREV      0x02
#define CMD_PLAY_W_INDEX   0x03
#define CMD_STOP           0x16  //Can you resume where stopped?
#define CMD_SET_VOLUME     0x06
#define CMD_SET_PRESET     0x07  //Add this feature
/*  0 = Normal
    1 = Pop
    2 = Rock
    3 = Jazz
    4 = Classic
    5 = Blues     */
#define CMD_SLEEP_MODE     0x0A  //Add this feature
#define CMD_SEL_DEV        0x09  //What does this actually do?
#define DEV_TF             0x02  //What does this actually do?

//GPIO Assignments
#define DFPLAYER_RESET_GPIO     4
#define KY040_BUTTON_GPIO_L    36    //move this to KY040 driver
#define KY040_BUTTON_GPIO_R    39    //move this to KY040 driver
#define PAM_ENABLE_GPIO        19

//UART Assignments
#define UART_PORT_NUM      UART_NUM_2
#define UART_TX_PIN        17
#define UART_RX_PIN        16
#define UART_BUF_SIZE      1024

//DFPlayer Command Function
void mp3_command(int8_t command, int16_t dat)
{
    uint8_t frame[8] = {
        0x7E, 0xFF, 0x06, command, 0x00,
        (uint8_t)(dat >> 8), (uint8_t)dat,
        0xEF
    };
    uart_write_bytes(UART_PORT_NUM, (const char *)frame, sizeof(frame));
}

//DFPlayer and PAM initialization
void init_dfplayer_and_pam()
{
    //PAM EN + DFPlayer RESET GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    //Configure PAM and DFPlayer reset
    io_conf.pin_bit_mask = (1ULL << PAM_ENABLE_GPIO) | (1ULL << DFPLAYER_RESET_GPIO);
    gpio_config(&io_conf);

    //PAM startup delay
    gpio_set_level(PAM_ENABLE_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PAM_ENABLE_GPIO, 1);

    //DFPlayer reset pulse
    gpio_set_level(DFPLAYER_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(DFPLAYER_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    //UART2 init
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, 0, 0, NULL, 0);

    //DFPlayer startup commands
    mp3_command(CMD_SEL_DEV, DEV_TF);
    vTaskDelay(pdMS_TO_TICKS(200));
    mp3_command(CMD_SET_VOLUME, 30);
}