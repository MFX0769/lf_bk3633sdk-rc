/**
 ****************************************************************************************
 * @file main.c
 * @brief RC Remote Controller - Main Entry
 ****************************************************************************************
 */
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "intc.h"
#include "my_drv_uart.h"
#include "my_drv_uart2.h"
#include "flash.h"
#include "reg_access.h"
#include "boot.h"
#include "dbg.h"
#include "icu.h"
#include "wdt.h"
#include "spi.h"
#include "adc.h"
#include "aon_rtc.h"
#include "rf.h"
#include "drv_gpio.h"
#include "driver_timer.h"
#include "timer_handler.h"
#include "rf_handler.h"
#include "rc_scheduler.h"
#include "user_config.h"
#include "pinmap.h"
#include "lcd_init.h"
#include "lcd_kmg.h"

#if(USB_DRIVER)
#include "usb.h"
#include "driver_icu.h"
#endif

extern void xvr_reg_initial_24(void);

static void stack_integrity_check(void)
{
    if ((REG_PL_RD(STACK_BASE_UNUSED) != BOOT_PATTERN_UNUSED)) {
        while(1) { uart_printf("Stack_Integrity_Check STACK_BASE_UNUSED fail!\r\n"); }
    }
    if ((REG_PL_RD(STACK_BASE_SVC) != BOOT_PATTERN_SVC)) {
        while(1) { uart_printf("Stack_Integrity_Check STACK_BASE_SVC fail!\r\n"); }
    }
    if ((REG_PL_RD(STACK_BASE_FIQ) != BOOT_PATTERN_FIQ)) {
        while(1) { uart_printf("Stack_Integrity_Check STACK_BASE_FIQ fail!\r\n"); }
    }
    if ((REG_PL_RD(STACK_BASE_IRQ) != BOOT_PATTERN_IRQ)) {
        while(1) { uart_printf("Stack_Integrity_Check STACK_BASE_IRQ fail!\r\n"); }
    }
}

void platform_reset(uint32_t error)
{
    uart_printf("reset error = %x\r\n", error);
    GLOBAL_INT_STOP();
    cpu_reset();
}


int main(void)
{
    /* ========== Hardware Init ========== */
    icu_init();
    intc_init();

    #if(UART_PRINTF_ENABLE)
    uart2_init(1000000);
    uart_init(1000000);
    #else
    gpio_config(Port_Pin(0,0),GPIO_FLOAT,GPIO_PULL_NONE); //uart关掉
    gpio_config(Port_Pin(0,1),GPIO_FLOAT,GPIO_PULL_NONE);
    #endif
    uart_printf("init uarts\r\n");

    xvr_reg_initial();

    /* Timer init (depends on rc32k clock from xvr init) */
    Timer_Handler_Init();
    uart_printf("Timer_Handler_Init done:%d\r\n", Get_SysTick_ms());

    flash_init();
    uart_printf("init flash:%d\r\n", Get_SysTick_ms());

    #ifndef __USB_TEST__
    mcu_clk_switch(MCU_CLK_16M);
    uart_printf("select MCU_CLK_16M:%d\r\n", Get_SysTick_ms());
    #else
    mcu_clk_switch(MCU_CLK_64M);
    #endif

    #if(AON_RTC_DRIVER)
    aon_rtc_init();
    uart_printf("init aon_rtc:%d\r\n", Get_SysTick_ms());
    #endif

    #if(ADC_DRIVER)
    adc_init(2, 1);
    adc_init(1, 1);
    #endif

    GLOBAL_INT_START();
    uart_printf("GLOBAL_INT_START:%d\r\n", Get_SysTick_ms());

    #if(SPI_DRIVER)
    spi_init(0, 0, 0);
    uart_printf("init spi:%d\r\n", Get_SysTick_ms());
    #endif

    #if(ENABLE_LED_DISPLAY)
    gpio_config(LCD_PWR_EN, GPIO_OUTPUT, GPIO_PULL_NONE);
    gpio_set(LCD_PWR_EN, 1);
    OLED_Init();
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
    update_ui_test(10, 85);
    #endif


    /* ======================== Test Mode Switch ======================== */
    /* Set to 0 for normal remote control mode
    * Set to 1 for ESC slave test mode
    * Set to 2 for battery slave test mode */
    #define TEST_MODE  2

    /* Forward declarations for test modes */

    /* ========== Application Mode ========== */
    #if (TEST_MODE == 1)
    extern void test_slave_loop(void);
    test_slave_loop();          /* ESC slave test (never returns) */
    #elif (TEST_MODE == 2)
    extern void test_bat_slave_loop(void);
    test_bat_slave_loop();      /* Battery slave test (never returns) */
    #else
    {
        RC_Scheduler_t sched;
        RC_Scheduler_Init(&sched);
        RC_Scheduler_Task(&sched);  /* Normal mode (never returns) */
    }
    #endif

    /* Should never reach here */
    while(1) {
        stack_integrity_check();
    }
}
