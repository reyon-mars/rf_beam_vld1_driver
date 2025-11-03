#include "vld1_application.hpp"

static constexpr char TAG[] = "Application";

void application::start_read_and_forward()
{
    xTaskCreate(get_pdat_and_forward, "PDAT_read_forward", 4096, this, 5, nullptr);
    ESP_LOGI(TAG, "PDAT read and forward task started.");
}

void application::get_pdat_and_forward(void *arg)
{
    auto *app = static_cast<application *>(arg);

    vld1 &sensor = *app->ctx_.vld1_sensor;
    rs485 &rs485_slave = *app->ctx_.rs485_slave;
    batch_averager &avg = *app->ctx_.averager;
    led &led_pdat = *app->ctx_.main_led;

    vld1::pdat_payload_t pdat_data{};
    uint16_t rs485_regs[3] = {0xFFFF, 0xFFFF, 0xFFFF};

    while (true)
    {
        if (sensor.get_pdat(pdat_data) == ESP_OK)
        {
            float distance_m = pdat_data.distance;
            uint16_t distance_mm = static_cast<uint16_t>(distance_m * 1000.0f);

            avg.add_sample(distance_m);
            uint16_t avg_distance_mm = avg.average_millimeters();

            rs485_regs[0] = distance_mm;
            rs485_regs[1] = pdat_data.magnitude;
            rs485_regs[2] = avg_distance_mm;

            ESP_LOGI(TAG, "Distance=%u mm | Mag=%u | Avg=%u",
                     distance_mm, pdat_data.magnitude, avg_distance_mm);

            rs485_slave.write(rs485_regs, 3);
        }
        else
        {
            rs485_regs[0] = 0xFFFF;
            rs485_regs[1] = 0xFFFF;
            rs485_regs[2] = 0xFFFF;
            rs485_slave.write(rs485_regs, 3);
        }

        led_pdat.blink(1, 10);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
