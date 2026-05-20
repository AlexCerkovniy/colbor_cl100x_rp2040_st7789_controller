#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "RP2040_PWM.h"     // Library RP2040_PWM
#include "color_temp_table.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"

#define FAN_PWM_PIN      29 //PWM6B
#define FAN_SD_PIN       28
#define YELLOW_PWM_PIN   15 //PWM7B
#define WHITE_PWM_PIN    14 //PWM7A
#define GLOBAL_PWM_PIN   27 //PWM5B 
#define CONVERTER_EN_PIN 26

#define TFT_MOSI  3
#define TFT_SCLK  2
#define TFT_CS    4  // Chip select control pin
#define TFT_DC    5  // Data Command control pin
#define TFT_RST   6  // Reset pin (could connect to Arduino RESET pin)
#define TFT_BL    7  // LED back-light

/* Define control PWM frequencies */
#define YELLOW_PWM_FREQ_HZ 30000
#define WHITE_PWM_FREQ_HZ  30000
#define GLOBAL_PWM_FREQ_HZ 20000

/* Define temp. sensor */
#define TEMP_SENSOR_RESISTANCE         10000
#define TEMP_SENSOR_PULL_UP_RESISTANCE 1000

RP2040_PWM* yellow_pwm;
RP2040_PWM* white_pwm;
RP2040_PWM* global_pwm;
RP2040_PWM* fan_pwm;

//Serial buffering
uint8_t serial_buffer[128] = {0};
uint32_t serial_buffer_size = 0;
bool serial_buffer_ready = false;

//Control
int color_temp_index = 0;
bool converter_enabled = false;
int white_brightness, white_brightness_last = 0;
int yellow_brightness, yellow_brightness_last = 0;
int global_brightness, global_brightness_last = 0;
int fan_speed, fan_speed_last = 0;

//Stats
int supply_voltage_mv = 0;
int supply_current_ma = 0;

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(CONVERTER_EN_PIN, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(CONVERTER_EN_PIN, false);

  /* Initialize PWM (crutch - after init pwm set up to 29% not applied somehow)*/
  yellow_pwm = new RP2040_PWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 0);
  yellow_pwm->setPWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 30);
  yellow_pwm->setPWM(YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, 0);

  white_pwm = new RP2040_PWM(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 0);
  white_pwm->setPWM_Int(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 30);
  white_pwm->setPWM_Int(WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, 0);

  global_pwm = new RP2040_PWM(GLOBAL_PWM_PIN, GLOBAL_PWM_FREQ_HZ, 0);
  global_pwm->setPWM_Int(GLOBAL_PWM_PIN, GLOBAL_PWM_FREQ_HZ, 30);
  global_pwm->setPWM_Int(GLOBAL_PWM_PIN, GLOBAL_PWM_FREQ_HZ, 0);

  fan_pwm = new RP2040_PWM(FAN_PWM_PIN, 25000, 100);

  SPI.setTX(TFT_MOSI);
  SPI.setSCK(TFT_SCLK);
  SPI.begin();
  tft.init(170, 320);
  tft.setRotation(1);
  tft.fillScreen(0);
  digitalWrite(TFT_BL, true);
  startup_screen();
}

void loop() {
  uint8_t serial_byte;

  while(Serial.available()) {
    serial_byte = Serial.read();
    serial_buffer[serial_buffer_size] = serial_byte;
    serial_buffer_size++;

    if(serial_byte == '\n' || serial_byte == '\r'){
      serial_buffer_ready = true;
      Serial.flush();
      break;
    }
  }

  if(serial_buffer_ready) {
    if(serial_buffer[0] == 'c') {
      converter_enabled ^= true;
      digitalWrite(CONVERTER_EN_PIN, converter_enabled);

      if(converter_enabled) {
        Serial.println("Converter enabled");
      }
      else {
        Serial.println("Converter disabled");
      }
    }
    else if(serial_buffer[0] == 'y') {
      yellow_brightness = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
      Serial.print("Set yellow brightness to ");
      Serial.println(yellow_brightness);
    }
    else if(serial_buffer[0] == 'w') {
      white_brightness = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
      Serial.print("Set white brightness to ");
      Serial.println(white_brightness);
    }
    else if(serial_buffer[0] == 'g') {
      global_brightness = map(atoi((char *)&serial_buffer[1]), 0, 100, 10, 100);
      Serial.print("Set global brightness to ");
      Serial.println(global_brightness);
    }
    else if(serial_buffer[0] == 'f') {
      fan_speed = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
      Serial.print("Set fan speed to ");
      Serial.println(fan_speed);
    }
    else {
      Serial.println("Unknown data");
    }

    serial_buffer_ready = false;
    serial_buffer_size = 0;
  }

  pwm_update(yellow_pwm, YELLOW_PWM_PIN, YELLOW_PWM_FREQ_HZ, yellow_brightness, &yellow_brightness_last);
  pwm_update(white_pwm, WHITE_PWM_PIN, WHITE_PWM_FREQ_HZ, white_brightness, &white_brightness_last);
  pwm_update(global_pwm, GLOBAL_PWM_PIN, GLOBAL_PWM_FREQ_HZ, global_brightness, &global_brightness_last);
  pwm_update(fan_pwm, FAN_PWM_PIN, 25000, 100 - fan_speed, &fan_speed_last);
}

void pwm_update(RP2040_PWM *pwm, int pin, int freq_hz, int new_duty, int *last_duty) {
  if(new_duty != *last_duty) {
    *last_duty = new_duty;
    pwm->setPWM_Int(pin, freq_hz, new_duty * 1000);
    Serial.print("PWM updated for pin ");
    Serial.println(pin);
  }
}

void startup_screen(void) {
  tft.setFont(&FreeMonoBold9pt7b);
  tft.fillRect(0, 0, tft.width(), 20, 0xFFFF);
  tft.setCursor(5, 14);
  tft.setTextColor(0);
  tft.printf("%.2d.%.2dV", supply_voltage_mv/1000, (supply_voltage_mv%1000)/10);

  // Dimm percents
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 75);
  tft.printf("%d%%", global_brightness);

  // Color temp
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 130);
  tft.printf("%dK", color_temp_table[color_temp_index]);
}
