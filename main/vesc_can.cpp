#include "vesc_can.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_mcp2515.h"
#include "freertos/FreeRTOS.h"

#include "io_pins.h"

static const char *TAG = "vesc_can";

// VESC CAN packet IDs (from VESC firmware datatypes.h)
#define VESC_CMD_SET_DUTY              0
#define VESC_CMD_SET_CURRENT           1
#define VESC_CMD_SET_CURRENT_BRAKE     2
#define VESC_CMD_SET_RPM               3
#define VESC_CMD_SET_CURRENT_REL           10
#define VESC_CMD_SET_CURRENT_BRAKE_REL     11
#define VESC_CMD_SET_CURRENT_HANDBRAKE     12
#define VESC_CMD_SET_CURRENT_HANDBRAKE_REL 13
#define VESC_CMD_STATUS      9   // periodic: ERPM, current, duty
#define VESC_CMD_STATUS_5   27   // periodic: tachometer, input voltage

#define MAX_VESC_ID 16

static twai_node_handle_t s_node;
static vesc_status_t      s_status[MAX_VESC_ID];

static inline int32_t be32(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                     (uint32_t)p[2] <<  8 | p[3]);
}

static inline int16_t be16(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}

// Called from ISR on every received frame
static bool on_rx_done(twai_node_handle_t node, const twai_rx_done_event_data_t *, void *) {
    uint8_t buf[8] = {};
    twai_frame_t frame = {};
    frame.buffer     = buf;
    frame.buffer_len = sizeof(buf);

    if (twai_node_receive_from_isr(node, &frame) != ESP_OK) return false;
    if (!frame.header.ide) return false;  // ignore standard frames

    uint8_t vid = frame.header.id & 0xFF;
    uint8_t cmd = (frame.header.id >> 8) & 0xFF;
    if (vid >= MAX_VESC_ID) return false;

    vesc_status_t *s = &s_status[vid];
    if (cmd == VESC_CMD_STATUS && frame.header.dlc >= 8) {
        s->erpm      = (float)be32(buf);
        s->current_a = (float)be16(buf + 4) / 10.0f;
        s->duty      = (float)be16(buf + 6) / 1000.0f;
        s->valid     = true;
    } else if (cmd == VESC_CMD_STATUS_5 && frame.header.dlc >= 6) {
        s->tachometer = be32(buf);
        s->voltage_v  = (float)be16(buf + 4) / 10.0f;
    }
    return false;
}

esp_err_t vesc_can_init(void) {
    spi_bus_config_t bus = {};
    bus.sclk_io_num   = io_pins::SPI_CLK;
    bus.mosi_io_num   = io_pins::SPI_MOSI;
    bus.miso_io_num   = io_pins::SPI_MISO;
    bus.quadwp_io_num = GPIO_NUM_NC;
    bus.quadhd_io_num = GPIO_NUM_NC;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "SPI bus init failed");

    // GPIO ISR service is required by the MCP2515 component for the INT pin
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service: %s", esp_err_to_name(err));
        return err;
    }

    twai_mcp2515_node_config_t cfg = {};
    cfg.io_cfg.cs_gpio        = io_pins::SPI_CS;
    cfg.io_cfg.int_gpio       = io_pins::MCP2515_INT;
    cfg.spi_clock_hz          = 10 * 1000 * 1000;
    cfg.oscillator_hz         = 8 * 1000 * 1000;
    cfg.bit_timing.bitrate    = 500000;
    cfg.bit_timing.sp_permill = 875;
    cfg.tx_queue_depth        = 8;
    cfg.fail_retry_cnt        = -1;  // auto-retry

    ESP_RETURN_ON_ERROR(twai_new_node_mcp2515(SPI2_HOST, &cfg, &s_node), TAG, "node create failed");

    twai_event_callbacks_t cbs = {};
    cbs.on_rx_done = on_rx_done;
    ESP_RETURN_ON_ERROR(twai_node_register_event_callbacks(s_node, &cbs, NULL), TAG, "register cb failed");
    ESP_RETURN_ON_ERROR(twai_node_enable(s_node), TAG, "node enable failed");

    memset(s_status, 0, sizeof(s_status));
    ESP_LOGI(TAG, "ready at 500 kbps");
    return ESP_OK;
}

static void send_ext(uint8_t vesc_id, uint8_t cmd, const uint8_t *data, uint8_t len) {
    twai_frame_t frame = {};
    frame.header.id  = ((uint32_t)cmd << 8) | vesc_id;
    frame.header.ide = 1;
    frame.header.dlc = len;
    frame.buffer     = (uint8_t *)data;
    frame.buffer_len = len;
    if (twai_node_transmit(s_node, &frame, 10) != ESP_OK) {
        ESP_LOGW(TAG, "tx failed vesc=%u cmd=%u", vesc_id, cmd);
    }
}

static void send_millivalue(uint8_t id, uint8_t cmd, float val) {
    int32_t v = (int32_t)(val * 1000.0f);
    uint8_t d[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    send_ext(id, cmd, d, 4);
}

static void send_scaled(uint8_t id, uint8_t cmd, float val) {
    int32_t v = (int32_t)(val * 100000.0f);
    uint8_t d[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    send_ext(id, cmd, d, 4);
}

void vesc_can_set_current(uint8_t id, float amps) {
    send_millivalue(id, VESC_CMD_SET_CURRENT, amps);
}

void vesc_can_set_current_rel(uint8_t id, float rel) {
    send_scaled(id, VESC_CMD_SET_CURRENT_REL, rel);
}

void vesc_can_set_current_brake(uint8_t id, float amps) {
    send_millivalue(id, VESC_CMD_SET_CURRENT_BRAKE, amps);
}

void vesc_can_set_current_brake_rel(uint8_t id, float rel) {
    send_scaled(id, VESC_CMD_SET_CURRENT_BRAKE_REL, rel);
}

void vesc_can_set_current_handbrake(uint8_t id, float amps) {
    send_millivalue(id, VESC_CMD_SET_CURRENT_HANDBRAKE, amps);
}

void vesc_can_set_current_handbrake_rel(uint8_t id, float rel) {
    send_scaled(id, VESC_CMD_SET_CURRENT_HANDBRAKE_REL, rel);
}

void vesc_can_set_duty(uint8_t id, float duty) {
    send_scaled(id, VESC_CMD_SET_DUTY, duty);
}

void vesc_can_set_rpm(uint8_t id, int32_t rpm) {
    uint8_t d[4] = { (uint8_t)(rpm >> 24), (uint8_t)(rpm >> 16), (uint8_t)(rpm >> 8), (uint8_t)rpm };
    send_ext(id, VESC_CMD_SET_RPM, d, 4);
}

void vesc_can_poll(void) {
    // Reception is interrupt-driven via on_rx_done; nothing to poll
}

bool vesc_can_get_status(uint8_t id, vesc_status_t *out) {
    if (id >= MAX_VESC_ID || !s_status[id].valid) return false;
    *out = s_status[id];
    return true;
}
