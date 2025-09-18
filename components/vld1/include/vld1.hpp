#pragma once
#include <cstdint>
#include <cstddef>
#include "uart.hpp"
class vld1
{
public:
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
        float distance;
        uint16_t magnitude;
    } pdat_resp_t;
#pragma pack(pop)

    vld1(uart &uart_no) noexcept;

    void send_packet(const vld1_header_t &header, const uint8_t *payload) noexcept;

    int parse_message(uint8_t *buffer, int len, char *out_header, uint8_t *out_payload, uint32_t *out_len) noexcept;

private:
    uart &uart_;
};
