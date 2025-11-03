#include "vld1.hpp"
#include "esp_log.h"
#include <cstring>
#include <inttypes.h>

static constexpr char TAG[] = "VLD1";

vld1::vld1(uart &uart_no) noexcept
    : uart_(uart_no)
{
    vld1_mutex_ = xSemaphoreCreateMutex();
    if (vld1_mutex_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create VLD1 Mutex.");
    }
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

vld1::vld1_error_code_t vld1::resp_status() noexcept
{
    resp_t resp_data{};
    esp_err_t err = uart_.read_exact(reinterpret_cast<uint8_t *>(&resp_data), sizeof(resp_t));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Insufficient bytes read.");
        return vld1_error_code_t::RESP_FRAME_ERR;
    }

    if (std::strncmp(resp_data.header.header, "RESP", 4) != 0 ||
        resp_data.header.payload_len != sizeof(vld1_error_code_t))
    {
        ESP_LOGE(TAG, "Invalid response header or payload length.");
        return vld1_error_code_t::RESP_FRAME_ERR;
    }

    switch (resp_data.err_code)
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
                 static_cast<uint8_t>(resp_data.err_code));
        break;
    }

    return resp_data.err_code;
}

esp_err_t vld1::get_parameters(void) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }

    vld1_header_t header{};
    std::memcpy(header.header, "GRPS", 4);
    header.payload_len = 0;

    send_packet(header, nullptr);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    rpst_resp_t rpst_data{};
    esp_err_t err = uart_.read_exact(reinterpret_cast<uint8_t *>(&rpst_data), sizeof(rpst_resp_t));
    if (err != ESP_OK)
    {
        return err;
    }

    if (std::strncmp(rpst_data.header.header, "RPST", 4) != 0 ||
        rpst_data.header.payload_len != sizeof(radar_params_t))
    {
        return ESP_FAIL;
    }

    std::memcpy(&vld1_config_, &rpst_data.params, sizeof(radar_params_t));

    ESP_LOGI("VLD1", "---------------------- RADAR CONFIGURATION ----------------------");
    ESP_LOGI("VLD1", "Firmware Version      : %.*s",
             static_cast<int>(sizeof(vld1_config_.firmware_version)),
             vld1_config_.firmware_version);
    ESP_LOGI("VLD1", "Unique ID             : %.*s",
             static_cast<int>(sizeof(vld1_config_.unique_id)),
             vld1_config_.unique_id);

    ESP_LOGI("VLD1", "Distance Range        : %s",
             vld1_config_.distance_range == vld1_distance_range_t::range_20 ? "20m" : vld1_config_.distance_range == vld1_distance_range_t::range_50 ? "50m"
                                                                                                                                                     : "Unknown");

    ESP_LOGI("VLD1", "Threshold Offset      : %u", vld1_config_.threshold_offset);
    ESP_LOGI("VLD1", "Min Range Filter      : %u mm", vld1_config_.min_range_filter);
    ESP_LOGI("VLD1", "Max Range Filter      : %u mm", vld1_config_.max_range_filter);
    ESP_LOGI("VLD1", "Distance Avg Count    : %u", vld1_config_.distance_avg_count);

    ESP_LOGI("VLD1", "Target Filter         : %s",
             vld1_config_.target_filter == target_filter_t::strongest ? "Strongest" : vld1_config_.target_filter == target_filter_t::nearest ? "Nearest"
                                                                                  : vld1_config_.target_filter == target_filter_t::farthest  ? "Farthest"
                                                                                                                                             : "Unknown");

    ESP_LOGI("VLD1", "Precision Mode        : %s",
             vld1_config_.distance_precision == precision_mode_t::low ? "Low" : vld1_config_.distance_precision == precision_mode_t::high ? "High"
                                                                                                                                          : "Unknown");

    ESP_LOGI("VLD1", "TX Power              : %u", vld1_config_.tx_power);
    ESP_LOGI("VLD1", "Chirp Integration Cnt : %u", vld1_config_.chirp_integration_count);

    ESP_LOGI("VLD1", "Short Range Filter    : %s",
             vld1_config_.short_range_distance_filter == short_range_distance_t::enable ? "Enabled" : "Disabled");

    ESP_LOGI("VLD1", "----------------------------------------------------------------");

    return ESP_OK;
};

