#include "uart.hpp"
#include "rs485_slave.hpp"
#include "vld1.hpp"
#include "averager.hpp"
#include "led.hpp"
#include "vld1_application.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

    static Application app(vld1_uart, rs485_uart, rs485_slave, vld1_sensor, averager, main_led);
    app.start();

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
