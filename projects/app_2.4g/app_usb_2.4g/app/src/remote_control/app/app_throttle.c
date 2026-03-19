/**
 ****************************************************************************************
 * @file app_throttle.c
 * @brief 油门控制模块（霍尔读取 + RF发送）
 ****************************************************************************************
 */
#include "app_throttle.h"
#include "hall_sensor.h"
#include "rf_handler.h"
#include "pinmap_v1.h"
#include "user_config.h"

// 霍尔传感器实例
hall_sensor_t hall;
static uint16_t filter_buf[8];

// 油门发送死区
#define THROTTLE_SEND_DEADZONE  3

/**
 * @brief 油门模块初始化
 */
void app_throttle_init(void) {
    //旧板子
    // hall_hw_config_t hw_config = {
    //     .adc_channel = 2,
    //     .en_ctrl = {
    //         .gpio = Port_Pin(1,2),
    //         .active_level = 0,
    //     },
    //     .power_ctrl = {
    //         .gpio = Port_Pin(1,0),
    //         .active_level = 1,
    //     },
    // };

    hall_hw_config_t hw_config = {
        .adc_channel = 1,
        .en_ctrl = {
            .gpio = Port_Pin(1,0),
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = Port_Pin(1,0),
            .active_level = 0,
        },
    };

    hall_map_config_t map_config = {
        .adc_min = 96,
        .adc_max = 919,
        .deadzone_low = 10,
        .deadzone_high = 10,
        .throttle_min = 0,
        .throttle_max = 100,
        .map_type = HALL_MAP_LINEAR,
        .reverse = true,
    };

    hall_sensor_init(&hall, &hw_config, &map_config, filter_buf, 8);

    uart_printf("app_throttle_init done\r\n");
}

/**
 * @brief 油门值更新
 * @param throttle_value 输出参数: 指向油门值的指针
 * @param throttle_changed_flag 输出参数: 指向油门变化标志的指针
 * @note 变化判断: 比较当前值和上次发送值，超过THROTTLE_SEND_DEADZONE才标记为变化
 *       这样可以避免油门慢慢变化时（每次变化小于死区）导致的漏发问题
 */
void app_throttle_update(uint8_t *throttle_value, uint8_t *throttle_changed_flag) {

    // 上次发送的油门值（用于死区判断）
    static uint16_t last_sent_throttle = 0;

    hall_sensor_update(&hall);
    uint16_t now_value = hall.data.throttle;
    *throttle_value = now_value;

    // 判断油门是否需要发送: 比较当前值和上次发送值
    int diff = abs((int)now_value - (int)last_sent_throttle);
    if (diff >= THROTTLE_SEND_DEADZONE) {
        *throttle_changed_flag = 1;
        last_sent_throttle = now_value;  // 记录本次发送值
    } else {
        *throttle_changed_flag = 0;
    }
}
