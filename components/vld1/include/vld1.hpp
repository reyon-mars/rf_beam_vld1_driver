#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <inttypes.h>
#include "esp_log.h"
#include "nvs_flash.h"
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
        INVALID_DATA_RECEIVED = 8,
        RESP_FRAME_ERR = 9,
        MUTEX_ERR = 10,
        SAVE_FAIL = 11,
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

    enum class gnfd_payload_t : uint8_t
    {
        DONE = 1U << 5,
        PDAT = 1U << 2,
        RFFT = 1U << 1,
        RADC = 1U << 0
    };

#pragma pack(push, 1)
    typedef struct
    {
        char firmware_version[19];
        char unique_id[12];
        vld1_distance_range_t distance_range;
        uint8_t threshold_offset;
        uint16_t min_range_filter;
        uint16_t max_range_filter;
        uint8_t distance_avg_count;
        target_filter_t target_filter;
        precision_mode_t distance_precision;
        uint8_t tx_power;
        uint8_t chirp_integration_count;
        short_range_distance_t short_range_distance_filter;
    } radar_params_t;
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
        vld1_header_t header;
        vld1_error_code_t err_code;
    } resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
        char version[19];
    } vers_resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
        radar_params_t params;
    } rpst_resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        float distance;
        uint16_t magnitude;
    } pdat_payload_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
        pdat_payload_t payload;
    } pdat_resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
    } grps_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
        radar_params_t params;
    } srps_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
    typedef struct
    {
        vld1_header_t header;
        gnfd_payload_t payload;
    } gnfd_req_t;
#pragma pack(pop)

    vld1(uart &uart_no) noexcept;

    radar_params_t get_curr_radar_params(void) const noexcept { return vld1_config_; };

    vld1_error_code_t init(const vld1_baud_t baud = vld1_baud_t::BAUD_115200) noexcept;

    vld1_error_code_t get_parameters(void) noexcept;
    vld1_error_code_t get_pdat(pdat_payload_t &pdat_data) noexcept;

    esp_err_t save_config(const radar_params_t &params_struct) noexcept;
    esp_err_t restore_config(void) noexcept;

    vld1_error_code_t set_radar_parameters(const radar_params_t &params_struct) noexcept;
    vld1_error_code_t set_distance_range(vld1_distance_range_t range) noexcept;
    vld1_error_code_t set_threshold_offset(uint8_t val) noexcept;
    vld1_error_code_t set_min_range_filter(uint16_t val) noexcept;
    vld1_error_code_t set_max_range_filter(uint16_t val) noexcept;
    vld1_error_code_t set_target_filter(target_filter_t filter) noexcept;
    vld1_error_code_t set_precision_mode(precision_mode_t mode) noexcept;
    vld1_error_code_t set_chirp_integration_count(uint8_t val) noexcept;
    vld1_error_code_t set_tx_power(uint8_t val) noexcept;
    vld1_error_code_t set_short_range_distance_filter(short_range_distance_t state) noexcept;

    vld1_error_code_t exit_sequence() noexcept;

private:
    void send_packet(const vld1_header_t &header, const uint8_t *payload) noexcept;

    vld1_error_code_t resp_status(void) noexcept;

    int parse_message(uint8_t *buffer, int len, char *out_header, uint8_t *out_payload, uint32_t *out_len) noexcept;

    void vld1_flush_buffer(void) noexcept;

    uart &uart_;
    SemaphoreHandle_t vld1_mutex_;
    radar_params_t vld1_config_;
    class scoped_lock
    {
    public:
        explicit scoped_lock(SemaphoreHandle_t mutex, TickType_t timeout = portMAX_DELAY) noexcept
            : mutex_(mutex), locked_(false)
        {
            if (mutex_ && xSemaphoreTake(mutex_, timeout) == pdTRUE)
                locked_ = true;
        }

        ~scoped_lock() noexcept
        {
            if (locked_)
                xSemaphoreGive(mutex_);
        }

        [[nodiscard]] bool locked() const noexcept { return locked_; }

    private:
        SemaphoreHandle_t mutex_;
        bool locked_;
    };

    using scoped_lock_t = scoped_lock;
};