esp_err_t vld1::get_pdat(pdat_payload_t &pdat_data) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }

    gnfd_req_t gnfd_req{};
    std::memcpy(gnfd_req.header.header, "GNFD", 4);
    gnfd_req.header.payload_len = sizeof(gnfd_payload_t);
    gnfd_req.payload = gnfd_payload_t::PDAT;

    send_packet(gnfd_req.header, reinterpret_cast<const uint8_t *>(&gnfd_req.payload));

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    pdat_resp_t pdat_resp{};
    esp_err_t err = uart_.read_exact(reinterpret_cast<uint8_t *>(&pdat_resp), sizeof(pdat_resp_t));
    if (err != ESP_OK)
        return err;

    if (std::strncmp(pdat_resp.header.header, "PDAT", 4) != 0)
    {
        ESP_LOGE("VLD1", "Invalid PDAT header received");
        return ESP_FAIL;
    }

    if (pdat_resp.header.payload_len != sizeof(pdat_payload_t))
    {
        ESP_LOGE("VLD1",
                 "Invalid PDAT payload length: expected %zu, got %" PRIu32,
                 sizeof(pdat_payload_t),
                 pdat_resp.header.payload_len);
        return ESP_FAIL;
    }

    pdat_data = pdat_resp.payload;

    ESP_LOGI("VLD1", "---------------- PDAT FRAME ----------------");
    ESP_LOGI("VLD1", "Distance  : %.3f m", static_cast<double>(pdat_data.distance));
    ESP_LOGI("VLD1", "Magnitude : %u", pdat_data.magnitude);
    ESP_LOGI("VLD1", "-------------------------------------------");

    return ESP_OK;
}

esp_err_t vld1::init(const vld1_baud_t baud) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }

    vld1_header_t header{};
    std::memcpy(header.header, "INIT", 4);

    const uint8_t payload = static_cast<uint8_t>(baud);
    header.payload_len = sizeof(vld1_baud_t);

    send_packet(header, &payload);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    vers_resp_t vers_resp{};
    esp_err_t err = uart_.read_exact(reinterpret_cast<uint8_t *>(&vers_resp), sizeof(vers_resp_t));

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Insufficient bytes read.");
        return ESP_FAIL;
    }

    if (std::strncmp(vers_resp.header.header, "VERS", 4) != 0 || vers_resp.header.payload_len != 19)
    {
        ESP_LOGE(TAG, "Invalid response header or payload length.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "VLD1 VERSION: %s", vers_resp.version);
    return ESP_OK;
}

esp_err_t vld1::set_radar_parameters(const radar_params_t &params_struct) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "SRPS", 4);

    header.payload_len = sizeof(radar_params_t);
    send_packet(header, reinterpret_cast<const uint8_t *>(&params_struct));

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    std::memcpy(&vld1_config_, &params_struct, sizeof(radar_params_t));
    return ESP_OK;
}

esp_err_t vld1::set_distance_range(vld1_distance_range_t range) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "RRAI", 4);

    const uint8_t payload = static_cast<uint8_t>(range);
    header.payload_len = sizeof(vld1_distance_range_t);

    send_packet(header, &payload);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_threshold_offset(uint8_t val) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "THOF", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_min_range_filter(uint16_t val) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "MIRA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_max_range_filter(uint16_t val) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "MARA", 4);

    header.payload_len = sizeof(val);
    send_packet(header, reinterpret_cast<const uint8_t *>(&val));

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_target_filter(target_filter_t filter) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "TGFI", 4);

    const uint8_t payload = static_cast<uint8_t>(filter);
    header.payload_len = sizeof(target_filter_t);

    send_packet(header, &payload);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_precision_mode(precision_mode_t mode) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "PREC", 4);

    const uint8_t payload = static_cast<uint8_t>(mode);
    header.payload_len = sizeof(precision_mode_t);

    send_packet(header, &payload);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::exit_sequence() noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "GBYE", 4);

    header.payload_len = 0;
    send_packet(header, nullptr);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_chirp_integration_count(uint8_t val) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }

    vld1_header_t header{};
    std::memcpy(header.header, "INTN", 4);

    header.payload_len = sizeof(val);
    send_packet(header, &val);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_tx_power(uint8_t power) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "TXPW", 4);

    header.payload_len = sizeof(power);
    send_packet(header, &power);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t vld1::set_short_range_distance_filter(short_range_distance_t state) noexcept
{
    scoped_lock_t lock(vld1_mutex_);
    if (!lock.locked())
    {
        return ESP_ERR_TIMEOUT;
    }
    vld1_header_t header{};
    std::memcpy(header.header, "SRDF", 4);

    const uint8_t payload = static_cast<uint8_t>(state);
    header.payload_len = sizeof(short_range_distance_t);

    send_packet(header, &payload);

    if (resp_status() != vld1_error_code_t::OK)
        return ESP_FAIL;

    return ESP_OK;
}

void vld1::flush_buffer()
{
    uart_.flush_buffer();
}
