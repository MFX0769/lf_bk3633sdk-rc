/**
 ****************************************************************************************
 * @file app_throttle.h
 * @brief 油门控制模块（霍尔读取 + 油门映射）
 ****************************************************************************************
 */
#ifndef __APP_THROTTLE_H__
#define __APP_THROTTLE_H__

#include <stdint.h>

/**
 * @brief 油门模块初始化（初始化霍尔传感器）
 */
void app_throttle_init(void);

/**
 * @brief 油门值更新（50ms周期调用）
 * @param throttle_value 输出：当前油门值
 * @param throttle_changed_flag 输出：油门是否发生变化（1=变化，0=未变化）
 */
void app_throttle_update(uint16_t *throttle_value, uint8_t *throttle_changed_flag);

/**
 * @brief 获取当前油门值
 * @return 油门值 0-100
 */
uint16_t app_throttle_get(void);

#endif // __APP_THROTTLE_H__
