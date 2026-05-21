#pragma once

#include <Arduino.h>

class LightControl {
  public:
    void begin(void);
    void tick(void);
    void set_yellow(int level);
    void set_white(int level);
    void set_dimm(int level);
};
