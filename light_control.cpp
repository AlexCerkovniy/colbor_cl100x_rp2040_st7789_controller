#include "light_control.h"
#include "light_config.h"
#include "color_temp_table.h"

static void converter_en(void) {
    #define PWM_TIME_US           (10000)
    #define PWM_PERIOD_US         (500)
    #define PWM_DUTY_MAX          (45)
    #define PWM_DUTY_MIN          (10)

    uint32_t active_period_us = 0;

    /* Pre-charge step for soft-start capacitor */
    uint32_t cycle = 2;
    digitalWrite(CONVERTER_EN_PIN, true);
    while(cycle--) {
        delayMicroseconds(100);
    }

    /* Feed inverted PWM pulses on pin */
    cycle = PWM_TIME_US / PWM_PERIOD_US;
    while(cycle) {
        active_period_us = ((PWM_PERIOD_US * cycle) / (PWM_TIME_US / PWM_PERIOD_US)) * PWM_DUTY_MAX / 100;
        if(active_period_us < (PWM_PERIOD_US * PWM_DUTY_MIN / 100)) {
          active_period_us = (PWM_PERIOD_US * PWM_DUTY_MIN / 100);
        }

        digitalWrite(CONVERTER_EN_PIN, true);
        delayMicroseconds(active_period_us);
        digitalWrite(CONVERTER_EN_PIN, false);
        delayMicroseconds(PWM_PERIOD_US - active_period_us);
        cycle--;
    }

    /* Some few final fixed duty cycle PWM pulses  */
    cycle = 6;
    active_period_us = (PWM_PERIOD_US * PWM_DUTY_MIN / 100);

    while(cycle) {
        digitalWrite(CONVERTER_EN_PIN, true);
        delayMicroseconds(active_period_us);
        digitalWrite(CONVERTER_EN_PIN, false);
        delayMicroseconds(PWM_PERIOD_US - active_period_us);
        cycle--;
    }

    /* Finally enable domain */
    digitalWrite(CONVERTER_EN_PIN, true);
}

// Public Methods //////////////////////////////////////////////////////////////

void LightControl::begin(void) {
  pinMode(CONVERTER_EN_PIN, OUTPUT);
  digitalWrite(CONVERTER_EN_PIN, false);

  /* Initialize PWM (crutch - after init pwm set up to 29% not applied somehow)*/
  for(uint32_t pwm_param = LIGHT_YELLOW_PWM; pwm_param < LIGHT_FAN_PWM; pwm_param++) {
    this->pwm_instance[pwm_param].pwm = new RP2040_PWM(this->pwm_instance[pwm_param].pin, this->pwm_instance[pwm_param].freq, 0);
    this->pwm_instance[pwm_param].pwm->setPWM(this->pwm_instance[pwm_param].pin, this->pwm_instance[pwm_param].freq, 30);
    this->pwm_instance[pwm_param].pwm->setPWM(this->pwm_instance[pwm_param].pin, this->pwm_instance[pwm_param].freq, 0);
  }

  this->pwm_instance[LIGHT_FAN_PWM].pwm = new RP2040_PWM(this->pwm_instance[LIGHT_FAN_PWM].pin, this->pwm_instance[LIGHT_FAN_PWM].freq, 40);
}

void LightControl::tick(void) {
  const char *pwm_param_string[] = {
    "LIGHT_YELLOW_PWM",
    "LIGHT_WHITE_PWM",
    "LIGHT_DIMM_PWM",
    "LIGHT_FAN_PWM"
  };

  uint32_t level_to_set = 0;

  if(millis() - this->level_follow_timer >= TARGET_LEVEL_FOLLOW_STEP_MS) {
    this->level_follow_timer = millis();

    for(uint32_t pwm_param = LIGHT_YELLOW_PWM; pwm_param < LIGHT_PWM_COUNT; pwm_param++) {
      if(this->pwm_instance[pwm_param].level_last < this->pwm_instance[pwm_param].level) {
        if(this->pwm_instance[pwm_param].level_last < 100) {
          this->pwm_instance[pwm_param].level_last += 1;
        }
      }
      else if(this->pwm_instance[pwm_param].level_last > this->pwm_instance[pwm_param].level) {
        if(this->pwm_instance[pwm_param].level_last) {
          this->pwm_instance[pwm_param].level_last -= 1;
        }
      }
      else {
        continue;
      }

      level_to_set = this->pwm_instance[pwm_param].level_last;
      if(pwm_param == LIGHT_DIMM_PWM) {
        level_to_set = map(level_to_set, 0, 100, 10, 100);
      }

      this->pwm_instance[pwm_param].pwm->setPWM_Int(this->pwm_instance[pwm_param].pin, this->pwm_instance[pwm_param].freq, level_to_set * 1000);
      Serial.printf("Update PWM for %s to %lu%%\r\n", pwm_param_string[pwm_param], level_to_set);
    }
  }
}

void LightControl::enable(bool state) {
  this->enabled = state;

  if(this->enabled) {
    converter_en();
    Serial.println("Converter enabled");
  }
  else {
    digitalWrite(CONVERTER_EN_PIN, false);
    Serial.println("Converter disabled");
  }
}

bool LightControl::is_enabled(void) {
  return this->enabled;
}

void LightControl::set_yellow(int level) {
  this->pwm_instance[LIGHT_YELLOW_PWM].level = constrain(level, 0, 100);
  Serial.print("Set yellow level to ");
  Serial.println(this->pwm_instance[LIGHT_YELLOW_PWM].level);
}

int LightControl::get_yellow(void) {
  return this->pwm_instance[LIGHT_YELLOW_PWM].level;
}

void LightControl::set_white(int level) {
  this->pwm_instance[LIGHT_WHITE_PWM].level = constrain(level, 0, 100);
  Serial.print("Set white level to ");
  Serial.println(this->pwm_instance[LIGHT_WHITE_PWM].level);
}

int LightControl::get_white(void) {
  return this->pwm_instance[LIGHT_WHITE_PWM].level;
}

void LightControl::set_dimm(int level) {
  this->pwm_instance[LIGHT_DIMM_PWM].level = constrain(level, 0, 100);
  Serial.print("Set dimm. level to ");
  Serial.println(this->pwm_instance[LIGHT_DIMM_PWM].level);
}

int LightControl::get_dimm(void) {
  return this->pwm_instance[LIGHT_DIMM_PWM].level;
}

void LightControl::set_fan_speed(int level) {
  this->pwm_instance[LIGHT_FAN_PWM].level = 100 - constrain(level, 0, 100);
  Serial.print("Set fan speed to ");
  Serial.println(100 - this->pwm_instance[LIGHT_FAN_PWM].level);
}

int LightControl::get_fan_speed(void) {
  return 100 - this->pwm_instance[LIGHT_FAN_PWM].level;
}

void LightControl::set_color_temperature(int kelvin) {
  this->color_temp = constrain(kelvin, 2700, 6500);
  int index = (this->color_temp - 2700) / 100;
  this->set_white(color_temp_table[index].white_level);
  this->set_yellow(color_temp_table[index].yellow_level);
  Serial.print("Set color temp. to ");
  Serial.print(this->color_temp);
  Serial.println("K");
}

int LightControl::get_color_temperature(void) {
  return this->color_temp;
}
