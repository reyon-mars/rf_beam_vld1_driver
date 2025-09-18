#include "vld1_application.hpp"

static constexpr char TAG[] = "Application";

void Application::start()
{
    xTaskCreate(uart_read_task, "uart_read_task", 4096, &ctx_, 10, nullptr);
}

void Application::uart_read_task(void *arg)
{
    AppContext *ctx = static_cast<AppContext *>(arg);

    uint8_t buffer[512];
    int buf_len = 0;

    while (true)
    {
        int read_bytes = ctx->vld1_uart->read(buffer + buf_len, sizeof(buffer) - buf_len, pdMS_TO_TICKS(100));
        if (read_bytes > 0)
        {
            buf_len += read_bytes;
            int parsed_len = 0;
            do
            {
                char header[5] = {0};
                uint8_t payload[128] = {0};
                uint32_t payload_len = 0;

                parsed_len = ctx->vld1_sensor->parse_message(buffer, buf_len, header, payload, &payload_len);
                if (parsed_len > 0)
                {
                    if (std::strcmp(header, "RESP") == 0 && payload_len >= 1)
                    {
                        uint8_t error_code = payload[0];
                        uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF}; // default error

                        switch (error_code)
                        {
                        case 0:
                            ESP_LOGI(TAG, "VLD1 RESP: OK");
                            break;
                        case 1:
                            ESP_LOGW(TAG, "VLD1 RESP: Unknown command");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 2:
                            ESP_LOGW(TAG, "VLD1 RESP: Invalid parameter value");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 3:
                            ESP_LOGW(TAG, "VLD1 RESP: Invalid RPST version");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 4:
                            ESP_LOGW(TAG, "VLD1 RESP: UART error (parity, framing, noise)");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 5:
                            ESP_LOGW(TAG, "VLD1 RESP: No calibration values");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 6:
                            ESP_LOGW(TAG, "VLD1 RESP: Timeout");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 7:
                            ESP_LOGW(TAG, "VLD1 RESP: Application corrupt or not programmed");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        default:
                            ESP_LOGW(TAG, "VLD1 RESP: Unknown error code %u", error_code);
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        }
                    }
                    // ---------------- PDAT Handling ----------------
                    // ---------------- PDAT Handling ----------------
                    else if (std::strcmp(header, "PDAT") == 0)
                    {
                        uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF}; // default error values

                        if (payload_len >= sizeof(vld1::pdat_resp_t))
                        {
                            auto *pdat_data = reinterpret_cast<vld1::pdat_resp_t *>(payload);
                            float distance_m = pdat_data->distance;
                            uint16_t distance_mm = static_cast<uint16_t>(distance_m * 1000.0f);

                            ctx->averager->add_sample(distance_m);
                            uint16_t avg_mm = ctx->averager->average_millimeters();

                            regs[0] = distance_mm;
                            regs[1] = pdat_data->magnitude;
                            regs[2] = avg_mm;

                            ESP_LOGI(TAG, "\t[ Distance (mm): %u | Magnitude (dB): %u | Avg (mm): %u ]",
                                     distance_mm, pdat_data->magnitude, avg_mm);

                            if (ctx->averager->is_complete())
                            {
                                ESP_LOGI(TAG, "Batch complete: avg=%.3f m (%u mm)", ctx->averager->average_meters(), avg_mm);
                                ctx->averager->reset();
                            }
                        }
                        else
                        {
                            // No target detected → payload missing
                            ESP_LOGW(TAG, "PDAT: No target detected, sending error values to Modbus");
                        }

                        // Write to RS485 Modbus slave in all cases (payload or no payload)
                        ctx->rs485_slave->write(regs, 3);
                    }

                    // ---------------- VERS Handling ----------------
                    else if (std::strcmp(header, "VERS") == 0)
                    {
                        char fw[65] = {0};
                        int copy_len = (payload_len < 64) ? static_cast<int>(payload_len) : 64;
                        std::memcpy(fw, payload, copy_len);
                        ESP_LOGI(TAG, "VLD1 Firmware: %s", fw);
                    }

                    std::memmove(buffer, buffer + parsed_len, buf_len - parsed_len);
                    buf_len -= parsed_len;
                }
            } while (parsed_len > 0 && buf_len > 0);
        }
    }
}
