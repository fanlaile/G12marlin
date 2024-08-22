#ifndef LCD_DWIN_CT300_H
#define LCD_DWIN_CT300_H

#include "string.h"
#include <arduino.h>

#include "../../inc/MarlinConfig.h"

void HMI_Init(void);
void HMI_UpdateProcess(void);
void HMI_MCU_Request(unsigned char cmd);

#endif
