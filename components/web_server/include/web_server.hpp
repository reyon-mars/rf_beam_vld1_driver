#pragma once
#include <string>
#include "esp_err.h"
#include "esp_http_server.h"
#include "vld1.hpp"

class web_server {
public:
    explicit web_server(vld1 &sensor) noexcept;
    ~web_server();

    esp_err_t init();
    void deinit();

private:
    // Wi-Fi
    esp_err_t initSoftAP();
    void updateIPAddress();

    // JSON config
    esp_err_t applyConfigFromJson(const char *json_data, size_t len) noexcept;
    std::string getConfigAsJson();

    // HTTP handlers
    static void* getServerFromRequest(httpd_req_t *req);
    static esp_err_t handleGetConfig(httpd_req_t *req);
    static esp_err_t handlePostConfig(httpd_req_t *req);
    void registerHandlers();

private:
    httpd_handle_t server_;
    vld1 &sensor_;
    std::string ssid_;
    std::string password_;
    std::string ip_;
    bool is_initialized_;
};
