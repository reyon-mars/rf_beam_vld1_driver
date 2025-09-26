#include "averager.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

batch_averager::batch_averager(size_t batch_size,
                               double max_step,
                               double trim_fraction) noexcept
    : batch_size_(batch_size),
      count_(0),
      curr_avg_m_(0.0),
      max_step_(max_step),
      trim_fraction_(trim_fraction)
{
    samples_.reserve(batch_size_);
}

void batch_averager::add_sample(double meters) noexcept
{
    if (!std::isfinite(meters))
        return;

    // Step-change limiter: reject unrealistic jumps
    if (!samples_.empty())
    {
        double last = samples_.back();
        if (std::fabs(meters - last) > max_step_)
        {
            return; // discard outlier
        }
    }

    // Append sample
    if (samples_.size() == batch_size_)
        samples_.erase(samples_.begin());
    samples_.push_back(meters);

    ++count_;

    // Update robust average when we have a full batch
    if (is_complete())
    {
        curr_avg_m_ = compute_trimmed_mean(samples_, trim_fraction_);
    }
}

bool batch_averager::is_complete() const noexcept
{
    return samples_.size() >= batch_size_;
}

double batch_averager::average_meters() const noexcept
{
    return curr_avg_m_;
}

uint16_t batch_averager::average_millimeters() const noexcept
{
    double mm = curr_avg_m_ * 1000.0;
    if (mm < 0.0)
        return 0;
    if (mm > 65535.0)
        return 0xFFFF;
    return static_cast<uint16_t>(mm + 0.5);
}

void batch_averager::reset() noexcept
{
    count_ = 0;
    curr_avg_m_ = 0.0;
    samples_.clear();
}

double batch_averager::compute_trimmed_mean(std::vector<double> data, double trimFrac)
{
    if (data.empty())
        return NAN;

    std::sort(data.begin(), data.end());
    size_t trim = static_cast<size_t>(trimFrac * data.size());
    size_t start = trim;
    size_t end = data.size() - trim;

    if (end <= start)
    {
        // Fallback to median
        return data[data.size() / 2];
    }

    double sum = std::accumulate(data.begin() + start, data.begin() + end, 0.0);
    return sum / static_cast<double>(end - start);
}
