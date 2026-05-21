#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pin_config.h"
#include "color_temp_table.h"
#include "light_control.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"
#include "button.h"
#include "TimerInterrupt_Generic.h"

#define TIMER0_INTERVAL_MS (1)
RPI_PICO_Timer ITimer0(0);
bool TimerHandler0(struct repeating_timer *t);

#define BUTTONS_COUNT 6

static struct {
  uint16_t button_id;
	button_t button;
} buttons[] = {
  {.button_id = SW1_UP},
  {.button_id = SW1_DOWN},
  {.button_id = SW1_PRESS},
  {.button_id = SW2_UP},
  {.button_id = SW2_DOWN},
  {.button_id = SW2_PRESS}
};

static void button_callback(uint8_t button_id, button_callback_event_t event);

/* Define temp. sensor */
#define TEMP_SENSOR_RESISTANCE         10000
#define TEMP_SENSOR_PULL_UP_RESISTANCE 1000

//Serial buffering
uint8_t serial_buffer[128] = {0};
uint32_t serial_buffer_size = 0;
bool serial_buffer_ready = false;

LightControl light = LightControl();

//Stats
int supply_voltage_mv = 0;
int supply_current_ma = 0;

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
bool screen_update = true;

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Init(&buttons[btn].button, buttons[btn].button_id, 50, 0 /* Disabled */);
		BTN_RegisterCallback(&buttons[btn].button, button_callback);
	}

  ITimer0.attachInterruptInterval(TIMER0_INTERVAL_MS * 1000, TimerHandler0);

  light.begin();
  light.set_color_temperature(5600);
  light.set_fan_speed(61);

  SPI.setTX(TFT_MOSI);
  SPI.setSCK(TFT_SCLK);
  SPI.begin();
  tft.init(170, 320);
  tft.setRotation(1);
  tft.fillScreen(0);
  delay(100);
  digitalWrite(TFT_BL, true);
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
      light.enable(light.is_enabled() ^ true);
      if(!light.is_enabled()) {
        light.set_fan_speed(60);
      }
    }
    else if(serial_buffer[0] == 'y') {
      light.set_yellow(atoi((char *)&serial_buffer[1]));
    }
    else if(serial_buffer[0] == 'w') {
      light.set_white(atoi((char *)&serial_buffer[1]));
    }
    else if(serial_buffer[0] == 'g') {
      light.set_dimm(atoi((char *)&serial_buffer[1]));
    }
    else if(serial_buffer[0] == 'f') {
      light.set_fan_speed(atoi((char *)&serial_buffer[1]));
    }
    else {
      Serial.println("Unknown data");
    }

    serial_buffer_ready = false;
    serial_buffer_size = 0;
    screen_update = true;
  }

  light.tick();

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Main(&buttons[btn].button);
	}

  if(screen_update) {
    screen_update = false;
    main_screen();
  }
}

void main_screen(void) {
  tft.fillScreen(0);
  tft.setFont(&FreeMonoBold9pt7b);
  tft.fillRect(0, 0, tft.width(), 20, 0xFFFF);
  tft.setCursor(5, 14);
  tft.setTextColor(0);
  tft.printf("%.2d.%.2dV", supply_voltage_mv/1000, (supply_voltage_mv%1000)/10);

  // Dimm percents
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 75);
  tft.printf("%d%%", light.get_dimm());

  // Color temp
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 130);
  tft.printf("%dK", light.get_color_temperature());
}

static void button_callback(uint8_t button_id, button_callback_event_t event) {
  Serial.printf("Button ID =%d event = %d\r\n", button_id, event);

  if(event == BTN_SHORT_PRESS) {
    if(button_id == SW1_UP) {
      light.set_dimm(light.get_dimm() + 5);
      if(light.is_enabled()) {
        light.set_fan_speed(map(light.get_dimm(), 0, 100, 60, 100));
      }
    }
    else if(button_id == SW1_DOWN) {
      if(!(light.get_dimm() < 5)) {
        light.set_dimm(light.get_dimm() - 5);
        if(light.is_enabled()) {
          light.set_fan_speed(map(light.get_dimm(), 0, 100, 60, 100));
        }
      }
    }
    else if(button_id == SW1_PRESS) {
      light.enable(light.is_enabled() ^ true);
      if(light.is_enabled()) {
        light.set_fan_speed(map(light.get_dimm(), 0, 100, 60, 100));
      }
      else {
        light.set_fan_speed(60);
      }
    }
    else if(button_id == SW2_UP) {
      light.set_color_temperature(light.get_color_temperature() + 100);
    }
    else if(button_id == SW2_DOWN) {
      light.set_color_temperature(light.get_color_temperature() - 100);
    }

    screen_update = true;
  }
}

bool TimerHandler0(struct repeating_timer *t)
{
	(void) t;
  /* Update timings for buttons library */
	for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Tick(&buttons[btn].button, 1);
	}
	return true;
}
