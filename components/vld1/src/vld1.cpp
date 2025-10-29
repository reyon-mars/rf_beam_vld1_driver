#include "vld1.hpp"
#include "esp_log.h"
#include <cstring>

static constexpr char TAG[] = "VLD1";

vld1::vld1(uart &uart_no) noexcept : uart_(uart_no) {}

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

int vld1::parse_message(uint8_t *buffer, int len, char *response_code, uint8_t *out_payload, uint32_t *out_len) noexcept
{
    if (len < sizeof(vld1_header_t))
        return 0;

    const vld1_header_t *out_header = reinterpret_cast<vld1_header_t *>(buffer);
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

void vld1::set_radar_parameters(const radar_params_t &params_struct) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "SRPS", 4);

    header.payload_len = sizeof(radar_params_t);
    send_packet(header, reinterpret_cast<const uint8_t *>(&params_struct));
}

void vld1::set_distance_range(vld1_distance_range_t range) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "RRAI", 4);

    const uint8_t payload = static_cast<uint8_t>(range);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);
}

void vld1::set_threshold_offset(uint8_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "THOF", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);
}

void vld1::set_min_range_filter(uint16_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "MIRA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));
}

void vld1::set_max_range_filter(uint16_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "MARA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));
}

void vld1::set_target_filter(target_filter_t filter) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "TGFI", 4);

    const uint8_t payload = static_cast<uint8_t>(filter);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);
}

void vld1::set_precision_mode(precision_mode_t mode) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "PREC", 4);

    const uint8_t payload = static_cast<uint8_t>(mode);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);
}

void vld1::exit_sequence() noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "GBYE", 4);

    header.payload_len = 0;
    send_packet(header, nullptr);
}

void vld1::set_chirp_integration_count(uint8_t val) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "INTN", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);
}

void vld1::set_tx_power(uint8_t power) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "TXPW", 4);

    header.payload_len = sizeof(power);
    send_packet(header, &power);
}

void vld1::set_short_range_distance_filter(short_range_distance_t state) noexcept
{
    vld1_header_t header{};
    std::memcpy(header.header, "SRDF", 4);

    const uint8_t payload = static_cast<uint8_t>(state);
    header.payload_len = sizeof(payload);

    send_packet(header, &payload);
}

void vld1::flush_buffer ( void )
{
    uart_.flush_buffer();
}
