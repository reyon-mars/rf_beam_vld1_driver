#pragma once
#include <cstdint>
#include <cstddef>

class batch_averager
{
public:
    explicit batch_averager(size_t batch_size = 20) noexcept;

    void add_sample(double meters) noexcept;

    bool is_complete() const noexcept;

    double average_meters() const noexcept;

    uint16_t average_millimeters() const noexcept;

    void reset() noexcept;

private:
    size_t batch_size_;
    size_t count_;
    double curr_avg_m_;
};
