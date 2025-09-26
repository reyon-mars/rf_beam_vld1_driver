#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

class batch_averager
{
public:
    explicit batch_averager(size_t batch_size = 20,
                            double max_step = 0.05, // meters, max plausible jump between samples
                            double trim_fraction = 0.1) noexcept;

    // Add a new sample (in meters). Outliers may be rejected internally.
    void add_sample(double meters) noexcept;

    // Returns true once at least batch_size samples have been accepted
    bool is_complete() const noexcept;

    // Robust average of the current batch, in meters
    double average_meters() const noexcept;

    // Robust average in millimeters (clamped to [0, 65535])
    uint16_t average_millimeters() const noexcept;

    // Reset state for a new batch
    void reset() noexcept;

private:
    size_t batch_size_; // number of samples in one batch
    size_t count_;      // how many samples have been seen (accepted)
    double curr_avg_m_; // cached robust average in meters

    double max_step_;             // maximum plausible change between consecutive samples
    double trim_fraction_;        // fraction of samples trimmed from each end
    std::vector<double> samples_; // buffer for current batch

    static double compute_trimmed_mean(std::vector<double> data, double trimFrac);
};
