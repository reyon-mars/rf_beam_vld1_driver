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

struct AppContext
{
    uart *vld1_uart;
    uart *rs485_uart;
    rs485 *rs485_slave;
    vld1 *vld1_sensor;
    batch_averager *averager;
    led *main_led;
};

class Application
{
public:
    Application(uart &vld1_sensor, uart &rs, rs485 &rs_slave, vld1 &v, batch_averager &avg, led &led_main)
        : ctx_{&vld1_sensor, &rs, &rs_slave, &v, &avg, &led_main} {}

    void start();

private:
    static void uart_read_task(void *arg);
    AppContext ctx_;
};
