#pragma once

#include "driver/gpio.h"

namespace io_pins {

// Button inputs (normally open, pulled up — active low)
constexpr gpio_num_t BUTTON_LEFT  = GPIO_NUM_21;
constexpr gpio_num_t BUTTON_RIGHT = GPIO_NUM_20;

// SPI bus for MCP2515
constexpr int        SPI_CLK      = 6;
constexpr int        SPI_MOSI     = 7;
constexpr int        SPI_MISO     = 4;
constexpr gpio_num_t SPI_CS       = GPIO_NUM_5;
constexpr gpio_num_t MCP2515_INT  = GPIO_NUM_3;

}  // namespace io_pins
