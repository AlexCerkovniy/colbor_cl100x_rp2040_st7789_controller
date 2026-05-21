#include "button_port.h"
#include "button_config.h"
#include "pin_config.h"
#include <Arduino.h>

__attribute((__weak__)) void BTN_PL_Init(uint16_t id){
    pinMode(SW1_UP_PIN, INPUT_PULLUP);
    pinMode(SW1_DOWN_PIN, INPUT_PULLUP);
    pinMode(SW2_UP_PIN, INPUT_PULLUP);
    pinMode(SW2_DOWN_PIN, INPUT_PULLUP);
}

__attribute((__weak__)) bool BTN_PL_Read(uint16_t id){
    if(id == SW1_UP) {
        return !digitalRead(SW1_UP_PIN) && digitalRead(SW1_DOWN_PIN);
    }
    else if(id == SW1_DOWN) {
        return digitalRead(SW1_UP_PIN) && !digitalRead(SW1_DOWN_PIN);
    }
    else if(id == SW1_PRESS) {
        return !digitalRead(SW1_UP_PIN) && !digitalRead(SW1_DOWN_PIN);
    }
    else if(id == SW2_UP) {
        return !digitalRead(SW2_UP_PIN) && digitalRead(SW2_DOWN_PIN);
    }
    else if(id == SW2_DOWN) {
        return digitalRead(SW2_UP_PIN) && !digitalRead(SW2_DOWN_PIN);
    }
    else if(id == SW2_PRESS) {
        return !digitalRead(SW2_UP_PIN) && !digitalRead(SW2_DOWN_PIN);
    }

    return false;
}
