#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cmath>

class batch_averager
{
public:
    // Constructor
    explicit batch_averager(size_t batch_size = 20,
                            double max_step = 0.05,     // meters: max plausible jump
                            double hampel_thresh = 3.0, // threshold in MAD units
                            double trim_fraction = 0.1) noexcept;

    // Add a new sample (meters)
    void add_sample(double meters) noexcept;

    // Query if the buffer is full
    bool is_complete() const noexcept;

    // Get current averaged value (in meters)
    double average_meters() const noexcept;

    // Get averaged value converted to millimeters (clamped to uint16_t)
    uint16_t average_millimeters() const noexcept;

    // Reset the averager to its initial state
    void reset() noexcept;

private:
    //------------------------------------------
    // Core configuration parameters
    //------------------------------------------
    size_t batch_size_;    // Total capacity of the ring buffer
    size_t count_;         // Total samples processed
    double curr_avg_m_;    // Latest computed average in meters
    double max_step_;      // Max physical jump allowed between samples
    double hampel_thresh_; // Hampel outlier threshold (in MAD units)
    double trim_fraction_; // Fraction to trim from both ends for trimmed mean

    //------------------------------------------
    // Ring buffer data members
    //------------------------------------------
    std::vector<double> samples_; // Circular buffer storage
    size_t head_;                 // Next write index in the ring
    bool full_;                   // Flag indicating buffer has wrapped

    //------------------------------------------
    // Internal helpers
    //------------------------------------------
    static double compute_trimmed_mean(std::vector<double> data,
                                       double trimFrac);

    static std::vector<double> hampel_filter(const std::vector<double> &data,
                                             double threshold);
};
