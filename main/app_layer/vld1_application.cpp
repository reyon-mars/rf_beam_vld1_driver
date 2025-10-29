#include "vld1_application.hpp"

static constexpr char TAG[] = "Application";

void Application::start()
{
    xTaskCreate(vld1_read_task, "vld1_read_task", 4096, &ctx_, 10, nullptr);
}

void Application::start_gnfd_task()
{
    xTaskCreate(gnfd_task, "gnfd_task", 4096, this, 5, nullptr);
    ESP_LOGI(TAG, "GNFD task started.");
}

void Application::vld1_read_task(void *arg)
{
    auto *ctx = static_cast<AppContext *>(arg);
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
                    // === RESP Handler ===
                    if (std::strcmp(header, "RESP") == 0 && payload_len >= 1)
                    {
                        uint8_t error_code = payload[0];
                        uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF};

                        switch (error_code)
                        {
                        case 0:
                            ESP_LOGI(TAG, "VLD1 RESP: OK");
                            break;
                        case 1:
                            ESP_LOGW(TAG, "RESP: Unknown command");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 2:
                            ESP_LOGW(TAG, "RESP: Invalid parameter value");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 3:
                            ESP_LOGW(TAG, "RESP: Invalid RPST version");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 4:
                            ESP_LOGW(TAG, "RESP: UART error");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 5:
                            ESP_LOGW(TAG, "RESP: No calibration values");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 6:
                            ESP_LOGW(TAG, "RESP: Timeout");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        case 7:
                            ESP_LOGW(TAG, "RESP: Application corrupt");
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        default:
                            ESP_LOGW(TAG, "RESP: Unknown error code %u", error_code);
                            ctx->rs485_slave->write(regs, 3);
                            break;
                        }
                    }

                    // === PDAT Handler ===
                    else if (std::strcmp(header, "PDAT") == 0)
                    {
                        uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF};

                        if (payload_len >= sizeof(vld1::pdat_resp_t))
                        {
                            auto *pdat = reinterpret_cast<vld1::pdat_resp_t *>(payload);
                            float distance_m = pdat->distance;
                            uint16_t distance_mm = static_cast<uint16_t>(distance_m * 1000.0f);

                            ctx->averager->add_sample(distance_m);
                            uint16_t avg_mm = ctx->averager->average_millimeters();

                            regs[0] = distance_mm;
                            regs[1] = pdat->magnitude;
                            regs[2] = avg_mm;

                            ESP_LOGI(TAG, "Distance=%u mm | Mag=%u | Avg=%u", distance_mm, pdat->magnitude, avg_mm);

                            if (ctx->averager->is_complete())
                            {
                                ESP_LOGI(TAG, "Batch complete: avg=%.3f m (%u mm)",
                                         ctx->averager->average_meters(), avg_mm);
                                ctx->averager->reset();
                            }
                        }
                        else
                        {
                            ESP_LOGW(TAG, "PDAT: Invalid payload length.");
                        }

                        ctx->rs485_slave->write(regs, 3);
                    }

                    // === VERS Handler ===
                    else if (std::strcmp(header, "VERS") == 0)
                    {
                        char fw[65] = {0};
                        int copy_len = (payload_len < 64) ? static_cast<int>(payload_len) : 64;
                        std::memcpy(fw, payload, copy_len);
                        ESP_LOGI(TAG, "Firmware: %s", fw);
                    }

                    // === RPST Handler ===
                    else if (std::strcmp(header, "RPST") == 0)
                    {
                        if (payload_len == sizeof(vld1::radar_params_t))
                        {
                            ctx->radar_params = *reinterpret_cast<vld1::radar_params_t *>(payload);

                            ESP_LOGI(TAG, "Radar Params Updated:");
                            ESP_LOGI(TAG, "  FW Ver: %s", ctx->radar_params.firmware_version);
                            ESP_LOGI(TAG, "  Unique ID: %s", ctx->radar_params.unique_id);
                            ESP_LOGI(TAG, "  Range: %u", static_cast<uint8_t>(ctx->radar_params.distance_range));
                            ESP_LOGI(TAG, "  TX Power: %u", ctx->radar_params.tx_power);
                        }
                        else
                        {
                            ESP_LOGW(TAG, "RPST: payload size mismatch (%" PRIu32 ")", payload_len);
                        }
                    }

                    std::memmove(buffer, buffer + parsed_len, buf_len - parsed_len);
                    buf_len -= parsed_len;
                }

            } while (parsed_len > 0 && buf_len > 0);
        }
    }
}

void Application::gnfd_task(void *arg)
{
    auto *app = static_cast<Application *>(arg);
    vld1 &sensor = *app->ctx_.vld1_sensor;

    while (true)
    {
        if (xSemaphoreTake(app->uart_mutex_, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            const uint8_t payload_gnfd = 0x04;
            vld1::vld1_header_t header{};
            std::memcpy(header.header, "GNFD", 4);
            header.payload_len = 1;

            sensor.send_packet(header, &payload_gnfd);

            xSemaphoreGive(app->uart_mutex_);
        }
        else
        {
            ESP_LOGV(TAG, "GNFD: Could not acquire mutex, skipping this cycle");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
