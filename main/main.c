#include "ssd1309.h"  // Include the OLED driver
#include "mp3.h"      // Include MP3 Module Driver
#include "songs.h"
#include "freertos/FreeRTOS.h"   //RTOS implement for time vTaskDelay commands
#include "freertos/task.h"       //Task management API   xTaskCreate(), vTaskDelete(), vTaskDelay()
#include "driver/gpio.h"
#include "driver/pcnt.h"
#define CMD_PLAY_W_INDEX   0x03   //have to define this command if you want to use it in app_main

#define ROTARY_PCNT_INPUT_SIG 32   // CLK
#define ROTARY_PCNT_INPUT_CTRL 33  // DT
#define ROTARY_BUTTON GPIO_NUM_39  //PCNT (pulse counter) Implemented
#define ROTARY_PCNT_UNIT      PCNT_UNIT_0


/////////////////////////////////////////////////////////////////////////////////////////////////////////



void rotary_pcnt_init() {
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = ROTARY_PCNT_INPUT_SIG,   // CLK pin
        .ctrl_gpio_num = ROTARY_PCNT_INPUT_CTRL,   // DT pin
        .channel = PCNT_CHANNEL_0,
        .unit = ROTARY_PCNT_UNIT,
        .pos_mode = PCNT_COUNT_INC,   // Count up on CLK rising if DT low
        .neg_mode = PCNT_COUNT_DEC,   // Count down on CLK falling if DT high
        .lctrl_mode = PCNT_MODE_KEEP, // Don't invert counting if DT low
        .hctrl_mode = PCNT_MODE_REVERSE, // Invert if DT high
        .counter_h_lim = 1000,
        .counter_l_lim = -1000,
    };
    pcnt_unit_config(&pcnt_config);

    pcnt_counter_pause(ROTARY_PCNT_UNIT);
    pcnt_counter_clear(ROTARY_PCNT_UNIT);
    pcnt_counter_resume(ROTARY_PCNT_UNIT);
}

extern const char* songlist[41];  // from songs.h
volatile int current_index = 0;

void rotary_task(void *arg) {
    int16_t pcnt_value = 0;
    int last_index = -1;
    char index_string[4];

    while (1) {
        pcnt_get_counter_value(ROTARY_PCNT_UNIT, &pcnt_value);

        // Scale movement (e.g. every 4 detents = 1 item)
        int step_size = 2;
        int new_index = pcnt_value / step_size;

        //Clamp allows overrun to beginning/end
        if (new_index != last_index) {
            if (new_index < 0) new_index = 39;
            if (new_index > 39) new_index = 0;

            current_index = new_index;

            // OLED Update
            //ssd1309_clear();
            ssd1309_draw_text(0, 6, "                ");
            ssd1309_draw_text(0, 7, "                ");
            ssd1309_draw_text(0, 6, "Song Index:");
            sprintf(index_string, "%d", current_index);
            ssd1309_draw_text(90, 6, index_string);
            ssd1309_draw_text(0, 7, songlist[current_index]);
            ssd1309_display();

            last_index = new_index;
        }

        // Handle button (same as before)
        int btn = gpio_get_level(ROTARY_BUTTON);
        static int last_btn = 1;
        if (btn == 0 && last_btn == 1) {
            mp3_command(CMD_PLAY_W_INDEX, current_index+1);   //Offset because songs start at 0001 in MP3 module
        }
        last_btn = btn;

        vTaskDelay(pdMS_TO_TICKS(10)); // fast poll since PCNT is fast
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////

void app_main(void) {
/////////////////////////////////////////////////////////////////////////////////////////////
    gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = (1ULL << ROTARY_BUTTON),
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE
};
gpio_config(&io_conf);

rotary_pcnt_init();
xTaskCreate(rotary_task, "rotary_task", 4096, NULL, 10, NULL);
/////////////////////////////////////////////////////////////////////////////////////////////

    ssd1309_reset();
    ssd1309_init();                // Initialize I2C and OLED display
    ssd1309_clear();               // Clear the local framebuffer
    ssd1309_draw_text(10, 0, "Time:  9:00PM");
    ssd1309_draw_text(0, 1, "________________");
    ssd1309_draw_text(10, 2, "Alarm: 6:00AM");
    ssd1309_draw_text(0, 3, "________________");
    ssd1309_draw_text(10, 4, "Sleep: 9 Hrs");
    
    ssd1309_draw_text(0, 6, "NO ALARM");  //Max characters per line, FYI
    ssd1309_display();             // Push buffer to the OLED screen

    init_dfplayer_and_pam();      //Initialize MP3 Player

}


//Blinks text for testing
/*
    while(1){
    vTaskDelay(pdMS_TO_TICKS(500));
    ssd1309_cmd(0xAE);
    vTaskDelay(pdMS_TO_TICKS(100));
    ssd1309_cmd(0xAF);
    }
*/

