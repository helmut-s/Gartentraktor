#include "control_logic.h"

#include "driver/gpio.h"
#include "esp_err.h"

#include "app_config.h"
#include "io_pins.h"
#include "vesc_can.h"

static bool s_driving = false;

void control_logic_init(void) {
    gpio_config_t in_cfg = {};
    in_cfg.pin_bit_mask = (1ULL << io_pins::BUTTON_LEFT) | (1ULL << io_pins::BUTTON_RIGHT);
    in_cfg.mode         = GPIO_MODE_INPUT;
    in_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    ESP_ERROR_CHECK(vesc_can_init());
}

void control_logic_step(void) {
    vesc_can_poll();

    bool button_right = (gpio_get_level(io_pins::BUTTON_RIGHT) == 0); // active low

    if (button_right && !s_driving) {
        vesc_can_set_current(app_config::VESC_ID_01, app_config::DRIVE_CURRENT_A);
        s_driving = true;
    } else if (!button_right && s_driving) {
        vesc_can_set_current(app_config::VESC_ID_01, 0.0f);
        s_driving = false;
    }
}
