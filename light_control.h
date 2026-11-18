#pragma once

#include <Arduino.h>
#include "RP2040_PWM.h"     // Library RP2040_PWM
#include "light_config.h"

typedef enum {
  LIGHT_YELLOW_PWM = 0,
  LIGHT_WHITE_PWM,
  LIGHT_DIMM_PWM,
  LIGHT_FAN_PWM,

  LIGHT_PWM_COUNT
} pwm_param_t;

typedef struct {
  RP2040_PWM* pwm;
  uint8_t pin;
  float freq;
  uint32_t level;
  uint32_t level_last;
} pwm_instance_t;

class LightControl {
  private:
    pwm_instance_t pwm_instance[LIGHT_PWM_COUNT] = {
      {.pwm = NULL, .pin = YELLOW_PWM_PIN, .freq = YELLOW_PWM_FREQ_HZ, .level = 0, .level_last = 0},
      {.pwm = NULL, .pin = WHITE_PWM_PIN, .freq = WHITE_PWM_FREQ_HZ, .level = 0, .level_last = 0},
      {.pwm = NULL, .pin = DIMM_PWM_PIN, .freq = DIMM_PWM_FREQ_HZ, .level = 0, .level_last = 0},
      {.pwm = NULL, .pin = FAN_PWM_PIN, .freq = 25000, .level = 0, .level_last = 0}
    };

    bool enabled = false;
    int color_temp = 0;
    uint32_t level_follow_timer = 0;

  public:
    void begin(void);
    void tick(void);
    void enable(bool state);
    bool is_enabled(void);
    void set_yellow(int level);
    int get_yellow(void);
    void set_white(int level);
    int get_white(void);
    void set_dimm(int level);
    int get_dimm(void);
    void set_fan_speed(int level);
    int get_fan_speed(void);
    void set_color_temperature(int kelvin);
    int get_color_temperature(void);
};
