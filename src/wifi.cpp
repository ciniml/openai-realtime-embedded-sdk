#include <assert.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <qrcode.h>

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>
#include <esp_http_server.h>
#include <nvs.h>
#include <mdns.h>
#endif // CONFIG_USE_WIFI_PROVISIONING_SOFTAP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#include "main.h"
#include "bsp.h"

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
// From IDF examples/provisioning/wifi_prov_mgr/main/app_main.c
// Example of WiFi provisioning using SoftAP provisioning scheme
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

/* This salt,verifier has been generated for username = "wifiprov" and password = "abcd1234"
 * IMPORTANT NOTE: For production cases, this must be unique to every device
 * and should come from device manufacturing partition.*/
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};
#define QRCODE_BASE_URL         "https://espressif.github.io/esp-jumpstart/qrcode.html"

constexpr const char* OAI_NVS_NS = "oai";
constexpr const char* OAI_API_KEY_NVS_KEY = "oai_api_key";
constexpr const char* OAI_API_URI_NVS_KEY = "oai_api_uri";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static std::vector<char> s_api_key;

/**
 * Check if the API key is present
 */
esp_err_t oai_has_api_key(bool& has_api_key)
{
  has_api_key = false;
  if( s_api_key.size() > 0 ) {
    has_api_key = true;
    return ESP_OK;
  }

  nvs_handle_t nvs_handle;
  if( esp_err_t err = nvs_open(OAI_NVS_NS, NVS_READONLY, &nvs_handle); err != ESP_OK ) {
      return err;
  }
  
  size_t required_size = 0;
  esp_err_t err = nvs_get_str(nvs_handle, OAI_API_KEY_NVS_KEY, nullptr, &required_size);
  if( err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND ) {
      nvs_close(nvs_handle);
      return err;
  }

  nvs_close(nvs_handle);
  has_api_key = err != ESP_ERR_NVS_NOT_FOUND && required_size > 0;
  return ESP_OK;
}

/**
 * Get the API key from the NVS
 */
esp_err_t oai_get_api_key(std::vector<char>& api_key)
{
  if( s_api_key.size() > 0 ) {
    api_key = s_api_key;
    return ESP_OK;
  }

  nvs_handle_t nvs_handle;
  if( esp_err_t err = nvs_open(OAI_NVS_NS, NVS_READONLY, &nvs_handle); err != ESP_OK ) {
      return err;
  }

  size_t required_size = 0;
  if( esp_err_t err = nvs_get_str(nvs_handle, OAI_API_KEY_NVS_KEY, nullptr, &required_size); err != ESP_OK ) {
      nvs_close(nvs_handle);
      return err;
  }

  s_api_key.resize(required_size);
  if( esp_err_t err = nvs_get_str(nvs_handle, OAI_API_KEY_NVS_KEY, s_api_key.data(), &required_size); err != ESP_OK ) {
      nvs_close(nvs_handle);
      return err;
  }

  api_key = s_api_key;

  nvs_close(nvs_handle);
  return ESP_OK;
}

/**
 * Set the API key in the NVS
 */
esp_err_t oai_set_api_key(const char* api_key)
{
  assert(api_key != nullptr);

  // Store the new API key in the RAM.
  size_t api_key_len = strnlen(api_key, 1024);
  if( api_key_len == 0 ) {
    return ESP_ERR_INVALID_ARG;
  }
  s_api_key.resize(api_key_len + 1);
  strncpy(s_api_key.data(), api_key, api_key_len);
  s_api_key[api_key_len] = '\0';

  // Store the new API key in the NVS.
  nvs_handle_t nvs_handle;
  if( esp_err_t err = nvs_open(OAI_NVS_NS, NVS_READWRITE, &nvs_handle); err != ESP_OK ) {
      return err;
  }

  esp_err_t ret = nvs_set_str(nvs_handle, OAI_API_KEY_NVS_KEY, api_key);
  if( ret != ESP_OK ) {
      nvs_close(nvs_handle);
      return ret;
  }

  ret = nvs_commit(nvs_handle);
  if( ret != ESP_OK ) {
      nvs_close(nvs_handle);
      return ret;
  }

  nvs_close(nvs_handle);
  return ESP_OK;
}

