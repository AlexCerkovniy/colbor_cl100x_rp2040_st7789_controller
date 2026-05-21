#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "pin_config.h"
#include "color_temp_table.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"

/* Define temp. sensor */
#define TEMP_SENSOR_RESISTANCE         10000
#define TEMP_SENSOR_PULL_UP_RESISTANCE 1000

//Serial buffering
uint8_t serial_buffer[128] = {0};
uint32_t serial_buffer_size = 0;
bool serial_buffer_ready = false;

//Stats
int supply_voltage_mv = 0;
int supply_current_ma = 0;

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);



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
      //digitalWrite(CONVERTER_EN_PIN, converter_enabled);
    }
    else if(serial_buffer[0] == 'y') {
      //yellow_brightness = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
      
    }
    else if(serial_buffer[0] == 'w') {
      //white_brightness = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
    }
    else if(serial_buffer[0] == 'g') {
      //global_brightness = map(atoi((char *)&serial_buffer[1]), 0, 100, 10, 100);
    }
    else if(serial_buffer[0] == 'f') {
      //fan_speed = constrain(atoi((char *)&serial_buffer[1]), 0, 100);
    }
    else {
      Serial.println("Unknown data");
    }

    serial_buffer_ready = false;
    serial_buffer_size = 0;
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
  //tft.printf("%d%%", global_brightness);

  // Color temp
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextColor(0xFFFF);
  tft.setCursor(100, 130);
  //tft.printf("%dK", color_temp_table[color_temp_index]);
}
