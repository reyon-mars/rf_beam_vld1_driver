#pragma once
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include <cstddef>
#include <cstdint>

class uart
{
public:
    uart(uart_port_t port, int tx_pin, int rx_pin, int baud_rate = 115200, size_t rx_buf_size = 512) noexcept;
    ~uart() noexcept;

    esp_err_t init(uart_word_length_t data_bits = UART_DATA_8_BITS,
                   uart_parity_t parity = UART_PARITY_DISABLE,
                   uart_stop_bits_t stop_bits = UART_STOP_BITS_1) noexcept;

    int read(uint8_t *dst, size_t max_len, TickType_t ticks_to_wait) noexcept;
    esp_err_t read_exact(uint8_t *dst, size_t max_len, TickType_t ticks_to_wait = 10 ) noexcept;

    int write(const uint8_t *data, size_t len) noexcept;

    void flush_buffer(void) noexcept;

    int baud_rate() const noexcept { return config_.baud_rate; }
    uart_word_length_t data_bits() const noexcept { return config_.data_bits; }
    uart_parity_t parity() const noexcept { return config_.parity; }
    uart_stop_bits_t stop_bits() const noexcept { return config_.stop_bits; }
    uart_hw_flowcontrol_t flow_ctrl() const noexcept { return config_.flow_ctrl; }
    int tx_pin() const noexcept { return config_.tx_pin; }
    int rx_pin() const noexcept { return config_.rx_pin; }
    size_t rx_buf_size() const noexcept { return config_.rx_buf_size; }

    uart_port_t port() const noexcept { return port_; }

private:
    uart_port_t port_;
    struct uart_config_s
    {
        int baud_rate;
        uart_word_length_t data_bits;
        uart_parity_t parity;
        uart_stop_bits_t stop_bits;
        uart_hw_flowcontrol_t flow_ctrl;
        int tx_pin;
        int rx_pin;
        size_t rx_buf_size;
    } config_;
};
