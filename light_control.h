#pragma once

#include <Arduino.h>
#include "RP2040_PWM.h"     // Library RP2040_PWM

class LightControl {
  private:
    RP2040_PWM* yellow_pwm;
    RP2040_PWM* white_pwm;
    RP2040_PWM* dimm_pwm;
    RP2040_PWM* fan_pwm;
    bool enabled = false;
    int white_level, white_level_last = 0;
    int yellow_level, yellow_level_last = 0;
    int dimm_level, dimm_level_last = 0;
    int fan_speed, fan_speed_last = 100;
    int color_temp_index = 0;

  public:
    void begin(void);
    void tick(void);
    void enable(bool state);
    void toggle(void);
    void set_yellow(int level);
    int get_yellow(void);
    void set_white(int level);
    int get_white(void);
    void set_dimm(int level);
    int get_dimm(void);
    void set_fan_speed(int level);
    int get_fan_speed(void);
};
