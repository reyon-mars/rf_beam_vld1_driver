#include "rs485_slave.hpp"
#include "esp_log.h"
#include <string>

static constexpr char TAG[] = "RS485";

rs485::rs485(uart &uart_no, gpio_num_t de_re_pin) noexcept
    : uart_(uart_no), de_re_pin_(de_re_pin),
      mbc_slave_handle_(nullptr),
      slave_address_(1),
      input_registers_(nullptr),
      input_reg_size_(0) {}

rs485::~rs485() noexcept
{
    if (mbc_slave_handle_)
    {
        mbc_slave_stop(mbc_slave_handle_);
        mbc_slave_delete(mbc_slave_handle_);
        mbc_slave_handle_ = nullptr;
    }
    free_registers();
}

void rs485::free_registers() noexcept
{
    delete[] input_registers_;
    input_registers_ = nullptr;
}

esp_err_t rs485::configure_pins() noexcept
{
    gpio_config_t io_conf{};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << static_cast<uint32_t>(de_re_pin_));
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure DE/RE pin: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level(de_re_pin_, 0);
    return ESP_OK;
}

esp_err_t rs485::init(uint8_t slave_addr, mb_param_type_t reg_type, size_t input_reg_count) noexcept
{
    slave_address_ = slave_addr;
    input_reg_size_ = input_reg_count;

    esp_err_t err = configure_pins();
    if (err != ESP_OK)
        return err;

    mb_communication_info_t comm_config{};
    comm_config.mode = MB_RTU;
    comm_config.ser_opts.mode = MB_RTU;
    comm_config.ser_opts.port = uart_.port();
    comm_config.ser_opts.uid = slave_address_;
    comm_config.ser_opts.baudrate = uart_.baud_rate();
    comm_config.ser_opts.data_bits = uart_.data_bits();
    comm_config.ser_opts.stop_bits = uart_.stop_bits();
    comm_config.ser_opts.parity = uart_.parity();

    err = mbc_slave_create_serial(&comm_config, &mbc_slave_handle_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create Modbus slave: %s", esp_err_to_name(err));
        return err;
    }

    free_registers();
    input_registers_ = new uint16_t[input_reg_size_]();

    mb_register_area_descriptor_t reg_area{};
    reg_area.start_offset = 0x00;
    reg_area.type = reg_type;
    reg_area.address = static_cast<void *>(input_registers_);
    reg_area.size = sizeof(uint16_t) * input_reg_size_;

    err = mbc_slave_set_descriptor(mbc_slave_handle_, reg_area);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Modbus input register descriptor: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(uart_set_pin(uart_.port(), uart_.tx_pin(),
                                 uart_.rx_pin(), de_re_pin_,
                                 UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_set_mode(uart_.port(), UART_MODE_RS485_HALF_DUPLEX));

    err = mbc_slave_start(mbc_slave_handle_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Modbus slave: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "RS485 Modbus slave initialized (Addr=%u, InputRegs=%zu)", slave_address_, input_reg_size_);

    // ------------------- Initialization Logs -------------------
    ESP_LOGI(TAG, "RS485 Modbus slave initialized (Addr=%u, InputRegs=%zu)", slave_address_, input_reg_size_);
    ESP_LOGI(TAG, "  Slave Address   : %u", slave_address_);
    ESP_LOGI(TAG, "  Register Type   : %s",
             (reg_type == MB_PARAM_INPUT ? "Input" : reg_type == MB_PARAM_HOLDING ? "Holding"
                                                 : reg_type == MB_PARAM_COIL      ? "Coil"
                                                 : reg_type == MB_PARAM_DISCRETE  ? "Discrete"
                                                                                  : "Unknown"));
    ESP_LOGI(TAG, "  Register Count  : %zu", input_reg_size_);

    ESP_LOGI(TAG, "UART Config:");
    ESP_LOGI(TAG, "  Port            : %d", uart_.port());
    ESP_LOGI(TAG, "  TX Pin          : %d", uart_.tx_pin());
    ESP_LOGI(TAG, "  RX Pin          : %d", uart_.rx_pin());
    ESP_LOGI(TAG, "  DE/RE Pin       : %d", de_re_pin_);
    ESP_LOGI(TAG, "  Baudrate        : %d", uart_.baud_rate());
    ESP_LOGI(TAG, "  Data bits       : %d", uart_.data_bits());
    ESP_LOGI(TAG, "  Parity          : %s",
             (uart_.parity() == UART_PARITY_DISABLE ? "None" : uart_.parity() == UART_PARITY_ODD ? "Odd"
                                                                                                 : "Even"));
    ESP_LOGI(TAG, "  Stop bits       : %d", uart_.stop_bits());

    // ------------------- Modbus Stack Check -------------------
    mb_param_info_t reg_info{};
    err = mbc_slave_get_param_info(mbc_slave_handle_, &reg_info, 10 /* timeout ms */);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Modbus stack active, param info:");
        ESP_LOGI(TAG, "  Event Type      : %u", reg_info.type);
        ESP_LOGI(TAG, "  MB Offset       : %u", (unsigned)reg_info.mb_offset);
        ESP_LOGI(TAG, "  Instance Addr   : 0x%" PRIx32, (uint32_t)reg_info.address);
        ESP_LOGI(TAG, "  Size (bytes)    : %u", (unsigned)reg_info.size);
        ESP_LOGI(TAG, "  Timestamp (us)  : %" PRIu32, reg_info.time_stamp);
    }
    else
    {
        ESP_LOGW(TAG, "No Modbus events yet (stack idle)");
    }

    return ESP_OK;
}

esp_err_t rs485::write(const uint16_t *data, size_t count) noexcept
{
    if (!mbc_slave_handle_ || !input_registers_)
    {
        ESP_LOGE(TAG, "Slave not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (count > input_reg_size_)
    {
        ESP_LOGE(TAG, "Register write out of bounds: start=0x00, count=%zu", count);
        return ESP_ERR_INVALID_ARG;
    }

    if (data)
    {
        std::memcpy(input_registers_, data, count * sizeof(uint16_t));
        ESP_LOGI(TAG, "Wrote %zu input registers starting at address 0", count);
    }
    else
    {
        std::fill(input_registers_, input_registers_ + input_reg_size_, uint16_t(0));
        ESP_LOGW(TAG, "No data received. Filled %zu registers with 0", count);
    }

    std::string reg_content;
    for (size_t i = 0; i < input_reg_size_; ++i)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%04X ", input_registers_[i]);
        reg_content += buf;
    }
    ESP_LOGI(TAG, "Current Input Registers: %s", reg_content.c_str());

    return ESP_OK;
}
