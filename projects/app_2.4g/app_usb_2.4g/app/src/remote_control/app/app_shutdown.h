/**
 ****************************************************************************************
 * @file app_shutdown.h
 * @brief System power management and shutdown control
 ****************************************************************************************
 */
#ifndef __APP_SHUTDOWN_H__
#define __APP_SHUTDOWN_H__

#include <stdint.h>

/**
 * @brief 系统电源初始化（拉高电源使能，锁定电源）
 * @note 在RC_Scheduler_Init()中调用
 */
void app_board_power_on(void);

/**
 * @brief 获取关机标志
 * @return 1=需要关机, 0=正常运行
 */
uint8_t app_board_shutdown(uint8_t shutdown_flag);

#endif // __APP_SHUTDOWN_H__
