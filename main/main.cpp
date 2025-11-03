#include "uart.hpp"
#include "rs485_slave.hpp"
#include "vld1.hpp"
#include "averager.hpp"
#include "led.hpp"
#include "vld1_application.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "web_server.hpp"
#include "esp_log.h"
#include <cstring>

static constexpr char TAG[] = "Main";

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Starting VLD1 app");

    static uart vld1_uart(UART_NUM_1, 12, 13, 115200, 512);
    vld1_uart.init(UART_DATA_8_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1);

    static uart rs485_uart(UART_NUM_2, 17, 16, 115200, 512);
    static rs485 rs485_slave(rs485_uart, GPIO_NUM_5);
    rs485_slave.init(1, MB_PARAM_INPUT, 3);

    static led main_led(GPIO_NUM_2);
    main_led.init();
    main_led.blink(1, 100);

    static vld1 vld1_sensor(vld1_uart);
    static batch_averager averager(20);

    static application app(rs485_slave, vld1_sensor, averager, main_led);

    vld1_sensor.init();
    vld1_sensor.set_distance_range(vld1::vld1_distance_range_t::range_50);
    vld1_sensor.set_threshold_offset(40);
    vld1_sensor.set_min_range_filter(1);
    vld1_sensor.set_max_range_filter(505);
    vld1_sensor.set_target_filter(vld1::target_filter_t::strongest);
    vld1_sensor.set_precision_mode(vld1::precision_mode_t::high);
    vld1_sensor.set_chirp_integration_count(1);
    vld1_sensor.set_tx_power(31);
    vld1_sensor.set_short_range_distance_filter(vld1::short_range_distance_t::disable);

    vld1_sensor.get_parameters();

    static web_server server(vld1_sensor);
    server.init();

    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Connect to SSID: %s", server.get_ssid().c_str());
    ESP_LOGI(TAG, "Password: %s", server.get_password().c_str());
    ESP_LOGI(TAG, "Access web interface at: http://%s", server.get_ip().c_str());

    app.start_read_and_forward();
}