/**
 * Get the API URI from the NVS or the default value
 */
esp_err_t oai_get_api_uri(std::string& api_uri)
{
  nvs_handle_t nvs_handle;
  if( esp_err_t err = nvs_open(OAI_NVS_NS, NVS_READONLY, &nvs_handle); err != ESP_OK ) {
      return err;
  }

  size_t required_size = 0;
  esp_err_t err = nvs_get_str(nvs_handle, OAI_API_URI_NVS_KEY, nullptr, &required_size);
  if( err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND ) {
      nvs_close(nvs_handle);
      return err;
  }

  if( err == ESP_ERR_NVS_NOT_FOUND ) {
    api_uri = OPENAI_REALTIMEAPI;
    return ESP_OK;
  }

  std::vector<char> api_uri_buf(required_size);
  if( esp_err_t err = nvs_get_str(nvs_handle, OAI_API_URI_NVS_KEY, api_uri_buf.data(), &required_size); err != ESP_OK ) {
      nvs_close(nvs_handle);
      return err;
  }

  api_uri = api_uri_buf.data();

  nvs_close(nvs_handle);
  return ESP_OK;
}

/**
 * Set the API URI in the NVS
 */
esp_err_t oai_set_api_uri(const char* api_uri)
{
  assert(api_uri != nullptr);

  nvs_handle_t nvs_handle;
  if( esp_err_t err = nvs_open(OAI_NVS_NS, NVS_READWRITE, &nvs_handle); err != ESP_OK ) {
      return err;
  }

  esp_err_t ret = nvs_set_str(nvs_handle, OAI_API_URI_NVS_KEY, api_uri);
  if( ret != ESP_OK ) {
      nvs_close(nvs_handle);
      return ret;
  }

  ret = nvs_commit(nvs_handle);
  if( ret != ESP_OK ) {
      nvs_close(nvs_handle);
      return ret;
  }

  nvs_close(nvs_handle);
  return ESP_OK;
}

