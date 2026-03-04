/**
 ****************************************************************************************
 * @file debug.c
 * @brief Debug utilities implementation
 ****************************************************************************************
 */
#include "debug.h"
#include "user_config.h"
#include "BK3633_RegList.h"
#include "driver_bk24.h"

/**
 * @brief 打印所有RF相关寄存器
 */
void debug_print_rf_registers(void)
{
    uart_printf("\r\n=== RF Registers Dump ===\r\n");

    // TRX寄存器
    uart_printf("TRX_CONFIG     = 0x%02X\r\n", TRX_CONFIG);
    uart_printf("TRX_EN_AA      = 0x%02X\r\n", TRX_EN_AA);
    uart_printf("TRX_EN_RXADDR  = 0x%02X\r\n", TRX_EN_RXADDR);
    uart_printf("TRX_SETUP_AW   = 0x%02X\r\n", TRX_SETUP_AW);
    uart_printf("TRX_SETUP_RETR = 0x%02X\r\n", TRX_SETUP_RETR);
    uart_printf("TRX_RF_CH      = 0x%02X\r\n", TRX_RF_CH);
    uart_printf("TRX_RF_SETUP   = 0x%02X\r\n", TRX_RF_SETUP);

    uart_printf("TRX_RX_ADDR_P0 = ");
    for(int i=0; i<5; i++)
        uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_ADDR_P1 = ");
    for(int i=0; i<5; i++)
        uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P1_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_ADDR_P2 = 0x%02X\r\n", TRX_RX_ADDR_P2);
    uart_printf("TRX_RX_ADDR_P3 = 0x%02X\r\n", TRX_RX_ADDR_P3);
    uart_printf("TRX_RX_ADDR_P4 = 0x%02X\r\n", TRX_RX_ADDR_P4);
    uart_printf("TRX_RX_ADDR_P5 = 0x%02X\r\n", TRX_RX_ADDR_P5);

    uart_printf("TRX_TX_ADDR    = ");
    for(int i=0; i<5; i++)
        uart_printf("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_PW_P0   = 0x%02X\r\n", TRX_RX_PW_P0);
    uart_printf("TRX_RX_PW_P1   = 0x%02X\r\n", TRX_RX_PW_P1);
    uart_printf("TRX_RX_PW_P2   = 0x%02X\r\n", TRX_RX_PW_P2);
    uart_printf("TRX_RX_PW_P3   = 0x%02X\r\n", TRX_RX_PW_P3);
    uart_printf("TRX_RX_PW_P4   = 0x%02X\r\n", TRX_RX_PW_P4);
    uart_printf("TRX_RX_PW_P5   = 0x%02X\r\n", TRX_RX_PW_P5);

    uart_printf("TRX_DYNPD      = 0x%02X\r\n", TRX_DYNPD);
    uart_printf("TRX_FEATURE    = 0x%02X\r\n", TRX_FEATURE);

    // XVR寄存器
    uart_printf("\r\n=== XVR Registers ===\r\n");
    uart_printf("addXVR_Reg0x24 = 0x%08lX\r\n", addXVR_Reg0x24);
    uart_printf("addXVR_Reg0x04 = 0x%08lX\r\n", addXVR_Reg0x4);
    uart_printf("addXVR_Reg0x3b = 0x%08lX\r\n", addXVR_Reg0x3b);
    uart_printf("addXVR_Reg0x2e = 0x%08lX\r\n", addXVR_Reg0x2e);
    uart_printf("addXVR_Reg0x26 = 0x%08lX\r\n", addXVR_Reg0x26);
    uart_printf("addXVR_Reg0x2  = 0x%08lX\r\n", addXVR_Reg0x2);
    uart_printf("addXVR_Reg0x2c = 0x%08lX\r\n", addXVR_Reg0x2c);
    uart_printf("addXVR_Reg0x2d = 0x%08lX\r\n", addXVR_Reg0x2d);
    uart_printf("addXVR_Reg0x3a = 0x%08lX\r\n", addXVR_Reg0x3a);
    uart_printf("=========================\r\n\r\n");
}

/**
 * @brief 打印TX和RX地址
 */
void debug_print_txrx_addr(void)
{
    uart_printf("RX_ADDR: ");
    for(int i=0; i<5; i++)
        uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);

    uart_printf(" | TX_ADDR: ");
    for(int i=0; i<5; i++)
        uart_printf("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    uart_printf("\r\n");
}

/**
 * @brief 以十六进制格式打印数据
 * @param msg 消息前缀
 * @param dat 数据指针
 * @param len 数据长度
 */
void debug_print_hex(void *msg, void *dat, int len)
{
    char *ptr = (char*)dat;
    int i;

    uart_printf(msg);
    uart_printf("[%d]={\r\n\t", len);

    for(i=0; i<len; i++) {
        uart_printf("%.2x ", ptr[i]);
        if((i & 0x07) == 0x07)
            uart_printf("\r\n\t");
    }

    uart_printf("\r\n}\r\n");
}
