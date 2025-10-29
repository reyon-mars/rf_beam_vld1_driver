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

    // Setup the uart for the vld1 sensor
    static uart vld1_uart(UART_NUM_1, 12, 13, 115200, 512);
    vld1_uart.init(UART_DATA_8_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1);

    // Setup the uart for the rs485 interface
    static uart rs485_uart(UART_NUM_2, 17, 16, 115200, 512);
    static rs485 rs485_slave(rs485_uart, GPIO_NUM_5);
    rs485_slave.init(1, MB_PARAM_INPUT, 3);

    // Initialize LED
    static led main_led(GPIO_NUM_2);
    main_led.init();
    main_led.blink(1, 100);

    // Initialize the vld1_sensor
    static vld1 vld1_sensor(vld1_uart);
    static vld1::radar_params_t radar_parameters;
    static batch_averager averager(20);

    // Create Application (this creates the UART mutex internally)
    static Application app(vld1_uart, rs485_uart, rs485_slave, vld1_sensor, averager, main_led, radar_parameters);
    app.start();

    // Configure sensor parameters
    vld1_sensor.set_distance_range(vld1::vld1_distance_range_t::range_50);
    vld1_sensor.set_threshold_offset(40);
    vld1_sensor.set_min_range_filter(1);
    vld1_sensor.set_max_range_filter(505);
    vld1_sensor.set_target_filter(vld1::target_filter_t::strongest);
    vld1_sensor.set_precision_mode(vld1::precision_mode_t::high);
    vld1_sensor.set_chirp_integration_count(1);
    vld1_sensor.set_tx_power(31);
    vld1_sensor.set_short_range_distance_filter(vld1::short_range_distance_t::disable);

    // Send GRPS packet
    vld1::vld1_header_t grps_header{};
    std::memcpy(grps_header.header, "GRPS", 4);
    grps_header.payload_len = 0;
    vld1_sensor.send_packet(grps_header, nullptr);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Start GNFD task
    app.start_gnfd_task();

    // Create and initialize web server with UART mutex protection
    static web_server server(vld1_sensor, app.get_uart_mutex());
    server.init();

    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Connect to SSID: %s", server.getSSID().c_str());
    ESP_LOGI(TAG, "Password: %s", server.getPassword().c_str());
    ESP_LOGI(TAG, "Access web interface at: http://%s", server.getIP().c_str());
}