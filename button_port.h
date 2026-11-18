#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"

void BTN_PL_Init(uint16_t id);
bool BTN_PL_Read(uint16_t id);

#ifdef __cplusplus
}
#endif