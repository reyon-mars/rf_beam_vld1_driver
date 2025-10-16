#pragma once

#include "esp_http_server.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vld1.hpp"

#include <string>
#include <memory>

class web_server
{
public:
    explicit web_server(vld1& sensor, TaskHandle_t gnfd_task = nullptr) noexcept;
    ~web_server();

    esp_err_t init();
    void deinit();

private:
    // Core handles
    httpd_handle_t server_;
    vld1& sensor_;
    TaskHandle_t gnfd_task_;

    // Wi-Fi credentials and IP
    std::string ssid_;
    std::string password_;
    std::string ip_;
    bool is_initialized_;

    // Internal setup
    esp_err_t initSoftAP();
    void updateIPAddress();
    void registerHandlers();

    // HTTP handlers
    static esp_err_t handleRoot(httpd_req_t* req);
    static esp_err_t handlePostConfig(httpd_req_t* req);

    // Context accessor
    static void* getServerFromRequest(httpd_req_t* req);
};
