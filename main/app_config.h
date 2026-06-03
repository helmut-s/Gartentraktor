#pragma once

namespace app_config {

constexpr char WIFI_SSID[]     = "spieot";
constexpr char WIFI_PASSWORD[] = "spieIoTpass33";

// VESC controller ID as set in VESC Tool (0-based)
constexpr uint8_t VESC_ID_01 = 1;

// Current to apply when drive button is held
constexpr float DRIVE_CURRENT_A = 1.0f;

}  // namespace app_config
