/**
 ****************************************************************************************
 * @file debug.h
 * @brief Debug utilities for register dump and hex print
 ****************************************************************************************
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>

void debug_print_rf_registers(void);
void debug_print_txrx_addr(void);
void debug_print_hex(void *msg, void *dat, int len);

#endif // _DEBUG_H_
