#pragma once
#include <cstdint>
#include <cstddef>
#include "uart.hpp"
class vld1
{
public:
    enum class vld1_error_code_t : uint8_t
    {
        OK = 0,
        UNKNOWN_CMD = 1,
        INVALID_PARAM_VAL = 2,
        INVALID_RPST_VERS = 3,
        UART_ERROR = 4,
        NO_CALIB_VALUES = 5,
        TIMEOUT = 6,
        APP_CORRUPT = 7,
    };

    enum class vld1_baud_t : uint8_t
    {
        BAUD_115200 = 0,
        BAUD_460800 = 1,
        BAUD_921600 = 2,
        BAUD_2000000 = 3,
    };

    enum class vld1_distance_range_t : uint8_t
    {
        range_20 = 0,
        range_50 = 1
    };

    enum class target_filter_t : uint8_t
    {
        strongest = 0,
        nearest = 1,
        farthest = 2
    };

    enum class precision_mode_t : uint8_t
    {
        low = 0,
        high = 1
    };

    enum class short_range_distance_t : uint8_t
    {
        disable = 0,
        enable = 1
    };

#pragma pack(push, 1)
    typedef struct
    {
        char firmware_version[19] = "V-LD1_APP-RFB-YYXX";
        char unique_id[12] = "L1234n12345";
        vld1_distance_range_t distance_range = vld1_distance_range_t::range_20;
        uint8_t threshold_offset = 40;
        uint16_t min_range_filter = 5;
        uint16_t max_range_filter = 460;
        uint8_t distance_avg_count = 5;
        target_filter_t target_filter = target_filter_t::strongest;
        precision_mode_t distance_precision = precision_mode_t::low;
        uint8_t tx_power = 31;
        uint8_t chirp_integration_count = 1;
        short_range_distance_t short_range_distance_filter = short_range_distance_t::disable;
    } radar_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        char header[4];
        uint32_t payload_len;
        vld1_error_code_t err_code;
    } resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        char header[4];
        uint32_t payload_len;
    } vld1_header_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        char header[4];
        uint32_t payload_len;
        char version[19];
    } vers_resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        float distance;
        uint16_t magnitude;
    } pdat_resp_t;
#pragma pack(pop)

    vld1(uart &uart_no) noexcept;

    void send_packet(const vld1_header_t &header, const uint8_t *payload) noexcept;

    int parse_message(uint8_t *buffer, int len, char *out_header, uint8_t *out_payload, uint32_t *out_len) noexcept;
    void flush_buffer(void) noexcept;

    void set_radar_parameters(const radar_params_t &params_struct) noexcept;
    void set_distance_range(vld1_distance_range_t range) noexcept;
    void set_threshold_offset(uint8_t val) noexcept;
    void set_min_range_filter(uint16_t val) noexcept;
    void set_max_range_filter(uint16_t val) noexcept;
    void set_target_filter(target_filter_t filter) noexcept;
    void set_precision_mode(precision_mode_t mode) noexcept;
    void set_chirp_integration_count(uint8_t val) noexcept;
    void set_tx_power(uint8_t power) noexcept;
    void set_short_range_distance_filter(short_range_distance_t state) noexcept;
    void exit_sequence() noexcept;
    void get_parameters(radar_params_t &params) noexcept;

private:
    inline void init(const vld1_baud_t baud) noexcept;
    esp_err_t resp_status(void) noexcept;
    esp_err_t vers_resp(void) noexcept;
    uart &uart_;
};
