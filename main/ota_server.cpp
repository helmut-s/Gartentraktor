#include "ota_server.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota_server";

static esp_err_t root_get_handler(httpd_req_t *req) {
  const char *resp = "Hallo du Arsch";
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ota_get_handler(httpd_req_t *req) {
  const char *html =
      "<!doctype html><html><body>"
      "<h3>OTA Upload</h3>"
      "<input type='file' id='fw'/><button onclick='u()'>Upload</button>"
      "<pre id='s'></pre>"
      "<script>"
      "async function u(){"
      "const f=document.getElementById('fw').files[0];"
      "if(!f){return;}"
      "document.getElementById('s').textContent='Uploading...';"
      "const r=await fetch('/ota',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f});"
      "document.getElementById('s').textContent=await r.text();"
      "}"
      "</script></body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL) {
    ESP_LOGE(TAG, "OTA: no update partition");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No update partition");
    return ESP_FAIL;
  }

  esp_ota_handle_t ota_handle = 0;
  esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
    return err;
  }

  char buf[2048];
  int remaining = req->content_len;

  while (remaining > 0) {
    int recv_len =
        httpd_req_recv(req, buf, (remaining > (int)sizeof(buf)) ? (int)sizeof(buf) : remaining);
    if (recv_len <= 0) {
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA receive failed");
      return ESP_FAIL;
    }

    err = esp_ota_write(ota_handle, buf, recv_len);
    if (err != ESP_OK) {
      esp_ota_abort(ota_handle);
      ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
      return err;
    }
    remaining -= recv_len;
  }

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
    return err;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA set boot partition failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA set boot failed");
    return err;
  }

  httpd_resp_sendstr(req, "Update OK. Rebooting...");
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
  return ESP_OK;
}

esp_err_t ota_server_start(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  esp_err_t err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return err;
  }

  httpd_uri_t root_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = root_get_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &root_uri);

  httpd_uri_t ota_get_uri = {
      .uri = "/ota",
      .method = HTTP_GET,
      .handler = ota_get_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &ota_get_uri);

  httpd_uri_t ota_post_uri = {
      .uri = "/ota",
      .method = HTTP_POST,
      .handler = ota_post_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(server, &ota_post_uri);

  return ESP_OK;
}
