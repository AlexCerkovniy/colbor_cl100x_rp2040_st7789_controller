#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pin_config.h"
#include "color_temp_table.h"
#include "light_control.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"
#include "button.h"
#include "RPi_Pico_TimerInterrupt.h"

#define TIMER0_INTERVAL_MS (1)
RPI_PICO_Timer ITimer0(0);
bool TimerHandler0(struct repeating_timer *t);

#define BUTTONS_COUNT                 (6)
#define BUTTONS_HOLD_PRESS_PERIOD_MS  (150)

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

#define TFT_SPI_SPEED_HZ (50000000UL)

/* Off-screen buffers used for flicker-free partial updates */
#define VOLT_X 0
#define VOLT_Y 0
#define VOLT_W 120
#define VOLT_H 20
#define FIELD_X 100
#define FIELD_W 220
#define FIELD_H 48
#define DIMM_Y  40  // baseline in field = 75 - 40
#define TEMP_Y  95  // baseline in field = 130 - 95
#define FIELD_BASELINE 35

GFXcanvas16 volt_canvas(VOLT_W, VOLT_H);
GFXcanvas16 field_canvas(FIELD_W, FIELD_H);

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Init(&buttons[btn].button, buttons[btn].button_id, 50, 500, BUTTONS_HOLD_PRESS_PERIOD_MS);
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
  // Library default is 32 MHz, but RP2040 SPI clock = CPU clock / even divider,
  // so at 200 MHz CPU it really runs at 25 MHz. 200/4 = exactly 50 MHz.
  // If the picture gets corrupted, try 33000000 or 25000000.
  tft.setSPISpeed(TFT_SPI_SPEED_HZ);
  tft.setRotation(3); // 180 degrees from the previous rotation 1
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

/* Render text into the canvas and push only that rectangle to the display */
static void draw_field(int16_t x, int16_t y, const char *txt) {
  field_canvas.fillScreen(0);
  field_canvas.setFont(&FreeMonoBold24pt7b);
  field_canvas.setTextColor(0xFFFF);
  field_canvas.setCursor(0, FIELD_BASELINE);
  field_canvas.print(txt);
  tft.drawRGBBitmap(x, y, field_canvas.getBuffer(), FIELD_W, FIELD_H);
}

/* Partial update: only fields whose value changed are redrawn */
void main_screen(void) {
  static bool first_draw = true;
  static int last_voltage_mv = -1;
  static int last_dimm = -1;
  static int last_temp = -1;

  if(first_draw) {
    first_draw = false;
    tft.fillScreen(0);
    // White top bar, static
    tft.fillRect(0, 0, tft.width(), VOLT_H, 0xFFFF);
  }

  if(supply_voltage_mv != last_voltage_mv) {
    last_voltage_mv = supply_voltage_mv;
    volt_canvas.fillScreen(0xFFFF);
    volt_canvas.setFont(&FreeMonoBold9pt7b);
    volt_canvas.setTextColor(0);
    volt_canvas.setCursor(5, 14);
    volt_canvas.printf("%.2d.%.2dV", supply_voltage_mv/1000, (supply_voltage_mv%1000)/10);
    tft.drawRGBBitmap(VOLT_X, VOLT_Y, volt_canvas.getBuffer(), VOLT_W, VOLT_H);
  }

  int dimm = light.get_dimm();
  if(dimm != last_dimm) {
    last_dimm = dimm;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", dimm);
    draw_field(FIELD_X, DIMM_Y, buf);
  }

  int temp = light.get_color_temperature();
  if(temp != last_temp) {
    last_temp = temp;
    char buf[16];
    snprintf(buf, sizeof(buf), "%dK", temp);
    draw_field(FIELD_X, TEMP_Y, buf);
  }
}

static void button_callback(uint8_t button_id, button_callback_event_t event) {
  const char *event_string[] = {
    "BTN_STATE_UPDATED",
    "BTN_SHORT_PRESS",
    "BTN_LONG_PRESS",
    "BTN_HOLD_PRESS"
  };

  Serial.printf("Button ID=%d event %s\r\n", button_id, event_string[event]);

  if(event == BTN_SHORT_PRESS || event == BTN_HOLD_PRESS) {
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
