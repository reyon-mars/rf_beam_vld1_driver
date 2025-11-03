#include "averager.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

batch_averager::batch_averager(size_t batch_size,
                               double max_step,
                               double hampel_thresh,
                               double trim_fraction) noexcept
    : batch_size_(batch_size),
      count_(0),
      curr_avg_m_(0.0),
      max_step_(max_step),
      hampel_thresh_(hampel_thresh),
      trim_fraction_(trim_fraction),
      head_(0),
      full_(false)
{
    samples_.resize(batch_size_, 0.0); // Preallocate storage
}

void batch_averager::add_sample(double meters) noexcept
{
    if (!std::isfinite(meters))
        return;

    // Step-change limiter
    if (is_complete())
    {
        size_t last_idx = (head_ + batch_size_ - 1) % batch_size_;
        double last = samples_[last_idx];
        if (std::fabs(meters - last) > max_step_)
            return;
    }

    // Insert sample in O(1) into circular buffer
    samples_[head_] = meters;
    head_ = (head_ + 1) % batch_size_;
    if (head_ == 0)
        full_ = true;
    ++count_;

    if (is_complete())
    {
        // Reconstruct chronological order for filtering
        std::vector<double> data;
        data.reserve(batch_size_);
        if (full_)
        {
            for (size_t i = head_; i < batch_size_; ++i)
                data.push_back(samples_[i]);
            for (size_t i = 0; i < head_; ++i)
                data.push_back(samples_[i]);
        }
        else
        {
            data.insert(data.end(), samples_.begin(), samples_.begin() + head_);
        }

        // Apply Hampel filter and trimmed mean
        auto filtered = hampel_filter(data, hampel_thresh_);
        curr_avg_m_ = compute_trimmed_mean(filtered, trim_fraction_);
    }
}

bool batch_averager::is_complete() const noexcept
{
    return full_ || head_ >= batch_size_;
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
    std::fill(samples_.begin(), samples_.end(), 0.0);
    head_ = 0;
    full_ = false;
}

//----------------------------------------------
// Trimmed mean computation
//----------------------------------------------
double batch_averager::compute_trimmed_mean(std::vector<double> data, double trimFrac)
{
    if (data.empty())
        return NAN;

    std::sort(data.begin(), data.end());
    size_t trim = static_cast<size_t>(trimFrac * data.size());
    size_t start = trim;
    size_t end = data.size() - trim;

    if (end <= start)
        return data[data.size() / 2]; // fallback: median

    double sum = std::accumulate(data.begin() + start, data.begin() + end, 0.0);
    return sum / static_cast<double>(end - start);
}

//----------------------------------------------
// Hampel Filter Implementation
//----------------------------------------------
std::vector<double> batch_averager::hampel_filter(const std::vector<double> &data, double threshold)
{
    if (data.empty())
        return {};

    std::vector<double> filtered;
    filtered.reserve(data.size());

    // Compute median
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    double median = sorted[sorted.size() / 2];

    // Compute MAD
    std::vector<double> abs_dev;
    abs_dev.reserve(data.size());
    for (double x : data)
        abs_dev.push_back(std::fabs(x - median));
    std::sort(abs_dev.begin(), abs_dev.end());
    double mad = abs_dev[abs_dev.size() / 2];

    if (mad == 0.0)
        return data; // all values identical

    // Scale factor for Gaussian equivalence
    constexpr double k = 1.4826;
    double scaled_mad = k * mad;

    // Reject outliers
    for (double x : data)
    {
        if (std::fabs(x - median) <= threshold * scaled_mad)
            filtered.push_back(x);
    }

    return filtered;
}
