#pragma once
#include "uart.hpp"
#include "mbcontroller.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <cstring>
#include <algorithm>

class rs485
{
public:
    rs485(uart &uart_no, gpio_num_t de_re_pin) noexcept;
    ~rs485() noexcept;

    rs485(const rs485 &) = delete;
    rs485 &operator=(const rs485 &) = delete;

    esp_err_t init(uint8_t slave_addr, mb_param_type_t reg_type, size_t input_reg_count) noexcept;

    esp_err_t write(const uint16_t *data, size_t count) noexcept;

private:
    esp_err_t configure_pins() noexcept;
    void free_registers() noexcept;

    uart &uart_;
    gpio_num_t de_re_pin_;
    void *mbc_slave_handle_;
    uint8_t slave_address_;
    uint16_t *input_registers_;
    size_t input_reg_size_;
};
