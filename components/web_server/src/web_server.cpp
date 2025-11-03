#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "web_server.hpp"
#include "cJSON.h"
#include <cstring>
#include <memory>

static const char *TAG = "web_server";

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

esp_err_t web_server::initSoftAP()
{
    static const char *TAG = "web_server";

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

void web_server::updateIPAddress()
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
    static const char *TAG = "web_server";

    if (is_initialized_)
    {
        ESP_LOGW(TAG, "Web server already initialized; skipping reinit.");
        return ESP_OK;
    }

    esp_err_t ret = initSoftAP();
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

    registerHandlers();

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

void *web_server::getServerFromRequest(httpd_req_t *req)
{
    return req->user_ctx;
}

esp_err_t web_server::handleRoot(httpd_req_t *req)
{
    const char html[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>VLD1 Radar Configuration</title>
<style>
body { font-family: Arial; background: #111; color: #eee; padding: 2em; }
form { background: #222; padding: 1em; border-radius: 8px; width: 400px; }
label { display: block; margin-top: 10px; }
input, select { width: 100%; padding: 6px; margin-top: 5px; border-radius: 4px; border: none; }
button { margin-top: 15px; padding: 8px 12px; background: #007bff; color: white; border: none; border-radius: 4px; }
button:hover { background: #0056b3; cursor: pointer; }
</style>
</head>
<body>
<h2>VLD1 Radar Configuration</h2>
<form id="configForm">
  <label>Firmware Version</label>
  <input type="text" name="firmware_version" value="V-LD1_APP-RFB-YYX">
  <label>Unique ID</label>
  <input type="text" name="unique_id" value="L1234n12345">
  <label>Distance Range</label>
  <select name="distance_range"><option value="0">20m</option><option value="1">50m</option></select>
  <label>Threshold Offset (dB)</label>
  <input type="number" name="threshold_offset" value="40">
  <label>Min Range Filter</label>
  <input type="number" name="min_range_filter" value="5">
  <label>Max Range Filter</label>
  <input type="number" name="max_range_filter" value="460">
  <label>Distance Avg Count</label>
  <input type="number" name="distance_avg_count" value="5">
  <label>Target Filter</label>
  <select name="target_filter">
    <option value="0">Strongest</option><option value="1">Nearest</option><option value="2">Farthest</option>
  </select>
  <label>Distance Precision</label>
  <select name="distance_precision"><option value="0">Low</option><option value="1" selected>High</option></select>
  <label>TX Power</label>
  <input type="number" name="tx_power" value="31">
  <label>Chirp Integration Count</label>
  <input type="number" name="chirp_integration_count" value="1">
  <label>Short Range Filter</label>
  <select name="short_range_distance_filter"><option value="0">Disabled</option><option value="1">Enabled</option></select>
  <button type="button" onclick="submitForm()">Submit</button>
</form>
<script>
function submitForm() {
    const form = document.getElementById("configForm");
    const data = {};
    for (let i = 0; i < form.elements.length; i++) {
        const e = form.elements[i];
        if (e.name) data[e.name] = isNaN(e.value) ? e.value : Number(e.value);
    }
    fetch("/config", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(data)
    })
    .then(r => alert(r.ok ? "Configuration updated!" : "Update failed!"));
}
</script>
</body>
</html>
)rawliteral";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_server::handlePostConfig(httpd_req_t *req)
{
    auto *self = static_cast<web_server *>(req->user_ctx);
    if (!self)
    {
        ESP_LOGE(TAG, "User context is NULL");
        return ESP_FAIL;
    }

    // ESP_LOGI(TAG, "handlePostConfig() initiated");

    const size_t buf_len = req->content_len + 1;
    std::unique_ptr<char[]> buf(new char[buf_len]);
    memset(buf.get(), 0, buf_len);

    int ret = httpd_req_recv(req, buf.get(), req->content_len);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Failed to read request body");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
        return ESP_FAIL;
    }

    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf.get());
    if (!root)
    {
        ESP_LOGE(TAG, "Invalid JSON payload");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    vld1::radar_params_t params{};
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

    std::string fw_ver = get_str("firmware_version");
    std::string uid = get_str("unique_id");
    std::strncpy(params.firmware_version, fw_ver.c_str(), sizeof(params.firmware_version) - 1);
    params.firmware_version[sizeof(params.firmware_version) - 1] = '\0';
    std::strncpy(params.unique_id, uid.c_str(), sizeof(params.unique_id) - 1);
    params.unique_id[sizeof(params.unique_id) - 1] = '\0';

    params.distance_range = static_cast<vld1::vld1_distance_range_t>(get_num("distance_range"));
    params.threshold_offset = static_cast<uint8_t>(get_num("threshold_offset"));
    params.min_range_filter = static_cast<uint16_t>(get_num("min_range_filter"));
    params.max_range_filter = static_cast<uint16_t>(get_num("max_range_filter"));
    params.distance_avg_count = static_cast<uint8_t>(get_num("distance_avg_count"));
    params.target_filter = static_cast<vld1::target_filter_t>(get_num("target_filter"));
    params.distance_precision = static_cast<vld1::precision_mode_t>(get_num("distance_precision"));
    params.tx_power = static_cast<uint8_t>(get_num("tx_power"));
    params.chirp_integration_count = static_cast<uint8_t>(get_num("chirp_integration_count"));
    params.short_range_distance_filter = static_cast<vld1::short_range_distance_t>(get_num("short_range_distance_filter"));

    self->sensor_.flush_buffer();
    ESP_LOGI(TAG, "Mutex acquired, updating radar parameters...");

    self->sensor_.set_radar_parameters(params);

    ESP_LOGI(TAG, "Mutex released after parameter update");

    cJSON_Delete(root);

    const char resp[] = "{\"status\":\"ok\",\"message\":\"Radar parameters updated\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

void web_server::registerHandlers()
{
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handleRoot,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &root_uri);

    httpd_uri_t post_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = handlePostConfig,
        .user_ctx = this};
    httpd_register_uri_handler(server_, &post_uri);
}
