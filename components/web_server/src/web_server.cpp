#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "web_server.hpp"
#include "page_layout.hpp"
#include "cJSON.h"
#include <cstring>
#include <memory>

static constexpr char TAG[] = "web_server";

web_server::web_server(vld1 &sensor) noexcept
    : server_(nullptr),
      sensor_(sensor),
      ssid_("VLD1_AP"),
      password_("12345678"),
      ip_("192.168.4.1"),
      is_initialized_(false)
{
}

web_server::~web_server()
{
    deinit();
}

esp_err_t web_server::init_soft_ap()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition was truncated or version mismatch; erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, "ESP32_AP");
    wifi_config.ap.ssid_len = strlen("ESP32_AP");
    strcpy((char *)wifi_config.ap.password, "12345678");
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;

    if (strlen("12345678") == 0)
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started — SSID:%s password:%s", wifi_config.ap.ssid, wifi_config.ap.password);
    return ESP_OK;
}

void web_server::update_ip_address()
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info{};

    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        char ip_str[16];
        std::snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        ip_ = ip_str;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get IP info for AP interface");
        ip_ = "0.0.0.0";
    }
}

esp_err_t web_server::init()
{

    if (is_initialized_)
    {
        ESP_LOGW(TAG, "Web server already initialized; skipping reinit.");
        return ESP_OK;
    }

    esp_err_t ret = init_soft_ap();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SoftAP: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    ret = httpd_start(&server_, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    register_handlers();

    is_initialized_ = true;
    ESP_LOGI(TAG, "HTTP Server started successfully on port %d", config.server_port);

    return ESP_OK;
}

void web_server::deinit()
{
    if (server_)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    is_initialized_ = false;
}

void *web_server::get_server_from_req(httpd_req_t *req)
{
    return req->user_ctx;
}

void web_server::register_handlers()
{
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &root_uri);

    httpd_uri_t post_uri = {
        .uri = "/vld1_config",
        .method = HTTP_POST,
        .handler = handle_post_config,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &post_uri);
}

esp_err_t web_server::handle_root(httpd_req_t *req)
{
    auto *self = static_cast<web_server *>(req->user_ctx);
    vld1::radar_params_t config = self->sensor_.get_curr_radar_params();

    return self->serve_config_page(req, config);
}

esp_err_t web_server::serve_config_page(httpd_req_t *req, const vld1::radar_params_t &config)
{
    char buffer[1024];
    esp_err_t ret;

    httpd_resp_set_type(req, "text/html");

    ret = httpd_resp_sendstr_chunk(req, g_vld1_doc);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_style_start);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_style);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_style2);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_style3);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_style_end);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_header, "", "", "", "", "", "", "", "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_vld1_container);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_firmware, config.firmware_version);
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_unique_id, config.unique_id);
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_distance_range,
             "",
             config.distance_range == vld1::vld1_distance_range_t::range_20 ? "selected" : "",
             config.distance_range == vld1::vld1_distance_range_t::range_50 ? "selected" : "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_threshold, config.threshold_offset, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_min_range, config.min_range_filter, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_max_range, config.max_range_filter, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_avg_count, config.distance_avg_count, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_target_filter,
             "",
             config.target_filter == vld1::target_filter_t::strongest ? "selected" : "",
             config.target_filter == vld1::target_filter_t::nearest ? "selected" : "",
             config.target_filter == vld1::target_filter_t::farthest ? "selected" : "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_precision,
             "",
             config.distance_precision == vld1::precision_mode_t::low ? "selected" : "",
             config.distance_precision == vld1::precision_mode_t::high ? "selected" : "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_tx_power, config.tx_power, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_chirp_count, config.chirp_integration_count, "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    snprintf(buffer, sizeof(buffer), g_vld1_short_range,
             "",
             config.short_range_distance_filter == vld1::short_range_distance_t::disable ? "selected" : "",
             config.short_range_distance_filter == vld1::short_range_distance_t::enable ? "selected" : "");
    ret = httpd_resp_sendstr_chunk(req, buffer);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_vld1_script);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_vld1_form_end);
    if (ret != ESP_OK)
        return ret;

    ret = httpd_resp_sendstr_chunk(req, g_footer);
    if (ret != ESP_OK)
        return ret;

    return httpd_resp_sendstr_chunk(req, NULL);
}

