#include "web_server.hpp"
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "cJSON.h"
#include <cstring>

static const char *TAG = "WebServer";

// ---------------- Constructor / Destructor ----------------
web_server::web_server(vld1 &sensor)
    : server_(nullptr), sensor_(sensor), ssid_("VLD1_AP"), password_("12345678"),
      ip_(""), is_initialized_(false) {}

web_server::~web_server() { deinit(); }

// ---------------- Init SoftAP + HTTP Server ----------------
esp_err_t web_server::init()
{
    esp_err_t ret;

    // NVS init (needed for Wi-Fi)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Netif & event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // SoftAP init
    if ((ret = initSoftAP()) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SoftAP");
        return ret;
    }

    // HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server_, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    registerHandlers();
    is_initialized_ = true;
    updateIPAddress();

    ESP_LOGI(TAG, "Web server initialized: SSID=%s, IP=%s", ssid_.c_str(), ip_.c_str());
    return ESP_OK;
}

void web_server::deinit()
{
    if (server_)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
    if (is_initialized_)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_deinit());
        is_initialized_ = false;
    }
}

// ---------------- SoftAP ----------------
esp_err_t web_server::initSoftAP()
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {};
    strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), ssid_.c_str(), sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = ssid_.length();
    strncpy(reinterpret_cast<char *>(ap_config.ap.password), password_.c_str(), sizeof(ap_config.ap.password));
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (password_.empty())
        ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

void web_server::updateIPAddress()
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
        ip_ = std::string(buf);
    }
}

// ---------------- JSON Utilities ----------------
void web_server::applyConfigFromJson(const char *json_data, size_t len)
{
    if (!json_data)
        return;

    cJSON *root = cJSON_ParseWithLength(json_data, len);
    if (!root)
    {
        ESP_LOGW(TAG, "Invalid JSON data");
        return;
    }

    try
    {
        cJSON *item;

        if ((item = cJSON_GetObjectItem(root, "distance_range")))
            sensor_.set_distance_range(static_cast<vld1::vld1_distance_range_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "threshold_offset")))
            sensor_.set_threshold_offset(static_cast<uint8_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "min_range")))
            sensor_.set_min_range_filter(static_cast<uint16_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "max_range")))
            sensor_.set_max_range_filter(static_cast<uint16_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "target_filter")))
            sensor_.set_target_filter(static_cast<vld1::target_filter_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "precision")))
            sensor_.set_precision_mode(static_cast<vld1::precision_mode_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "tx_power")))
            sensor_.set_tx_power(static_cast<uint8_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "chirp_count")))
            sensor_.set_chirp_integration_count(static_cast<uint8_t>(item->valueint));

        if ((item = cJSON_GetObjectItem(root, "short_filter")))
            sensor_.set_short_range_distance_filter(
                static_cast<vld1::short_range_distance_t>(item->valueint));

        ESP_LOGI(TAG, "JSON configuration applied");
    }
    catch (...)
    {
        ESP_LOGW(TAG, "Error applying JSON configuration");
    }

    cJSON_Delete(root);
}

std::string web_server::getConfigAsJson()
{
    vld1::radar_params_t p;
    sensor_.get_parameters(p);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "distance_range", static_cast<int>(p.distance_range));
    cJSON_AddNumberToObject(root, "threshold_offset", p.threshold_offset);
    cJSON_AddNumberToObject(root, "min_range", p.min_range_filter);
    cJSON_AddNumberToObject(root, "max_range", p.max_range_filter);
    cJSON_AddNumberToObject(root, "target_filter", static_cast<int>(p.target_filter));
    cJSON_AddNumberToObject(root, "precision", static_cast<int>(p.distance_precision));
    cJSON_AddNumberToObject(root, "tx_power", p.tx_power);
    cJSON_AddNumberToObject(root, "chirp_count", p.chirp_integration_count);
    cJSON_AddNumberToObject(root, "short_filter", static_cast<int>(p.short_range_distance_filter));

    char *json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return result;
}

// ---------------- Request Handlers ----------------
void *web_server::getServerFromRequest(httpd_req_t *req) { return req->user_ctx; }

esp_err_t web_server::handleGetConfig(httpd_req_t *req)
{
    web_server *srv = static_cast<web_server *>(getServerFromRequest(req));
    std::string json = srv->getConfigAsJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json.c_str());
    return ESP_OK;
}

esp_err_t web_server::handlePostConfig(httpd_req_t *req)
{
    web_server *srv = static_cast<web_server *>(getServerFromRequest(req));

    std::string body;
    char buf[128];
    int remaining = req->content_len;

    while (remaining > 0)
    {
        int rlen = httpd_req_recv(req, buf, std::min(remaining, (int)sizeof(buf)));
        if (rlen <= 0)
            break;
        body.append(buf, rlen);
        remaining -= rlen;
    }

    ESP_LOGI(TAG, "Received JSON: %s", body.c_str());
    srv->applyConfigFromJson(body.c_str(), body.size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// ---------------- Register Handlers ----------------
void web_server::registerHandlers()
{
    httpd_uri_t get_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = handleGetConfig,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &get_uri);

    httpd_uri_t post_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = handlePostConfig,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &post_uri);
}
