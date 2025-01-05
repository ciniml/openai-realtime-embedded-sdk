#include <esp_http_client.h>
#include <esp_log.h>
#include <string.h>
#include <string>
#include <vector>

#include "main.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

esp_err_t oai_http_event_handler(esp_http_client_event_t *evt) {
  static int output_len;
  switch (evt->event_id) {
    case HTTP_EVENT_REDIRECT:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_REDIRECT");
      esp_http_client_set_header(evt->client, "From", "user@example.com");
      esp_http_client_set_header(evt->client, "Accept", "text/html");
      esp_http_client_set_redirection(evt->client);
      break;
    case HTTP_EVENT_ERROR:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
               evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA: {
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (esp_http_client_is_chunked_response(evt->client)) {
        ESP_LOGE(LOG_TAG, "Chunked HTTP response not supported");
#ifndef LINUX_BUILD
        esp_restart();
#endif
      }

      if (output_len == 0 && evt->user_data) {
        memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
      }

      // If user_data buffer is configured, copy the response into the buffer
      int copy_len = 0;
      if (evt->user_data) {
        // The last byte in evt->user_data is kept for the NULL character in
        // case of out-of-bound access.
        copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
        if (copy_len) {
          memcpy(((char *)evt->user_data) + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;

      break;
    }
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_FINISH");
      output_len = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(LOG_TAG, "HTTP_EVENT_DISCONNECTED");
      output_len = 0;
      break;
  }
  return ESP_OK;
}

void oai_http_request(char *offer, char *answer) {
  esp_http_client_config_t config;
  memset(&config, 0, sizeof(esp_http_client_config_t));

  extern esp_err_t oai_get_api_uri(std::string& api_uri);
  std::string api_uri;
  if( auto err = oai_get_api_uri(api_uri); err != ESP_OK ) {
    api_uri = CONFIG_OPENAI_REALTIMEAPI;
  }
  config.url = api_uri.c_str();
  ESP_LOGI(LOG_TAG, "Using API URI: %s", config.url);
  
  config.event_handler = oai_http_event_handler;
  config.user_data = answer;

#ifdef CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  extern esp_err_t oai_get_api_key(std::vector<char>& api_key);
  std::vector<char> api_key;
  if( auto err = oai_get_api_key(api_key); err != ESP_OK ) {
    ESP_LOGE(LOG_TAG, "API key not set");
  } else {
    ESP_LOGI(LOG_TAG, "Using API key: %s", api_key.data());
    snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Bearer %s", api_key.data());
  }
#else // CONFIG_USE_WIFI_PROVISIONING_SOFTAP
  snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Bearer %s", CONFIG_OPENAI_API_KEY);
#endif

  if( esp_http_client_handle_t client = esp_http_client_init(&config); client == nullptr ) {
    ESP_LOGE(LOG_TAG, "Failed to initialize HTTP client");
  } else {
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/sdp");
    esp_http_client_set_header(client, "Authorization", answer);
    esp_http_client_set_post_field(client, offer, strlen(offer));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK || esp_http_client_get_status_code(client) != 201) {
      ESP_LOGE(LOG_TAG, "Error perform http request %s", esp_err_to_name(err));
  #if !defined(LINUX_BUILD) && defined(CONFIG_DISABLE_CONFIGURATOR_AFTER_PROVISIONED)
      esp_restart();
  #endif
    }

    esp_http_client_cleanup(client);
  }
}
