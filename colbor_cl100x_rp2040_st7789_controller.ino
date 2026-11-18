#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pin_config.h"
#include "color_temp_table.h"
#include "light_control.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold12pt7b.h"
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
/* Divider: 3V3 -> 1k -> TEMP_SENSOR_PIN <- NTC 10k <- GND (ADC ref is also 3V3) */
#define TEMP_SENSOR_BETA               3950.0f  // NTC Beta value, check datasheet of your NTC
#define TEMP_SENSOR_NOMINAL_C          25.0f    // temperature where NTC = TEMP_SENSOR_RESISTANCE
#define TEMP_SENSOR_ADC_BITS           12
#define TEMP_SENSOR_SAMPLES            16
#define TEMP_SENSOR_PERIOD_MS          500

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

#define TFT_SPI_SPEED_HZ (80000000UL)

/* Colors (RGB565) */
#define RGB565(r, g, b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_BG         0x0000
#define COLOR_BLOCK      RGB565(44, 48, 56)    // slightly grey block background
#define COLOR_LABEL      RGB565(160, 168, 180)
#define COLOR_TRACK      RGB565(22, 24, 30)    // progress bar track
#define COLOR_DIMM_FILL  RGB565(255, 200, 80)
#define COLOR_ON         RGB565(30, 170, 70)   // light on indication
#define COLOR_OFF_TEXT   RGB565(120, 126, 136)

/* Layout (320x170 landscape): 4 px margins/gaps, 3 blocks fill the full height
 * 4 + 56 + 4 + 56 + 4 + 42 + 4 = 170 */
#define BLOCK_X          4
#define BLOCK_W          312
#define BLOCK_H          56
#define DIMM_BLOCK_Y     4
#define TEMP_BLOCK_Y     64
#define STATUS_BLOCK_Y   124
#define STATUS_BLOCK_H   42
/* Bottom row: three equal blocks (ON/OFF, temperature, power): 3*100 + 2*6 = 312 */
#define SMALL_BLOCK_W    100
#define SMALL_BLOCK_GAP  6
#define POWER_BLOCK_X    (BLOCK_X)
#define TEMP_C_BLOCK_X   (BLOCK_X + SMALL_BLOCK_W + SMALL_BLOCK_GAP)
#define STATUS_BLOCK_X   (BLOCK_X + 2 * (SMALL_BLOCK_W + SMALL_BLOCK_GAP))
#define TEMP_C_MAX       99
#define POWER_W_MAX      150
#define BLOCK_RADIUS     8
#define BLOCK_PAD        10
#define LABEL_BASELINE   24
#define VALUE_BASELINE   44

/* Progress bar inside a block (canvas coordinates) */
#define BAR_X            BLOCK_PAD
#define BAR_Y            34
#define BAR_W            112
#define BAR_H            8

#define TEMP_MIN_K       2700
#define TEMP_MAX_K       6500

/* Off-screen buffers used for flicker-free partial updates.
 * block_canvas is shared by all blocks (status block uses its first 32 rows). */
GFXcanvas16 block_canvas(BLOCK_W, BLOCK_H);
GFXcanvas16 small_canvas(SMALL_BLOCK_W, STATUS_BLOCK_H);

int temperature_c = 0;
/* Mock value until real measurement is wired in */
int power_w = 0;

/* Returns temperature in degrees C, or false if the sensor looks open/shorted */
static bool read_temperature_c(int *celsius) {
  const float adc_max = (float)((1 << TEMP_SENSOR_ADC_BITS) - 1);
  uint32_t sum = 0;

  for(int i = 0; i < TEMP_SENSOR_SAMPLES; i++) {
    sum += analogRead(TEMP_SENSOR_PIN);
  }
  float adc = (float)sum / TEMP_SENSOR_SAMPLES;

  /* Rail values mean shorted to GND (0) or open sensor (max) */
  if(adc < 1.0f || adc > adc_max - 1.0f) {
    return false;
  }

  /* Divider: Vpin = 3V3 * Rntc / (Rpullup + Rntc) */
  float r_ntc = TEMP_SENSOR_PULL_UP_RESISTANCE * adc / (adc_max - adc);

  /* Beta equation: 1/T = 1/T0 + ln(R/R0)/B */
  float t0_k = TEMP_SENSOR_NOMINAL_C + 273.15f;
  float t_k = 1.0f / (1.0f / t0_k + logf(r_ntc / TEMP_SENSOR_RESISTANCE) / TEMP_SENSOR_BETA);

  *celsius = (int)roundf(t_k - 273.15f);
  return true;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  analogReadResolution(TEMP_SENSOR_ADC_BITS);

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

  static uint32_t temp_sensor_timer = 0;
  if(millis() - temp_sensor_timer >= TEMP_SENSOR_PERIOD_MS) {
    temp_sensor_timer = millis();
    int celsius;
    if(read_temperature_c(&celsius) && celsius != temperature_c) {
      temperature_c = celsius;
      screen_update = true;
    }
  }

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Main(&buttons[btn].button);
	}

  if(screen_update) {
    screen_update = false;
    main_screen();
  }
}

/* Start a block: clear canvas to screen background, draw rounded block */
static void block_begin(uint16_t h, uint16_t color) {
  block_canvas.fillScreen(COLOR_BG);
  block_canvas.fillRoundRect(0, 0, BLOCK_W, h, BLOCK_RADIUS, color);
}

