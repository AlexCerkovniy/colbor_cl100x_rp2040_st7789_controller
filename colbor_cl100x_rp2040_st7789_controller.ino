#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pin_config.h"
#include "color_temp_table.h"
#include "light_control.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"
#include "button.h"

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

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Init(&buttons[btn].button, buttons[btn].button_id, 50, 0 /* Disabled */);
		BTN_RegisterCallback(&buttons[btn].button, button_callback);
	}

  light.begin();


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
      light.toggle();
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
  }

  light.tick();

  for(uint32_t btn = 0; btn < BUTTONS_COUNT; btn++){
		BTN_Main(&buttons[btn].button);
	}
}


void draw_value_window(int x, int y, int w, int h) {
  tft.setFont(&FreeMonoBold9pt7b);
  tft.fillRect(x, y, w, 9/* Font height */ + 2 * 2, 0xFFFF);
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
  tft.printf("%d%%", light.get_dimm());

  // Color temp
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 130);
  //tft.printf("%dK", color_temp_table[color_temp_index]);
}

static void button_callback(uint8_t button_id, button_callback_event_t event) {

}
