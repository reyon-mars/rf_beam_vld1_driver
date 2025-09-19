#include "web_server.hpp"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <cstring>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "VLD1ConfigServer";

#define WIFI_SSID "vld1_config"
#define WIFI_PASS "" // Open network
#define MAX_STA_CONN 5

// ---------------- HTML FORM ----------------
const char *VLD1ConfigServer::htmlForm =
    "<!DOCTYPE html>"
    "<html><head><title>VLD1 Config</title></head><body>"
    "<h2>VLD1 Configuration</h2>"
    "<form action=\"/submit\" method=\"post\">"

    "Firmware version:<br>"
    "<input type=\"text\" name=\"fw_version\" value=\"V-LD1_APP-RFB-YYXX\" maxlength=\"19\" required><br><br>"

    "Unique ID:<br>"
    "<input type=\"text\" name=\"unique_id\" value=\"L1234n12345\" maxlength=\"12\" readonly><br><br>"

    "Distance range:<br>"
    "<select name=\"distance_range\">"
    "<option value=\"0\">20m</option>"
    "<option value=\"1\">50m</option>"
    "</select><br><br>"

    "Threshold offset [dB]:<br>"
    "<input type=\"number\" name=\"threshold_offset\" min=\"20\" max=\"90\" value=\"40\"><br><br>"

    "Minimum range filter [bin]:<br>"
    "<input type=\"number\" name=\"min_range\" min=\"1\" max=\"510\" value=\"5\"><br><br>"

    "Maximum range filter [bin]:<br>"
    "<input type=\"number\" name=\"max_range\" min=\"2\" max=\"511\" value=\"460\"><br><br>"

    "Distance average count:<br>"
    "<input type=\"number\" name=\"avg_count\" min=\"1\" max=\"255\" value=\"5\"><br><br>"

    "Target filter:<br>"
    "<select name=\"target_filter\">"
    "<option value=\"0\">Strongest first</option>"
    "<option value=\"1\" selected>Nearest first</option>"
    "<option value=\"2\">Farthest first</option>"
    "</select><br><br>"

    "Distance precision:<br>"
    "<select name=\"precision\">"
    "<option value=\"0\">Low</option>"
    "<option value=\"1\" selected>High</option>"
    "</select><br><br>"

    "TX Power:<br>"
    "<input type=\"number\" name=\"tx_power\" min=\"0\" max=\"31\" value=\"31\"><br><br>"

    "Chirp integration count:<br>"
    "<input type=\"number\" name=\"chirp_count\" min=\"1\" max=\"100\" value=\"1\"><br><br>"

    "Short range distance filter:<br>"
    "<select name=\"short_filter\">"
    "<option value=\"0\" selected>Disabled</option>"
    "<option value=\"1\">Enabled</option>"
    "</select><br><br>"

    "<input type=\"submit\" value=\"Save\">"
    "</form></body></html>";

// ---------------- CLASS IMPLEMENTATION ----------------
VLD1ConfigServer::VLD1ConfigServer() : server(nullptr) {}

VLD1ConfigServer::~VLD1ConfigServer()
{
    stop();
}

esp_err_t VLD1ConfigServer::initWiFiAP()
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, WIFI_SSID);
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    wifi_config.ap.max_connection = MAX_STA_CONN;
    wifi_config.ap.channel = 1;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    if (strlen(WIFI_PASS) > 0)
    {
        strcpy((char *)wifi_config.ap.password, WIFI_PASS);
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started.");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password: %s", (strlen(WIFI_PASS) > 0) ? WIFI_PASS : "<OPEN>");

    // Fetch IP address assigned to AP
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "Config page available at: http://%s/", ip_str);
        }
    }

    return ESP_OK;
}

httpd_handle_t VLD1ConfigServer::startWebServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        registerHandlers();
        ESP_LOGI(TAG, "WebServer started on port %d", config.server_port);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start WebServer");
        server = nullptr;
    }
    return server;
}

void VLD1ConfigServer::registerHandlers()
{
    static const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handleRoot,
        .user_ctx = nullptr};
    static const httpd_uri_t submit = {
        .uri = "/submit",
        .method = HTTP_POST,
        .handler = handleSubmit,
        .user_ctx = nullptr};

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &submit);
}

esp_err_t VLD1ConfigServer::handleRoot(httpd_req_t *req)
{
    httpd_resp_send(req, htmlForm, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t VLD1ConfigServer::handleSubmit(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (ret <= 0)
    {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Received config: %s", buf);

    httpd_resp_sendstr(req,
                       "<html><body><h3>Configuration Saved!</h3>"
                       "<a href=\"/\">Back</a></body></html>");
    return ESP_OK;
}

esp_err_t VLD1ConfigServer::start()
{
    ESP_ERROR_CHECK(initWiFiAP());
    if (!startWebServer())
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void VLD1ConfigServer::stop()
{
    if (server)
    {
        httpd_stop(server);
        server = nullptr;
        ESP_LOGI(TAG, "WebServer stopped");
    }
}
