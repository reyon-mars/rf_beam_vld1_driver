#include "uart.hpp"
#include "rs485_slave.hpp"
#include "vld1.hpp"
#include "averager.hpp"
#include "led.hpp"
#include "vld1_application.hpp"
#include "web_server.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <cstring>

static constexpr char TAG[] = "Main";

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Starting VLD1 app");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    VLD1ConfigServer server;
    ESP_ERROR_CHECK(server.start());

    static uart vld1_uart(UART_NUM_1, 12, 13, 115200, 512);
    vld1_uart.init(UART_DATA_8_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1);

    static uart rs485_uart(UART_NUM_2, 17, 16, 115200, 512);
    static rs485 rs485_slave(rs485_uart, GPIO_NUM_5);
    rs485_slave.init(1, MB_PARAM_INPUT, 3);

    static led main_led(GPIO_NUM_2);
    main_led.init();
    main_led.blink(1, 100);

    static vld1 vld1_sensor(vld1_uart);
    static vld1::radar_params_t radar_parameters;

    static batch_averager averager(20);

    static Application app(vld1_uart, rs485_uart, rs485_slave, vld1_sensor, averager, main_led, radar_parameters);
    app.start();

    vld1_sensor.set_distance_range(vld1::vld1_distance_range_t::range_50);
    vld1_sensor.set_threshold_offset(40);
    vld1_sensor.set_min_range_filter(1);
    vld1_sensor.set_max_range_filter(505);
    vld1_sensor.set_target_filter(vld1::target_filter_t::strongest);
    vld1_sensor.set_precision_mode(vld1::precision_mode_t::high);
    vld1_sensor.set_chirp_integration_count(1);
    vld1_sensor.set_tx_power(31);
    vld1_sensor.set_short_range_distance_filter(vld1::short_range_distance_t::disable);

    vld1::vld1_header_t grps_header{};
    std::memcpy(grps_header.header, "GRPS", 4);
    grps_header.payload_len = 0;
    vld1_sensor.send_packet(grps_header, nullptr);

    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true)
    {
        const uint8_t payload_gnfd = 0x04;
        vld1::vld1_header_t header{};
        std::memcpy(header.header, "GNFD", 4);
        header.payload_len = 1;
        vld1_sensor.send_packet(header, &payload_gnfd);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
