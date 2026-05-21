#include "button_port.h"
#include "button_config.h"

/* Port (platform) includes */
#include "main.h"
#include "keyboard.h"

enum {
	BTN_0_MASK = (0x01 << 0),
	BTN_1_MASK = (0x02 << 0),
	BTN_2_MASK = (0x02 << 4),
	BTN_3_MASK = (0x02 << 8),
	BTN_4_MASK = (0x04 << 0),
	BTN_5_MASK = (0x04 << 4),
	BTN_6_MASK = (0x04 << 8),
	BTN_7_MASK = (0x08 << 0),
	BTN_8_MASK = (0x08 << 4),
	BTN_9_MASK = (0x08 << 8),
	BTN_DOT_MASK = (0x01 << 4),
	BTN_EQUAL_MASK = (0x01 << 8),
	BTN_ADD_MASK = (0x01 << 12),
	BTN_SUBSTRACT_MASK = (0x02 << 12),
	BTN_MULTIPLY_MASK = (0x04 << 12),
	BTN_DIVIDE_MASK = (0x08 << 12)
} keyboard_btn_mask;

uint16_t buttons_state = 0;

void BTN_PL_Init(uint16_t id){
	buttons_state = 0;	/* All buttons is not pressed */
}

bool BTN_PL_Read(uint16_t id){
    switch(id){
        case BTN_0_ID: return (buttons_state & BTN_0_MASK)?(true):(false);
        case BTN_1_ID: return (buttons_state & BTN_1_MASK)?(true):(false);
        case BTN_2_ID: return (buttons_state & BTN_2_MASK)?(true):(false);
        case BTN_3_ID: return (buttons_state & BTN_3_MASK)?(true):(false);
        case BTN_4_ID: return (buttons_state & BTN_4_MASK)?(true):(false);
        case BTN_5_ID: return (buttons_state & BTN_5_MASK)?(true):(false);
        case BTN_6_ID: return (buttons_state & BTN_6_MASK)?(true):(false);
        case BTN_7_ID: return (buttons_state & BTN_7_MASK)?(true):(false);
        case BTN_8_ID: return (buttons_state & BTN_8_MASK)?(true):(false);
        case BTN_9_ID: return (buttons_state & BTN_9_MASK)?(true):(false);
        case BTN_DOT_ID: return (buttons_state & BTN_DOT_MASK)?(true):(false);
        case BTN_EQUAL_ID: return (buttons_state & BTN_EQUAL_MASK)?(true):(false);
        case BTN_ADD_ID: return (buttons_state & BTN_ADD_MASK)?(true):(false);
        case BTN_SUBSTRACT_ID: return (buttons_state & BTN_SUBSTRACT_MASK)?(true):(false);
        case BTN_MULTIPLY_ID: return (buttons_state & BTN_MULTIPLY_MASK)?(true):(false);
        case BTN_DIVIDE_ID: return (buttons_state & BTN_DIVIDE_MASK)?(true):(false);
        default:
            return false;
    }

    return false;
}
