/**
 ****************************************************************************************
 * @file rc_scheduler.c
 * @brief Remote Control Task Scheduler Implementation
 ****************************************************************************************
 */
#include "rc_scheduler.h"
#include "app_shutdown.h"
#include "app_sleep.h"
#include "app_key.h"
#include "app_throttle.h"
#include "rf_handler.h"
#include "timer_handler.h"
#include "user_config.h"
#include <string.h>

// RF活动后的保护窗口(ms)，等待发送完成+接收应答
#define RF_GUARD_WINDOW_MS  8

/**
 * @brief 初始化RC调度器
 */
void RC_Scheduler_Init(RC_Scheduler_t *sched) {
    memset(sched, 0, sizeof(RC_Scheduler_t));

    // 拉高SYS_PWR_EN，锁定电源
    app_board_power_on();

    // 初始化油门模块
    app_throttle_init();
    app_key_init();

    sched->initialized = 1;

    uart_printf("RC_Scheduler_Init done\r\n");
}

/**
 * @brief RC调度器主循环（不会返回）
 */
void RC_Scheduler_Task(RC_Scheduler_t *sched) {
    if(!sched->initialized) return;

    uart_printf("enter RC_Scheduler_Task\r\n");

    while(1) {
        static uint32_t last_timestamp[10] = {0};
        static uint32_t rf_guard_deadline = 0; // RF保护窗口时间

        // ========== 10ms任务：按键扫描 ==========
        if((Get_SysTick_ms() - last_timestamp[0]) >= 10) {
            last_timestamp[0] = Get_SysTick_ms();
            app_key_scan();
        }

        // ========== 20ms任务：RF服务 ==========
        if((Get_SysTick_ms() - last_timestamp[1]) >= 20) {
            last_timestamp[1] = Get_SysTick_ms();
            RF_Service_Handler(&hrf);
            // 每次RF活动后刷新保护窗口
            rf_guard_deadline = Get_SysTick_ms() + RF_GUARD_WINDOW_MS;
        }

        // ========== 50ms任务：油门更新 ==========
        if((Get_SysTick_ms() - last_timestamp[2]) >= 50) {
            last_timestamp[2] = Get_SysTick_ms();
            app_throttle_update(&sched->throttle_value, &sched->throttle_changed);
        }

        // ========== 100ms任务：LCD刷新 ==========
        if((Get_SysTick_ms() - last_timestamp[3]) >= 100) {
            last_timestamp[3] = Get_SysTick_ms();
            // app_lcd_refresh();     // TODO
        }

        // ========== 200ms任务：关机检测 ==========
        if((Get_SysTick_ms() - last_timestamp[4]) >= 200) {
            last_timestamp[4] = Get_SysTick_ms();
            app_board_shutdown(sched->shutdown_flag);
        }

        // ========== 500ms任务：心跳 + 链路检测 ==========
        if((Get_SysTick_ms() - last_timestamp[5]) >= 500) {
            last_timestamp[5] = Get_SysTick_ms();
            // app_heartbeat_send();  // TODO
            // app_check_link();      // TODO
        }

        // ========== 700ms任务：电池状态查询 ==========
        if((Get_SysTick_ms() - last_timestamp[6]) >= 700) {
            last_timestamp[6] = Get_SysTick_ms();
            // app_bat_status_process(); // TODO
        }
        
        // ========== 睡眠判断 ==========
        // 条件1: RF硬件不在发送中
        // 条件2: 发送队列已空
        // 条件3: RF保护窗口已过（发送完成+等待应答的时间）
        if(Get_SysTick_ms() >= rf_guard_deadline) {
            app_enter_sleep_with_wakeup_by_timer(20, 1);
        }
        else{ //在窗口内尝试处理rx接收队列

        }
    }
}

/**
 * @brief 配置电控地址
 */
void RC_Scheduler_SetESCAddr(RC_Scheduler_t *sched, uint8_t *addr) {
    memcpy(sched->esc_addr, addr, 5);
}

/**
 * @brief 配置电池地址
 */
void RC_Scheduler_SetBATAddr(RC_Scheduler_t *sched, uint8_t *addr) {
    memcpy(sched->bat_addr, addr, 5);
}

/**
 * @brief 配置电控通信周期
 */
void RC_Scheduler_SetESCPeriod(RC_Scheduler_t *sched, uint16_t period_ms) {
    sched->esc_period_ms = period_ms;
}

/**
 * @brief 配置电池通信周期
 */
void RC_Scheduler_SetBATPeriod(RC_Scheduler_t *sched, uint16_t period_ms) {
    sched->bat_period_ms = period_ms;
}
