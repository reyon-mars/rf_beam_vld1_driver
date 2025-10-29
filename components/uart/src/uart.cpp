#include "uart.hpp"
#include "esp_log.h"
#include "esp_err.h"
#include <cstring>

static constexpr char TAG[] = "Uart";

uart::uart(uart_port_t port, int tx_pin, int rx_pin, int baud_rate, size_t rx_buf_size) noexcept
    : port_(port)
{
    config_.baud_rate = baud_rate;
    config_.tx_pin = tx_pin;
    config_.rx_pin = rx_pin;
    config_.rx_buf_size = rx_buf_size;
    config_.data_bits = UART_DATA_8_BITS;
    config_.parity = UART_PARITY_DISABLE;
    config_.stop_bits = UART_STOP_BITS_1;
    config_.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
}

uart::~uart() noexcept
{
    uart_driver_delete(port_);
}

esp_err_t uart::init(uart_word_length_t data_bits, uart_parity_t parity, uart_stop_bits_t stop_bits) noexcept
{

    config_.data_bits = data_bits;
    config_.parity = parity;
    config_.stop_bits = stop_bits;

    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = config_.baud_rate;
    uart_cfg.data_bits = config_.data_bits;
    uart_cfg.parity = config_.parity;
    uart_cfg.stop_bits = config_.stop_bits;
    uart_cfg.flow_ctrl = config_.flow_ctrl;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config(port_, &uart_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(port_, config_.tx_pin, config_.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install(port_, config_.rx_buf_size * 2, 0, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "UART%u initialized (TX=%d RX=%d @%d)", port_, config_.tx_pin, config_.rx_pin, config_.baud_rate);
    return ESP_OK;
}

int uart::read(uint8_t *dst, size_t max_len, TickType_t ticks_to_wait) noexcept
{
    return uart_read_bytes(port_, dst, max_len, ticks_to_wait);
}

int uart::write(const uint8_t *data, size_t len) noexcept
{
    int written = uart_write_bytes(port_, reinterpret_cast<const char *>(data), len);
    if (written > 0)
    {
        uart_wait_tx_done(port_, pdMS_TO_TICKS(100));
    }
    return written;
}

void uart::flush_buffer ( void ) noexcept
{
    uart_flush_input(port_);
}
