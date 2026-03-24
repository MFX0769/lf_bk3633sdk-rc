/**
 ****************************************************************************************
 * @file app_key.c
 * @brief Application layer key management
 ****************************************************************************************
 */
#include "app_key.h"
#include "key_scan.h"
#include "pinmap.h"
#include "timer_handler.h"
#include "user_config.h"

// 配对标志
static uint8_t pair_flag = 0;
// 关机标志
static uint8_t shutdown_flag = 0;

/**
 * @brief 按键事件处理回调
 */
static void app_key_event_handler(key_id_t id, key_event_t event) {
    switch(id) {
        case KEY_ID_LEFT: // KEY1
            if(event == KEY_EVT_SHORT_PRESS) {
                uart_printf("KEY1: Short Press\r\n");
                // TODO: 功能1
            } else if(event == KEY_EVT_LONG_PRESS) {
                uart_printf("KEY1: Long Press\r\n");
                // TODO: 功能2
                app_enter_sleep_with_wakeup_by_timer(10000, 1);
            }
            break;

        case KEY_ID_RIGHT: // KEY2
            if(event == KEY_EVT_SHORT_PRESS) {
                uart_printf("KEY2: Short Press\r\n");

                // 连续5次短按进入配对模式
                static uint8_t cnt = 0;
                static uint32_t last_time = 0;
                uint32_t now = Get_SysTick_ms();

                // 1秒内连续按5次
                if(now - last_time > 1000) {
                    cnt = 0;
                }
                cnt++;
                last_time = now;

                if(cnt >= 5) {
                    pair_flag = !pair_flag;  // 翻转：进入/退出配对
                    if (pair_flag)
                        uart_printf("Enter Pairing Mode (KEY2 x5)\r\n");
                    else
                        uart_printf("Exit Pairing Mode (KEY2 x5)\r\n");
                    cnt = 0;
                }
            }
            break;

        case KEY_ID_POWER: // KEY_SYS_PWR
        
            if(event == KEY_EVT_SHORT_PRESS) {
                uart_printf("PWR KEY: Short Press\r\n");
            } else if(event == KEY_EVT_LONG_PRESS) {
                uart_printf("PWR KEY: Long Press\r\n");
                // 长按3s进入关机标志
                shutdown_flag = 1;

            }
            break;

        default:
            break;
    }
}

/**
 * @brief 按键模块初始化
 */
void app_key_init(void) {
    // 配置按键（注意：按下=高电平）
    const key_config_t keys[] = {
        {KEY_ID_LEFT,   KEY1, 1000, true},         // KEY1: 高电平有效
        {KEY_ID_RIGHT,  KEY2, 1000, true},         // KEY2: 高电平有效
        {KEY_ID_POWER,  KEY_SYS_PWR, 3000, true},  // KEY_SYS_PWR: 高电平有效（长按3s）
    };

    key_init(keys, 3);
    key_register_callback(app_key_event_handler);

    uart_printf("app_key_init done\r\n");
}

/**
 * @brief 按键扫描
 */
void app_key_scan(uint8_t ms_period) {
    key_scan(ms_period);
}

/**
 * @brief 获取配对标志
 */
uint8_t app_key_get_pair_flag(void) {
    return pair_flag;
}

/**
 * @brief 获取配对标志指针（供Host_Pairing_Task直接操作）
 */
uint8_t* app_key_get_pair_flag_ptr(void) {
    return &pair_flag;
}

/**
 * @brief 清除配对标志
 */
void app_key_clear_pair_flag(void) {
    pair_flag = 0;
}

/**
 * @brief 获取关机键信号
 */
uint8_t app_key_get_shutdown_flag(void) {
    return shutdown_flag;
}