esp_err_t web_server::handle_post_config(httpd_req_t *req)
{
    auto *self = static_cast<web_server *>(req->user_ctx);
    if (!self)
    {
        ESP_LOGE(TAG, "User context is NULL");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
        return ESP_FAIL;
    }

    size_t remaining = req->content_len;
    std::unique_ptr<char[]> buf(new char[remaining + 1]);
    char *p = buf.get();
    std::memset(p, 0, remaining);

    while (remaining > 0)
    {
        int ret = httpd_req_recv(req, p, remaining);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            ESP_LOGE(TAG, "Failed to read request body");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
            return ESP_FAIL;
        }
        p += ret;
        remaining -= ret;
    }

    buf[req->content_len] = '\0';

    ESP_LOGI(TAG, "Received POST data: %s", buf.get());

    cJSON *root = cJSON_Parse(buf.get());
    if (!root)
    {
        ESP_LOGE(TAG, "Invalid JSON payload");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    auto get_str = [&](const char *key) -> std::string
    {
        const cJSON *item = cJSON_GetObjectItem(root, key);
        return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : "";
    };

    auto get_num = [&](const char *key, double default_val = 0.0) -> double
    {
        const cJSON *item = cJSON_GetObjectItem(root, key);
        return cJSON_IsNumber(item) ? item->valuedouble : default_val;
    };

    vld1::radar_params_t params{};

    std::string fw_ver = get_str("firmware_version");
    std::string uid = get_str("unique_id");
    std::strncpy(params.firmware_version, fw_ver.c_str(), sizeof(params.firmware_version) - 1);
    params.firmware_version[sizeof(params.firmware_version) - 1] = '\0';
    std::strncpy(params.unique_id, uid.c_str(), sizeof(params.unique_id) - 1);
    params.unique_id[sizeof(params.unique_id) - 1] = '\0';

    params.distance_range = static_cast<vld1::vld1_distance_range_t>(static_cast<uint8_t>(get_num("distance_range")));
    params.threshold_offset = static_cast<uint8_t>(get_num("threshold_offset"));
    params.min_range_filter = static_cast<uint16_t>(get_num("min_range_filter"));
    params.max_range_filter = static_cast<uint16_t>(get_num("max_range_filter"));
    params.distance_avg_count = static_cast<uint8_t>(get_num("distance_avg_count"));
    params.target_filter = static_cast<vld1::target_filter_t>(static_cast<uint8_t>(get_num("target_filter")));
    params.distance_precision = static_cast<vld1::precision_mode_t>(static_cast<uint8_t>(get_num("distance_precision")));
    params.tx_power = static_cast<uint8_t>(get_num("tx_power"));
    params.chirp_integration_count = static_cast<uint8_t>(get_num("chirp_integration_count"));
    params.short_range_distance_filter = static_cast<vld1::short_range_distance_t>(static_cast<uint8_t>(get_num("short_range_distance_filter")));

    ESP_LOGI(TAG, "Parsed parameters:");
    ESP_LOGI(TAG, "  Distance Range: %d", static_cast<int>(params.distance_range));
    ESP_LOGI(TAG, "  Threshold Offset: %u", params.threshold_offset);
    ESP_LOGI(TAG, "  Min Range Filter: %u", params.min_range_filter);
    ESP_LOGI(TAG, "  Max Range Filter: %u", params.max_range_filter);
    ESP_LOGI(TAG, "  Distance Avg Count: %u", params.distance_avg_count);
    ESP_LOGI(TAG, "  Target Filter: %d", static_cast<int>(params.target_filter));
    ESP_LOGI(TAG, "  Distance Precision: %d", static_cast<int>(params.distance_precision));
    ESP_LOGI(TAG, "  TX Power: %u", params.tx_power);
    ESP_LOGI(TAG, "  Chirp Integration Count: %u", params.chirp_integration_count);
    ESP_LOGI(TAG, "  Short Range Filter: %d", static_cast<int>(params.short_range_distance_filter));

    cJSON_Delete(root);

    esp_err_t sensor_ret = self->sensor_.set_radar_parameters(params);
    self->sensor_.get_parameters();

    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        ESP_LOGE(TAG, "Failed to create JSON response");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create response");
        return ESP_FAIL;
    }

    if (sensor_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Radar parameters successfully updated on sensor");
        cJSON_AddStringToObject(response, "status", "success");
        cJSON_AddStringToObject(response, "message", "Radar parameters updated successfully");

        vld1::radar_params_t readback_params = self->sensor_.get_curr_radar_params();
        cJSON *params_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(params_obj, "distance_range", static_cast<int>(readback_params.distance_range));
        cJSON_AddNumberToObject(params_obj, "threshold_offset", readback_params.threshold_offset);
        cJSON_AddNumberToObject(params_obj, "min_range_filter", readback_params.min_range_filter);
        cJSON_AddNumberToObject(params_obj, "max_range_filter", readback_params.max_range_filter);
        cJSON_AddNumberToObject(params_obj, "distance_avg_count", readback_params.distance_avg_count);
        cJSON_AddNumberToObject(params_obj, "target_filter", static_cast<int>(readback_params.target_filter));
        cJSON_AddNumberToObject(params_obj, "distance_precision", static_cast<int>(readback_params.distance_precision));
        cJSON_AddNumberToObject(params_obj, "tx_power", readback_params.tx_power);
        cJSON_AddNumberToObject(params_obj, "chirp_integration_count", readback_params.chirp_integration_count);
        cJSON_AddNumberToObject(params_obj, "short_range_distance_filter", static_cast<int>(readback_params.short_range_distance_filter));
        cJSON_AddItemToObject(response, "current_parameters", params_obj);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to update radar parameters on sensor: %s", esp_err_to_name(sensor_ret));
        cJSON_AddStringToObject(response, "status", "error");

        switch (sensor_ret)
        {
        case ESP_ERR_TIMEOUT:
            cJSON_AddStringToObject(response, "message", "Sensor communication timeout");
            break;
        case ESP_ERR_INVALID_ARG:
            cJSON_AddStringToObject(response, "message", "Invalid parameter values");
            break;
        case ESP_ERR_INVALID_STATE:
            cJSON_AddStringToObject(response, "message", "Sensor not ready");
            break;
        default:
            cJSON_AddStringToObject(response, "message", "Failed to update radar parameters");
            break;
        }
        cJSON_AddStringToObject(response, "error_code", esp_err_to_name(sensor_ret));
    }

    char *response_str = cJSON_PrintUnformatted(response);
    if (!response_str)
    {
        cJSON_Delete(response);
        ESP_LOGE(TAG, "Failed to serialize JSON response");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create response");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t http_ret = httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);

    cJSON_free(response_str);
    cJSON_Delete(response);

    return http_ret;
}