static esp_err_t config_http_get_handler(httpd_req_t* req)
{
  if( strncmp(req->uri, "/", 2) == 0 || strncmp(req->uri, "/index.html", 11) == 0 ) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    size_t content_length = index_html_end - index_html_start;
    httpd_resp_send(req, reinterpret_cast<const char*>(index_html_start), content_length);
    return ESP_OK;
  } else if( strncmp(req->uri, "/api_key", 8) == 0 ) {
    std::vector<char> api_key;
    if( esp_err_t err = oai_get_api_key(api_key); err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND ) {
      httpd_resp_set_status(req, "500 Internal Server Error");
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, "Internal Server Error", 20);
      return err;
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, api_key.data(), api_key.size());
  } else if( strncmp(req->uri, "/api_uri", 8) == 0 ) {
    std::string api_uri = CONFIG_OPENAI_REALTIMEAPI;
    if( esp_err_t err = oai_get_api_uri(api_uri); err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND ) {
      httpd_resp_set_status(req, "500 Internal Server Error");
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send(req, "Internal Server Error", 20);
      return err;
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, api_uri.c_str(), api_uri.size());
  } else {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Not found", 9);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t config_http_post_handler(httpd_req_t* req)
{
  if( strncmp(req->uri, "/reboot", 8) == 0 ) {
    ESP_LOGI(LOG_TAG, "Rebooting the device...");
    esp_restart();
  }

  size_t buf_len = req->content_len + 1;
  std::vector<char> buf(buf_len);
  if( httpd_req_recv(req, buf.data(), buf_len) <= 0 ) {
    return ESP_FAIL;
  }

  buf[buf_len - 1] = '\0';
  // Trim the newline character
  size_t line_len = strnlen(buf.data(), buf_len - 1);
  if( buf[line_len - -1] == '\n' ) {
    buf[line_len - 1] = '\0';
  }

  if( strncmp(req->uri, "/api_key", 9) == 0 ) {
    ESP_LOGI(LOG_TAG, "Received API key: %s", buf.data());
    if( esp_err_t err = oai_set_api_key(buf.data()); err != ESP_OK ) {
      ESP_LOGE(LOG_TAG, "Failed to store the API key in the NVS - %s(%d)", esp_err_to_name(err), err);
      return err;
    }
    // Notify to the task.
    if( req->user_ctx != nullptr ) {
      xTaskNotifyGive(reinterpret_cast<TaskHandle_t>(req->user_ctx));
    }
  } else if( strncmp(req->uri, "/api_uri", 9) == 0 ) {
    ESP_LOGI(LOG_TAG, "Received API URI: %s", buf.data());
    if( esp_err_t err = oai_set_api_uri(buf.data()); err != ESP_OK ) {
      ESP_LOGE(LOG_TAG, "Failed to store the API URI in the NVS - %s(%d)", esp_err_to_name(err), err);
      return err;
    }
  } else {
    ESP_LOGW(LOG_TAG, "Unknown POST request: %s", buf.data());
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

static httpd_handle_t s_config_server = nullptr;

static esp_err_t  oai_config_httpd_start()
{
  if( s_config_server != nullptr ) {
    return ESP_OK;
  }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if( auto err = httpd_start(&s_config_server, &config); err != ESP_OK ) {
    return err;
  }

  const httpd_uri_t config_http_get_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = config_http_get_handler,
      .user_ctx  = nullptr
  };
  
  httpd_register_uri_handler(s_config_server, &config_http_get_uri);
  const char* get_uris[] = {"/index.html", "/api_key", "/api_uri"};
  for( const auto& uri : get_uris ) {
    httpd_uri_t uri_handler = {
      .uri       = uri,
      .method    = HTTP_GET,
      .handler   = config_http_get_handler,
      .user_ctx  = nullptr
    };
    httpd_register_uri_handler(s_config_server, &uri_handler);
  }
  const char* post_uris[] = {"/api_key", "/api_uri", "/reboot"};
  for( const auto& uri : post_uris ) {
    httpd_uri_t uri_handler = {
      .uri       = uri,
      .method    = HTTP_POST,
      .handler   = config_http_post_handler,
      .user_ctx  = xTaskGetCurrentTaskHandle(),
    };
    httpd_register_uri_handler(s_config_server, &uri_handler);
  }
  
  // Start mDNS
  if( auto err = mdns_init(); err != ESP_OK ) {
    ESP_LOGE(LOG_TAG, "Failed to initialize mDNS - %d", err);
    return err;
  }
  mdns_hostname_set("oai-res-example");
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  mdns_service_instance_name_set("_http", "_tcp", "OpenAI Realtime Embedded SDK Example");

  return ESP_OK;
}

static esp_err_t oai_config_httpd_stop()
{
  if( s_config_server == nullptr ) {
    return ESP_OK;
  }

  mdns_free();
  httpd_stop(s_config_server);
  s_config_server = nullptr;
  return ESP_OK;
}
#else // CONFIG_USE_WIFI_PROVISIONING_SOFTAP
/**
 * Get the API URI from the default value
 */
esp_err_t oai_get_api_uri(std::string& api_uri)
{
  api_uri = CONFIG_OPENAI_REALTIMEAPI;
  return ESP_OK;
}
#endif // CONFIG_USE_WIFI_PROVISIONING_SOFTAP

static bool g_wifi_connected = false;

static void oai_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  static int s_retry_num = 0;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 5) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(LOG_TAG, "retry to connect to the AP");
    }
    ESP_LOGI(LOG_TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(LOG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    g_wifi_connected = true;
  }

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  if( event_base == WIFI_PROV_EVENT ) {
    switch( event_id ) {
      case WIFI_PROV_START:
        ESP_LOGI(LOG_TAG, "Provisioning started");
        break;
      case WIFI_PROV_CRED_RECV: {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(LOG_TAG, "Received Wi-Fi credentials. SSID: %s, Password: %s",
                 (char *)wifi_sta_cfg->ssid, (char *)wifi_sta_cfg->password);
        break;
      }
      case WIFI_PROV_CRED_FAIL: {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGE(LOG_TAG, "Provisioning failed! Reason : %s",
                 (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi station DHCP client failed");
        break;
      }
      case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(LOG_TAG, "Provisioning successful");
        break;
      case WIFI_PROV_END:
        ESP_LOGI(LOG_TAG, "Provisioning ended");
        wifi_prov_mgr_deinit();
        break;
    }
  }
#endif // CONFIG_USE_WIFI_PROVISIONING_SOFTAP
}

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
static void wifi_prov_print_qr(const char *name, const char *username, const char *pop)
{
    assert(name);
    if( pop ) {
        assert(username);
    }

    constexpr const char* transport = "softap";
    constexpr const char* PROV_QR_VERSION = "v1";

    std::vector<char> payload;
    payload.resize(150);

    if (pop) {
        snprintf(payload.data(), payload.size(), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, username, pop, transport);
    } else {
        snprintf(payload.data(), payload.size(), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, transport);
    }
    ESP_LOGI(LOG_TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload.data());
    ESP_LOGI(LOG_TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload.data());
}
#endif

void oai_wifi(void) {
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &oai_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &oai_event_handler, NULL));
#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &oai_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &oai_event_handler, NULL));
#endif // CONFIG_USE_WIFI_PROVISIONING_SOFTAP

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  esp_netif_create_default_wifi_ap();
#endif // CONFIG_USE_WIFI_PROVISIONING_SOFTAP

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

#ifndef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  // Start WiFi in station mode
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(LOG_TAG, "Connecting to WiFi SSID: %s", CONFIG_WIFI_SSID);
  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  strncpy((char *)wifi_config.sta.ssid, (char *)CONFIG_WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, (char *)CONFIG_WIFI_PASSWORD,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_config(
      static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());
#else // CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  wifi_prov_mgr_config_t config = {
      .scheme = wifi_prov_scheme_softap,
      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
      .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
  };
  ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

  bool reset_provisioning = bsp_check_reset_provisioning();

  bool provisioned = false;
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

  if (!provisioned || reset_provisioning) {
    // Not provisioned, start provisioning via SoftAP
    ESP_LOGI(LOG_TAG, "Starting provisioning");
    const auto security = WIFI_PROV_SECURITY_2;
    const char* service_name = "OAI_RES_WIFI";
    const char* service_key = nullptr;
    wifi_prov_security2_params_t sec2_params = {};
    sec2_params.salt = sec2_salt;
    sec2_params.salt_len = sizeof(sec2_salt);
    sec2_params.verifier = sec2_verifier;
    sec2_params.verifier_len = sizeof(sec2_verifier);

    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, &sec2_params, service_name, service_key));

    wifi_prov_print_qr(service_name, EXAMPLE_PROV_SEC2_USERNAME, EXAMPLE_PROV_SEC2_PWD);

    wifi_prov_mgr_wait();
  } else {
    ESP_LOGI(LOG_TAG, "Already provisioned, starting WiFi");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
  }

  // block until we get an IP address
  while (!g_wifi_connected) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  bool has_api_key = false;
  if( auto err = oai_has_api_key(has_api_key); err != ESP_OK || !has_api_key || reset_provisioning ) {
    ESP_LOGW(LOG_TAG, "API key not set");
    // Strat the HTTP server to receive the API key
    ESP_ERROR_CHECK(oai_config_httpd_start());
    // Wait for the API key to be set
    ESP_LOGI(LOG_TAG, "Waiting for the API key to be set");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#ifdef CONFIG_DISABLE_CONFIGURATOR_AFTER_PROVISIONED
    // Stop the HTTP server
    oai_config_httpd_stop();
#endif
  } else {
    ESP_LOGI(LOG_TAG, "API key found.");
#ifndef CONFIG_DISABLE_CONFIGURATOR_AFTER_PROVISIONED
    // Strat the HTTP server.
    ESP_ERROR_CHECK(oai_config_httpd_start());
#endif
  }

#endif // CONFIG_USE_WIFI_PROVISIONING_SMARTCONFIG
}
