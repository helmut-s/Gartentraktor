#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "control_logic.h"
#include "ota_server.h"
#include "wifi_service.h"

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  control_logic_init();
  wifi_init_sta(app_config::WIFI_SSID, app_config::WIFI_PASSWORD);
  ESP_ERROR_CHECK(ota_server_start());

  while (true) {
    control_logic_step();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
