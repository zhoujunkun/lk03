#ifndef _TLC5618_H
#define _TLC5618_H
#include "stm32f1xx_hal.h"
#include "main.h"



void tlc5618_write(uint16_t tlc_a,uint16_t tlc_b);
void tlc5618_writeBchannal(uint16_t tlc_reg);
void tlc5618_writeAchannal(uint16_t tlc_reg);
#endif


