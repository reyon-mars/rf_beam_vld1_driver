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
