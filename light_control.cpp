#include "light_control.h"
#include "light_config.h"

static void pwm_update(RP2040_PWM *pwm, int pin, int freq_hz, int new_duty, int *last_duty) {
  if(new_duty != *last_duty) {
    *last_duty = new_duty;
    pwm->setPWM_Int(pin, freq_hz, new_duty * 1000);
    Serial.print("PWM updated for pin ");
    Serial.println(pin);
  }
}

// Public Methods //////////////////////////////////////////////////////////////

void LightControl::begin(void) {
  pinMode(CONVERTER_EN_PIN, OUTPUT);
  digitalWrite(CONVERTER_EN_PIN, false);

  /* Initialize PWM (crutch - after init pwm set up to 29% not applied somehow)*/
  this->yellow_pwm = new RP2040_PWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 0);
  this->yellow_pwm->setPWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 30);
  this->yellow_pwm->setPWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 0);

  this->white_pwm = new RP2040_PWM(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 0);
  this->white_pwm->setPWM_Int(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 30);
  this->white_pwm->setPWM_Int(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 0);

  this->dimm_pwm = new RP2040_PWM(DIMM_PWM_PIN, DIMM_PWM_FREQ_HZ, 0);
  this->dimm_pwm->setPWM_Int(DIMM_PWM_PIN, DIMM_PWM_FREQ_HZ, 30);
  this->dimm_pwm->setPWM_Int(DIMM_PWM_PIN, DIMM_PWM_FREQ_HZ, 0);

  fan_pwm = new RP2040_PWM(FAN_PWM_PIN, 25000, 100);
}

void LightControl::tick(void) {
  pwm_update(this->yellow_pwm, YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, this->yellow_level, &this->yellow_level_last);
  pwm_update(this->white_pwm, WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, this->white_level, &this->white_level_last);
  pwm_update(this->dimm_pwm, DIMM_PWM_PIN, DIMM_PWM_FREQ_HZ, this->dimm_level, &this->dimm_level_last);
  pwm_update(this->fan_pwm, FAN_PWM_PIN, 25000, 100 - this->fan_speed, &this->fan_speed_last);
}

void LightControl::enable(bool state) {
  this->enabled = state;

  if(state) {
    Serial.println("Converter enabled");
  }
  else {
    Serial.println("Converter disabled");
  }
}

void LightControl::set_yellow(int level) {
  this->yellow_level = level;
  Serial.print("Set yellow level to ");
  Serial.println(this->yellow_level);
}

int LightControl::get_yellow(void) {
  return this->yellow_level;
}

void LightControl::set_white(int level) {
  this->white_level = level;
  Serial.print("Set white level to ");
  Serial.println(this->white_level);
}

int LightControl::get_white(void) {
  return this->white_level;
}

void LightControl::set_dimm(int level) {
  this->dimm_level = level;
  Serial.print("Set dimm. level to ");
  Serial.println(this->dimm_level);
}

int LightControl::get_dimm(void) {
  return this->dimm_level;
}

void LightControl::set_fan_speed(int level) {
  this->fan_speed = level;
  Serial.print("Set fan speed to ");
  Serial.println(this->fan_speed);
}

int LightControl::get_fan_speed(void) {
  return this->fan_speed;
}