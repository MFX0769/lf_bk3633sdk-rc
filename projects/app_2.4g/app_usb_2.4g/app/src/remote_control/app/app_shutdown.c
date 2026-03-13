/**
 ****************************************************************************************
 * @file app_shutdown.c
 * @brief System power management and shutdown control
 ****************************************************************************************
 */
#include "app_shutdown.h"
#include "drv_gpio.h"
#include "pinmap_v1.h"
#include "key_scan.h"
#include "timer_handler.h"
#include "user_config.h"

/**
 * @brief 电源管理芯片使能上电
 */
void app_board_power_on(void) {
    // 配置SYS_PWR_EN为输出
    gpio_config(SYS_PWR_EN, GPIO_OUTPUT, GPIO_PULL_NONE);

    // 拉高，锁定电源
    gpio_set(SYS_PWR_EN, 1);

    uart_printf("Power Hold: SYS_PWR_EN = HIGH\r\n");

    // 配置电源按键为输入，下拉，按下=高电平
    gpio_config(KEY_SYS_PWR, GPIO_INPUT, GPIO_PULL_LOW);
}
/**
 * @brief 获取关机标志
 */
uint8_t app_board_shutdown(uint8_t shutdown_flag) {
    if(!shutdown_flag) 
        return ;
    gpio_set(SYS_PWR_EN, 0);
}
