#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "vld1.hpp"
#include <string>

class web_server
{
public:
    web_server(vld1 &sensor) noexcept;
    ~web_server();

    esp_err_t init();
    void deinit();

    const std::string &get_ssid() const { return ssid_; }
    const std::string &get_password() const { return password_; }
    const std::string &get_ip() const { return ip_; }

private:
    esp_err_t init_soft_ap();
    void update_ip_address();
    void register_handlers();

    esp_err_t serve_config_page(httpd_req_t *req, const vld1::radar_params_t &config);

    static esp_err_t handle_root(httpd_req_t *req);
    static esp_err_t handle_post_config(httpd_req_t *req);
    static void *get_server_from_req(httpd_req_t *req);

    httpd_handle_t server_;
    vld1 &sensor_;
    std::string ssid_;
    std::string password_;
    std::string ip_;
    bool is_initialized_;
};
