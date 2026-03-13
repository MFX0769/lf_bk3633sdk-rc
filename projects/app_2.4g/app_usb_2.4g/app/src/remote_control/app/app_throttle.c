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

// 当前油门值

/**
 * @brief 油门模块初始化
 */
void app_throttle_init(void) {
    hall_hw_config_t hw_config = {
        .adc_channel = 1,
        .en_ctrl = {
            .gpio = HALL_PWR_EN,
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = HALL_PWR_EN,
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
 */
void app_throttle_update(uint16_t *throttle_value, uint8_t *throttle_changed_flag) {

    //上次油门值记录
    static uint16_t last_throttle_value = 0;

    uint16_t now_value = hall_sensor_read_throttle(&hall);
    *throttle_value = now_value;
    // 判断油门是否变化
    if(now_value != last_throttle_value) {
        *throttle_changed_flag = 1;
    } else {
        *throttle_changed_flag = 0;
    }
    //更新上次油门值
    last_throttle_value = now_value;
    
}
