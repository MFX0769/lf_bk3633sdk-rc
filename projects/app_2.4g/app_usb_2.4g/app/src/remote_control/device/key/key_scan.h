#ifndef _KEY_SCAN_H_
#define _KEY_SCAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "drv_gpio.h" // 需要用到 gpio_num_t 和 Port_Pin 宏

/**
 * @brief 按键 ID 枚举
 * 用户可根据实际按键数量增减，但建议保留 MAX
 */
typedef enum {
    KEY_ID_LEFT = 0,
    KEY_ID_RIGHT,
    KEY_ID_MIDDLE,
    KEY_ID_PAIR,
    KEY_ID_MAX
} key_id_t;

/**
 * @brief 按键事件枚举
 */
typedef enum {
    KEY_EVT_NONE = 0,       ///< 无事件
    KEY_EVT_DOWN,           ///< 按下瞬间
    KEY_EVT_UP,             ///< 释放瞬间
    KEY_EVT_SHORT_PRESS,    ///< 短按 (点击，释放后触发)
    KEY_EVT_LONG_PRESS,     ///< 长按触发 (按下超过设定时间触发)
    KEY_EVT_LONG_HOLD,      ///< 长按保持 (触发长按后继续保持，可用于连发)
    KEY_EVT_LONG_RELEASE,   ///< 长按后释放
} key_event_t;

/**
 * @brief 按键配置结构体
 * 用于初始化时传入每个逻辑按键对应的物理引脚
 */
typedef struct {
    key_id_t id;             ///< 逻辑按键 ID
    gpio_num_t pin;          ///< 物理引脚，使用 Port_Pin(port, pin) 生成
    uint16_t long_press_ms;  ///< 长按判定时间(ms)，0表示使用默认值(1000ms)
    bool active_level;       ///< 有效电平：true=高电平有效，false=低电平有效(默认)
} key_config_t;

/**
 * @brief 按键事件回调函数类型
 * @param id 触发事件的按键 ID
 * @param event 触发的事件类型
 */
typedef void (*key_callback_t)(key_id_t id, key_event_t event);

/**
 * @brief 设置按键事件全局回调
 * @param cb 回调函数指针
 */
void key_register_callback(key_callback_t cb);

/**
 * @brief 初始化按键扫描模块
 * 
 * 根据 active_level 配置 GPIO 上下拉，并初始化内部状态机。
 * 
 * @param p_config 按键配置数组指针
 * @param count 配置数组的元素个数
 * 
 * @example
 * // 定义按键映射表
 * const key_config_t my_keys[] = {
 *     {KEY_ID_LEFT,   Port_Pin(Port0, Pin1), 1000, false}, // P0.1 左键，低电平有效
 *     {KEY_ID_RIGHT,  Port_Pin(Port0, Pin2), 1000, true }, // P0.2 右键，高电平有效
 *     {KEY_ID_MIDDLE, Port_Pin(Port1, Pin5), 1500, false}, // P1.5 中键，低电平有效
 * };
 * 
 * // 系统初始化时调用
 * key_init(my_keys, sizeof(my_keys)/sizeof(key_config_t));
 */
void key_init(const key_config_t *p_config, uint8_t count);

/**
 * @brief 按键扫描函数
 * 
 * 核心状态机处理函数，必须周期性调用。
 * 
 * @note 需周期性调用，调用周期以毫秒传入，内部基于该周期完成消抖与计时。
 * 
 * @example
 * // 在定时器中断或主循环中调用
 * if (is_time_for_10ms_task()) {
 *     key_scan(10);
 * }
 */
void key_scan(uint16_t period_ms);
void key_scan_10ms(void);

/**
 * @brief 获取指定按键的最新事件
 * 
 * 读取按键事件缓冲区。读取后，该按键的当前事件会被清除（重置为 NONE）。
 * 
 * @param key 要查询的按键 ID
 * @return key_event_t 当前事件
 * 
 * @example
 * // 典型用法：在主循环中处理事件
 * key_event_t evt = key_get_event(KEY_ID_LEFT);
 * switch(evt) {
 *     case KEY_EVT_SHORT_PRESS:
 *         printf("Left Key Clicked\n");
 *         break;
 *     case KEY_EVT_LONG_PRESS:
 *         printf("Left Key Long Pressed (Start)\n");
 *         break;
 *     // ... 其他事件
 * }
 */
key_event_t key_get_event(key_id_t key);

/**
 * @brief 查询按键当前是否处于按下状态
 * 
 * 返回消抖后的实时电平状态。
 * 
 * @param key 按键 ID
 * @return true 按下
 * @return false 释放
 */
bool key_is_pressed(key_id_t key);

/**
 * @brief 检测组合键状态
 * 
 * 检查指定的多个按键是否**同时**处于按下状态。
 * 
 * @param key_mask 按键位掩码，通过 (1 << KEY_ID_xx) | (1 << KEY_ID_yy) 组合
 * @return true 掩码中指定的所有按键当前都处于按下状态
 * @return false 只要有一个按键未按下即返回 false
 * 
 * @example
 * // 检测 "左键 + 右键" 同时按下（例如用于进入配对模式）
 * if (key_check_combo((1 << KEY_ID_LEFT) | (1 << KEY_ID_RIGHT))) {
 *     // 检测到组合键按下，执行相应逻辑
 *     // 注意：通常建议在长按事件中配合检测，或者单独检测状态
 * }
 * 
 * // 进阶用法：检测 "长按左键" 的同时 "点击右键"
 * // 1. 监听右键的短按事件
 * if (key_get_event(KEY_ID_RIGHT) == KEY_EVT_SHORT_PRESS) {
 *     // 2. 检查左键是否正按着
 *     if (key_is_pressed(KEY_ID_LEFT)) {
 *         printf("Combo: Hold Left + Click Right\n");
 *     }
 * }
 * 
 * // 场景：检测单键的“短按 + 长按”序列（例如：点击后立即按住不放，触发开关机）
 * // 逻辑：短按 -> 释放 -> (规定时间内) -> 再次按下 -> 长按
 * static uint8_t  power_step = 0;
 * static uint32_t wait_timer = 0;
 * 
 * key_event_t evt = key_get_event(KEY_ID_LEFT);
 * 
 * switch (power_step) {
 *     case 0: // 等待第一次短按
 *         if (evt == KEY_EVT_SHORT_PRESS) {
 *             power_step = 1;
 *             wait_timer = 50; // 开启 500ms 窗口等待第二次按下
 *         }
 *         break;
 *     case 1: // 等待第二次按下
 *         if (evt == KEY_EVT_DOWN) {
 *             power_step = 2; // 检测到第二次按下，进入等待长按状态
 *         } else if (wait_timer == 0) {
 *             power_step = 0; // 超时，视为普通短按
 *             // 执行普通短按逻辑...
 *         }
 *         break;
 *     case 2: // 等待长按事件
 *         if (evt == KEY_EVT_LONG_PRESS) {
 *             printf("Power On/Off Triggered!\n");
 *             power_step = 0;
 *         } else if (evt == KEY_EVT_UP || evt == KEY_EVT_SHORT_PRESS) {
 *             power_step = 0; // 只是双击，重置
 *         }
 *         break;
 * }
 * 
 * if (power_step == 1 && wait_timer > 0) wait_timer--;
 */
bool key_check_combo(uint32_t key_mask);

#endif // _KEY_SCAN_H_
