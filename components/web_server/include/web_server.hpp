#pragma once
#include "esp_err.h"
#include <esp_err.h>
#include <esp_http_server.h>
#include "vld1.hpp"
#include <string>

class web_server
{
public:
    explicit web_server(vld1 &sensor);
    ~web_server();

    esp_err_t init(); // Initialize SoftAP + HTTP server
    void deinit();    // Stop server + SoftAP

    std::string getSSID() const { return ssid_; }
    std::string getIP() const { return ip_; }

private:
    // ---------------- Server & sensor ----------------
    httpd_handle_t server_;
    vld1 &sensor_;

    // ---------------- SoftAP state ----------------
    std::string ssid_;
    std::string password_;
    std::string ip_;
    bool is_initialized_;

    // ---------------- Handlers ----------------
    static esp_err_t handleGetConfig(httpd_req_t *req);
    static esp_err_t handlePostConfig(httpd_req_t *req);

    void registerHandlers();

    // ---------------- Utilities ----------------
    void applyConfigFromJson(const char *json_data, size_t len);
    std::string getConfigAsJson();
    static void *getServerFromRequest(httpd_req_t *req);

    // ---------------- WiFi Helpers ----------------
    esp_err_t initSoftAP();
    void updateIPAddress();
};
