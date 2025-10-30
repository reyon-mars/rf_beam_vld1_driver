#include "vld1.hpp"
#include "esp_log.h"
#include <cstring>

static constexpr char TAG[] = "VLD1";

vld1::vld1(uart &uart_no) noexcept
    : uart_(uart_no)
{
    init(vld1_baud_t::BAUD_115200);
}

void vld1::send_packet(const vld1_header_t &header, const uint8_t *payload) noexcept
{
    const size_t total_len = sizeof(vld1_header_t) + header.payload_len;
    uint8_t buf[512];

    if (total_len > sizeof(buf))
    {
        ESP_LOGE(TAG, "send_packet: Packet too large (%zu > %zu)", total_len, sizeof(buf));
        return;
    }

    std::memcpy(buf, &header, sizeof(vld1_header_t));

    if (header.payload_len && payload)
        std::memcpy(buf + sizeof(vld1_header_t), payload, header.payload_len);

    ESP_LOG_BUFFER_HEXDUMP(TAG, buf, total_len, ESP_LOG_WARN);
    uart_.write(buf, total_len);
}

int vld1::parse_message(uint8_t *buffer, int len, char *response_code,
                        uint8_t *out_payload, uint32_t *out_len) noexcept
{
    if (len < sizeof(vld1_header_t))
        return 0;

    const auto *out_header = reinterpret_cast<vld1_header_t *>(buffer);

    std::memcpy(response_code, out_header->header, 4);
    response_code[4] = '\0';

    uint32_t payload_len = out_header->payload_len;
    *out_len = payload_len;

    uint32_t total_len = sizeof(vld1_header_t) + payload_len;
    if (len < static_cast<int>(total_len))
        return 0;

    if (payload_len > 0 && out_payload)
        std::memcpy(out_payload, buffer + sizeof(vld1_header_t), payload_len);

    return static_cast<int>(total_len);
}

esp_err_t vld1::resp_status() noexcept
{
    alignas(resp_t) uint8_t buffer[sizeof(resp_t)];
    esp_err_t err = uart_.read_exact(buffer, sizeof(resp_t), pdMS_TO_TICKS(100));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Insufficient bytes read.");
        return ESP_FAIL;
    }

    const auto *resp_data = reinterpret_cast<const resp_t *>(buffer);

    if (std::strncmp(resp_data->header, "RESP", 4) != 0 || resp_data->payload_len != 1)
    {
        ESP_LOGE(TAG, "Invalid response header or payload length.");
        return ESP_FAIL;
    }

    switch (resp_data->err_code)
    {
    case vld1_error_code_t::OK:
        ESP_LOGI(TAG, "RESP: OK");
        break;

    case vld1_error_code_t::UNKNOWN_CMD:
        ESP_LOGW(TAG, "RESP: Unknown command");
        break;

    case vld1_error_code_t::INVALID_PARAM_VAL:
        ESP_LOGW(TAG, "RESP: Invalid parameter value");
        break;

    case vld1_error_code_t::INVALID_RPST_VERS:
        ESP_LOGW(TAG, "RESP: Invalid RPST version");
        break;

    case vld1_error_code_t::UART_ERROR:
        ESP_LOGW(TAG, "RESP: UART error");
        break;

    case vld1_error_code_t::NO_CALIB_VALUES:
        ESP_LOGW(TAG, "RESP: No calibration values");
        break;

    case vld1_error_code_t::TIMEOUT:
        ESP_LOGW(TAG, "RESP: Timeout");
        break;

    case vld1_error_code_t::APP_CORRUPT:
        ESP_LOGW(TAG, "RESP: Application corrupt");
        break;

    default:
        ESP_LOGW(TAG, "RESP: Unknown error code %u",
                 static_cast<uint8_t>(resp_data->err_code));
        break;
    }

    return ESP_OK;
}

esp_err_t vld1::vers_resp() noexcept
{
    alignas(vers_resp_t) uint8_t buffer[sizeof(vers_resp_t)];
    esp_err_t err = uart_.read_exact(buffer, sizeof(vers_resp_t), pdMS_TO_TICKS(100));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Insufficient bytes read.");
        return ESP_FAIL;
    }

    const auto *vers_resp_data = reinterpret_cast<const vers_resp_t *>(buffer);

    if (std::strncmp(vers_resp_data->header, "VERS", 4) != 0 || vers_resp_data->payload_len != 19)
    {
        ESP_LOGE(TAG, "Invalid response header or payload length.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "VLD1 VERSION: %s", vers_resp_data->version);
    return ESP_OK;
}

inline void vld1::init(const vld1_baud_t baud) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "INIT", 4);

    const uint8_t payload = static_cast<uint8_t>(baud);
    header.payload_len = sizeof(vld1_baud_t);

    send_packet(header, &payload);

    esp_err_t err = resp_status();
    if (err == ESP_OK)
        err = vers_resp();

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Invalid response received.");
}

void vld1::set_radar_parameters(const radar_params_t &params_struct) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "SRPS", 4);

    header.payload_len = sizeof(radar_params_t);
    send_packet(header, reinterpret_cast<const uint8_t *>(&params_struct));

    resp_status();
}

void vld1::set_distance_range(vld1_distance_range_t range) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "RRAI", 4);

    const uint8_t payload = static_cast<uint8_t>(range);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);

    resp_status();
}

void vld1::set_threshold_offset(uint8_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "THOF", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);

    resp_status();
}

void vld1::set_min_range_filter(uint16_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "MIRA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));

    resp_status();
}

void vld1::set_max_range_filter(uint16_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "MARA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));

    resp_status();
}

void vld1::set_target_filter(target_filter_t filter) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "TGFI", 4);

    const uint8_t payload = static_cast<uint8_t>(filter);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);

    resp_status();
}

void vld1::set_precision_mode(precision_mode_t mode) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "PREC", 4);

    const uint8_t payload = static_cast<uint8_t>(mode);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);

    resp_status();
}

void vld1::exit_sequence() noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "GBYE", 4);

    header.payload_len = 0;
    send_packet(header, nullptr);

    resp_status();
}

void vld1::set_chirp_integration_count(uint8_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "INTN", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);

    resp_status();
}

void vld1::set_tx_power(uint8_t power) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "TXPW", 4);

    header.payload_len = sizeof(power);
    send_packet(header, &power);

    resp_status();
}

void vld1::set_short_range_distance_filter(short_range_distance_t state) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "SRDF", 4);

    const uint8_t payload = static_cast<uint8_t>(state);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);

    resp_status();
}

void vld1::flush_buffer()
{
    uart_.flush_buffer();
}
