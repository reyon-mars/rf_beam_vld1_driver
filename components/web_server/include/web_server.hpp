#ifndef VLD1_CONFIG_SERVER_H
#define VLD1_CONFIG_SERVER_H

#include <esp_err.h>
#include <esp_http_server.h>
#include <string>

class VLD1ConfigServer
{
public:
    VLD1ConfigServer();
    ~VLD1ConfigServer();

    esp_err_t start(); // Start Wi-Fi AP + WebServer
    void stop();       // Stop WebServer

private:
    // Internal functions
    esp_err_t initWiFiAP();
    httpd_handle_t startWebServer();
    void registerHandlers();

    // Handlers
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleSubmit(httpd_req_t *req);

    // Static HTML form
    static const char *htmlForm;

    httpd_handle_t server;
};

#endif // VLD1_CONFIG_SERVER_H
