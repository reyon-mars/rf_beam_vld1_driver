#include "averager.hpp"

batch_averager::batch_averager(size_t batch_size) noexcept
    : batch_size_(batch_size), count_(0), curr_avg_m_(0.0) {}

void batch_averager::add_sample(double meters) noexcept
{
    ++count_;
    curr_avg_m_ += (meters - curr_avg_m_) / static_cast<double>(count_);
}

bool batch_averager::is_complete() const noexcept
{
    return count_ >= batch_size_;
}

double batch_averager::average_meters() const noexcept
{
    return curr_avg_m_;
}

uint16_t batch_averager::average_millimeters() const noexcept
{
    double mm = curr_avg_m_ * 1000.0;
    if (mm < 0)
        return 0;
    if (mm > 65535.0)
        return 0xFFFF;
    return static_cast<uint16_t>(mm + 0.5);
}

void batch_averager::reset() noexcept
{
    count_ = 0;
    curr_avg_m_ = 0.0;
}
