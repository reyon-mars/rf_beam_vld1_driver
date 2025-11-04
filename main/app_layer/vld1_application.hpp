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

struct app_context
{
    rs485 *rs485_slave;
    vld1 *vld1_sensor;
    batch_averager *averager;
    led *main_led;
};

class application
{
public:
    application(rs485 &rs_slave,
                vld1 &vld1_sensor,
                batch_averager &avg,
                led &led_main) noexcept
        : ctx_{&rs_slave, &vld1_sensor, &avg, &led_main}
    {
    }

    void start_read_and_forward();

private:
    static void get_pdat_and_forward(void *arg);
    app_context ctx_;
};
