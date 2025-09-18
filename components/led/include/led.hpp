#pragma once
#include "driver/gpio.h"

class led
{
public:
    explicit led(gpio_num_t led_pin) noexcept;
    void init() noexcept;
    void blink(int times, int delay_ms) noexcept;
    void set(bool on) noexcept;

private:
    gpio_num_t pin_;
};
