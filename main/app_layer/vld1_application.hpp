#pragma once

#include "uart.hpp"
#include "rs485_slave.hpp"
#include "vld1.hpp"
#include "averager.hpp"
#include "led.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <cinttypes>

struct AppContext
{
    uart* vld1_uart;
    uart* rs485_uart;
    rs485* rs485_slave;
    vld1* vld1_sensor;
    batch_averager* averager;
    led* main_led;
    vld1::radar_params_t& radar_params;
};

class Application
{
public:
    Application(uart& vld1_sensor,
                uart& rs,
                rs485& rs_slave,
                vld1& v,
                batch_averager& avg,
                led& led_main,
                vld1::radar_params_t& radar_params)
        : ctx_{&vld1_sensor, &rs, &rs_slave, &v, &avg, &led_main, radar_params} {}

    void start();
    void start_gnfd_task();

    // Called externally (e.g., from web_server.cpp) before updating radar parameters
    void suspend_gnfd_task();
    void resume_gnfd_task();
    static inline TaskHandle_t gnfd_task_handle_ = nullptr;  // Handle to control GNFD task

private:
    static void gnfd_task(void* arg);
    static void vld1_read_task(void* arg);

    AppContext ctx_;
};
