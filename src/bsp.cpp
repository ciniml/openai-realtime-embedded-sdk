#include "bsp.h"

#include <esp_log.h>

constexpr const char* TAG = "oai_bsp";

#ifdef CONFIG_BSP_RESET_PROVISIONING_NONE
bool bsp_check_reset_provisioning() {
  return false;
}
#endif
#ifdef CONFIG_BSP_RESET_PROVISIONING_GPIO
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

bool bsp_check_reset_provisioning() {
  gpio_reset_pin(gpio_num_t(CONFIG_BSP_RESET_PROVISIONING_GPIO_NUM));
  const gpio_config_t config = {
    .pin_bit_mask = 1ULL << CONFIG_BSP_RESET_PROVISIONING_GPIO_NUM,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  if( auto err = gpio_config(&config); err != ESP_OK ) {
    ESP_LOGE(TAG, "Failed to configure reset provisioning GPIO - %d", err);
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  bool pressed = gpio_get_level(gpio_num_t(CONFIG_BSP_RESET_PROVISIONING_GPIO_NUM)) == (CONFIG_BSP_RESET_PROVISIONING_GPIO_LEVEL_LOW ? 0 : 1);
  ESP_LOGI(TAG, "Reset provisioning button %s", pressed ? "pressed" : "not pressed");
  return pressed;
}
#endif