/**
 ****************************************************************************************
 * @file rc_scheduler.h
 * @brief Remote Control Task Scheduler (基于PDF低功耗框架的task_list实现)
 ****************************************************************************************
 */
#ifndef __RC_SCHEDULER_H__
#define __RC_SCHEDULER_H__

#include <stdint.h>

/**
 * @brief RC调度器状态结构体
 */
typedef struct {
    // 任务时间戳数组（用于时间轮询调度）
    uint32_t last_timestamp[10];

    // 设备地址
    uint8_t esc_addr[5];        // 电控地址
    uint8_t bat_addr[5];        // 电池地址

    // 任务周期配置
    uint16_t esc_period_ms;     // 电控通信周期（默认50ms）
    uint16_t bat_period_ms;     // 电池通信周期（默认700ms）

    //油门状态
    uint16_t throttle_value;    // 当前油门值
    uint8_t  throttle_changed;  // 油门变化标志

    // 关机标志
    uint8_t shutdown_flag;

    // 运行状态
    uint8_t initialized;        // 初始化标志
} RC_Scheduler_t;

/**
 * @brief 初始化RC调度器
 * @param sched 调度器结构体指针
 * @note 会调用app_shutdown_init()锁定电源
 */
void RC_Scheduler_Init(RC_Scheduler_t *sched);

/**
 * @brief RC调度器主循环（内部while(1)，不会返回）
 * @param sched 调度器结构体指针
 * @note 实现PDF的task_list调度：
 *       - 10ms: app_key_scan()
 *       - 20ms: RF_Service_Handler()
 *       - 50ms: app_throttle_update()
 *       - 100ms: app_lcd_refresh()
 *       - 200ms: app_shutdown()
 *       - 500ms: app_heartbeat_send() + app_check_link()
 *       - 700ms: app_bat_status_process()
 *       - 动态睡眠: app_enter_sleep_with_wakeup_by_timer()
 */
void RC_Scheduler_Task(RC_Scheduler_t *sched);

/**
 * @brief 配置电控地址
 */
void RC_Scheduler_SetESCAddr(RC_Scheduler_t *sched, uint8_t *addr);

/**
 * @brief 配置电池地址
 */
void RC_Scheduler_SetBATAddr(RC_Scheduler_t *sched, uint8_t *addr);

/**
 * @brief 配置电控通信周期
 */
void RC_Scheduler_SetESCPeriod(RC_Scheduler_t *sched, uint16_t period_ms);

/**
 * @brief 配置电池通信周期
 */
void RC_Scheduler_SetBATPeriod(RC_Scheduler_t *sched, uint16_t period_ms);

#endif // __RC_SCHEDULER_H__
