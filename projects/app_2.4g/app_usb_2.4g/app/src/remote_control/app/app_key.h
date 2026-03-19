/**
 ****************************************************************************************
 * @file app_key.h
 * @brief Application layer key management
 ****************************************************************************************
 */
#ifndef __APP_KEY_H__
#define __APP_KEY_H__

#include <stdint.h>

/**
 * @brief 按键模块初始化
 */
void app_key_init(void);

/**
 * @brief 按键扫描（10ms周期调用）
 */
void app_key_scan(uint8_t ms_period);

/**
 * @brief 获取配对标志
 * @return 1=进入配对模式, 0=正常模式
 */
uint8_t app_key_get_pair_flag(void);

/**
 * @brief 获取配对标志指针（供Host_Pairing_Task直接操作）
 */
uint8_t* app_key_get_pair_flag_ptr(void);

/**
 * @brief 清除配对标志
 */
void app_key_clear_pair_flag(void);

#endif // __APP_KEY_H__
