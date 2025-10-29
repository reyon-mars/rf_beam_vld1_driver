#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "vld1.hpp"
#include <string>

class web_server
{
public:
    web_server(vld1 &sensor, SemaphoreHandle_t uart_mutex) noexcept;
    ~web_server();

    esp_err_t init();
    void deinit();

    const std::string &getSSID() const { return ssid_; }
    const std::string &getPassword() const { return password_; }
    const std::string &getIP() const { return ip_; }

private:
    esp_err_t initSoftAP();
    void updateIPAddress();
    void registerHandlers();

    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handlePostConfig(httpd_req_t *req);
    static void *getServerFromRequest(httpd_req_t *req);

    httpd_handle_t server_;
    vld1 &sensor_;
    SemaphoreHandle_t uart_mutex_;

    std::string ssid_;
    std::string password_;
    std::string ip_;
    bool is_initialized_;
};