/* Push the first h rows of the shared canvas to the display (partial update) */
static void block_end(int16_t y, uint16_t h) {
  tft.drawRGBBitmap(BLOCK_X, y, block_canvas.getBuffer(), BLOCK_W, h);
}

static void block_label(const char *txt) {
  block_canvas.setFont(&FreeMonoBold9pt7b);
  block_canvas.setTextColor(COLOR_LABEL);
  block_canvas.setCursor(BLOCK_PAD, LABEL_BASELINE);
  block_canvas.print(txt);
}

static void block_value_right(const char *txt) {
  int16_t x1, y1;
  uint16_t w, h;
  block_canvas.setFont(&FreeMonoBold24pt7b);
  block_canvas.setTextColor(0xFFFF);
  block_canvas.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  block_canvas.setCursor(BLOCK_W - BLOCK_PAD - x1 - w, VALUE_BASELINE);
  block_canvas.print(txt);
}

static void draw_dimm_block(int dimm) {
  char buf[16];
  dimm = constrain(dimm, 0, 100);

  block_begin(BLOCK_H, COLOR_BLOCK);
  block_label("Dimming");

  block_canvas.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, COLOR_TRACK);
  int fill_w = dimm * BAR_W / 100;
  if(fill_w > 0) {
    block_canvas.fillRect(BAR_X, BAR_Y, fill_w, BAR_H, COLOR_DIMM_FILL);
  }

  snprintf(buf, sizeof(buf), "%d%%", dimm);
  block_value_right(buf);
  block_end(DIMM_BLOCK_Y, BLOCK_H);
}

static void draw_temp_block(int kelvin) {
  char buf[16];
  kelvin = constrain(kelvin, TEMP_MIN_K, TEMP_MAX_K);

  block_begin(BLOCK_H, COLOR_BLOCK);
  block_label("Warm");

  /* Warm (orange) -> cool (blue-white) gradient bar with position marker */
  for(int i = 0; i < BAR_W; i++) {
    int t = i * 255 / (BAR_W - 1);
    uint8_t r = 255 - (85 * t) / 255;
    uint8_t g = 150 + (55 * t) / 255;
    uint8_t b = 50 + (205 * t) / 255;
    block_canvas.drawFastVLine(BAR_X + i, BAR_Y, BAR_H, RGB565(r, g, b));
  }
  int marker = (kelvin - TEMP_MIN_K) * (BAR_W - 1) / (TEMP_MAX_K - TEMP_MIN_K);
  block_canvas.fillRect(BAR_X + marker - 1, BAR_Y - 4, 3, BAR_H + 8, 0xFFFF);

  snprintf(buf, sizeof(buf), "%dK", kelvin);
  block_value_right(buf);
  block_end(TEMP_BLOCK_Y, BLOCK_H);
}

/* Bottom-row block: rounded background with text centered both ways */
static void draw_small_block(int16_t x, uint16_t bg, uint16_t fg, const char *txt) {
  int16_t x1, y1;
  uint16_t w, h;

  small_canvas.fillScreen(COLOR_BG);
  small_canvas.fillRoundRect(0, 0, SMALL_BLOCK_W, STATUS_BLOCK_H, BLOCK_RADIUS, bg);
  small_canvas.setFont(&FreeMonoBold12pt7b);
  small_canvas.setTextColor(fg);
  small_canvas.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  small_canvas.setCursor((SMALL_BLOCK_W - w) / 2 - x1, (STATUS_BLOCK_H - h) / 2 - y1);
  small_canvas.print(txt);
  tft.drawRGBBitmap(x, STATUS_BLOCK_Y, small_canvas.getBuffer(), SMALL_BLOCK_W, STATUS_BLOCK_H);
}

static void draw_status_block(bool on) {
  draw_small_block(STATUS_BLOCK_X, on ? COLOR_ON : COLOR_BLOCK,
                   on ? 0xFFFF : COLOR_OFF_TEXT, on ? "ON" : "OFF");
}

static void draw_temp_c_block(int celsius) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%dC", constrain(celsius, 0, TEMP_C_MAX));
  draw_small_block(TEMP_C_BLOCK_X, COLOR_BLOCK, 0xFFFF, buf);
}

static void draw_power_block(int watts) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%dW", constrain(watts, 0, POWER_W_MAX));
  draw_small_block(POWER_BLOCK_X, COLOR_BLOCK, 0xFFFF, buf);
}

/* Partial update: only fields whose value changed are redrawn */
void main_screen(void) {
  static bool first_draw = true;
  static int last_dimm = -1;
  static int last_temp = -1;
  static int last_enabled = -1;
  static int last_temp_c = -1;
  static int last_power_w = -1;

  if(first_draw) {
    first_draw = false;
    tft.fillScreen(COLOR_BG);
  }

  int dimm = light.get_dimm();
  if(dimm != last_dimm) {
    last_dimm = dimm;
    draw_dimm_block(dimm);
  }

  int temp = light.get_color_temperature();
  if(temp != last_temp) {
    last_temp = temp;
    draw_temp_block(temp);
  }

  int enabled = light.is_enabled() ? 1 : 0;
  if(enabled != last_enabled) {
    last_enabled = enabled;
    draw_status_block(enabled);
  }

  if(temperature_c != last_temp_c) {
    last_temp_c = temperature_c;
    draw_temp_c_block(temperature_c);
  }

  if(power_w != last_power_w) {
    last_power_w = power_w;
    draw_power_block(power_w);
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
