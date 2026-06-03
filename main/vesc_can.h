#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    float   erpm;        // electrical RPM
    float   current_a;   // motor current [A]
    float   duty;        // duty cycle [-1.0 .. 1.0]
    float   voltage_v;   // input voltage [V]
    int32_t tachometer;  // cumulative tachometer value
    bool    valid;       // true once first STATUS frame received
} vesc_status_t;

esp_err_t vesc_can_init(void);

// Send commands
void vesc_can_set_current(uint8_t id, float amps);
void vesc_can_set_current_rel(uint8_t id, float rel);         // -1.0 .. 1.0 relative to configured max
void vesc_can_set_current_brake(uint8_t id, float amps);
void vesc_can_set_current_brake_rel(uint8_t id, float rel);     // 0.0 .. 1.0 relative to configured max
void vesc_can_set_current_handbrake(uint8_t id, float amps);
void vesc_can_set_current_handbrake_rel(uint8_t id, float rel); // 0.0 .. 1.0 relative to configured max
void vesc_can_set_duty(uint8_t id, float duty);               // -1.0 .. 1.0
void vesc_can_set_rpm(uint8_t id, int32_t rpm);

// Call every control loop tick to drain the RX buffer and update status cache
void vesc_can_poll(void);

// Read last received telemetry; returns false if no data yet for this id
bool vesc_can_get_status(uint8_t id, vesc_status_t *out);
