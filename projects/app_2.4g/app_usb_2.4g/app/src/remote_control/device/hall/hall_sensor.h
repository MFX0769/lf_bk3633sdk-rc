/**
 ****************************************************************************************
 *
 * @file hall_sensor.h
 *
 * @brief 霍尔传感器面向对象接口
 *
 ****************************************************************************************
 */

#ifndef _HALL_SENSOR_H_
#define _HALL_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief GPIO控制配置
 */
typedef struct {
    uint8_t gpio;           // GPIO引脚
    uint8_t active_level;   // 有效电平（0或1）
} gpio_ctrl_t;

/**
 * @brief 霍尔传感器硬件配置
 */
typedef struct {
    uint8_t      adc_channel;   // ADC通道号
    gpio_ctrl_t  en_ctrl;    // 使能控制gpio
    gpio_ctrl_t  power_ctrl; // 电源控制gpio
} hall_hw_config_t;

/**
 * @brief 滤波器配置
 */
typedef struct {
    uint8_t  window_size;         // 滑动窗口大小
    uint16_t *window_buf;         // 窗口缓冲区指针
    uint8_t  window_index;        // 当前窗口索引
    uint32_t window_sum;          // 窗口累加和
} hall_filter_t;

/**
 * @brief 映射曲线类型
 */
typedef enum {
    HALL_MAP_LINEAR = 0,        // 线性映射
    HALL_MAP_EXPONENTIAL,       // 指数映射（加速响应）
    HALL_MAP_LOGARITHMIC,       // 对数映射（平滑响应）
    HALL_MAP_S_CURVE,           // S曲线映射（中间敏感）
} hall_map_type_t;

/**
 * @brief 霍尔映射配置
 */
typedef struct {
    uint16_t adc_min;           // ADC最小值（下限）
    uint16_t adc_max;           // ADC最大值（上限）
    uint16_t deadzone_low;      // 下限死区半径  死区范围[adc_min - deadzone_low, adc_min + deadzone_low]
    uint16_t deadzone_high;     // 上限死区半径  死区范围 [adc_max - deadzone_high, adc_max + deadzone_high]
    uint16_t throttle_min;      // 油门输出最小值（通常0）
    uint16_t throttle_max;      // 油门输出最大值（通常100或1000）
    hall_map_type_t map_type;   // 映射类型
    bool     reverse;           // 是否反向（true: ADC大->油门小）
} hall_map_config_t;

/**
 * @brief 霍尔传感器对象
 */
typedef struct {
    hall_hw_config_t  hw;       // 硬件配置
    hall_filter_t     filter;   // 滤波器
    hall_map_config_t map;      // 映射配置
    bool              enabled;  // 已使能标志位
} hall_sensor_t;

// ============================================================================
// 内部层接口（不直接调用，由高层API内部使用）
// ============================================================================

/**
 * @brief 初始化硬件层（内部使用）
 */
void hall_hw_init(const hall_hw_config_t *config);

/**
 * @brief 使能硬件（内部使用）
 */
void hall_hw_enable(const hall_hw_config_t *config);

/**
 * @brief 失能硬件（内部使用）
 */
void hall_hw_disable(const hall_hw_config_t *config);

/**
 * @brief 读取ADC（内部使用）
 */
uint16_t hall_hw_read_adc(const hall_hw_config_t *config);

/**
 * @brief 初始化滤波器（内部使用）
 * @param filter 滤波器对象指针
 * @param buf 滤波缓冲区（uint16_t数组）
 * @param size 缓冲区大小（元素个数，非字节数）
 * @note 滤波器使用滑动窗口平均算法，窗口大小即为size
 */
void hall_filter_init(hall_filter_t *filter, uint16_t *buf, uint8_t size);

/**
 * @brief 重置滤波器（内部使用）
 */
void hall_filter_reset(hall_filter_t *filter);

/**
 * @brief 更新滤波器（内部使用）
 * @param filter 滤波器对象指针
 * @param raw_value 新的原始ADC值
 * @return 滤波后的平均值
 * @note 使用滑动窗口平均算法，返回窗口内所有值的算术平均值
 */
uint16_t hall_filter_update(hall_filter_t *filter, uint16_t raw_value);

/**
 * @brief 初始化映射配置（内部使用）
 */
void hall_map_init_default(hall_map_config_t *map);

/**
 * @brief ADC映射为油门（内部使用）
 */
uint16_t hall_map_adc_to_throttle(const hall_map_config_t *map, uint16_t adc_value);

/**
 * @brief 传感器对象接口（推荐使用的高层API）
 */
/**
 * @brief 初始化霍尔传感器对象
 * @param sensor 传感器对象指针
 * @param hw_config 硬件配置
 * @param map_config 映射配置（传NULL使用默认配置）
 * @param filter_buf 滤波器缓冲区（uint16_t数组）
 * @param filter_size 滤波器窗口大小（元素个数，建议4-16）
 * @note filter_size表示滑动窗口的大小，值越大滤波效果越平滑但响应越慢
 */
void hall_sensor_init(hall_sensor_t *sensor,
                       hall_hw_config_t *hw_config,
                       hall_map_config_t *map_config,
                      uint16_t *filter_buf,
                      uint8_t filter_size);

/**
 * @brief 配置映射参数
 * @param sensor 传感器对象指针
 * @param map 映射配置
 */
void hall_sensor_config_map(hall_sensor_t *sensor, const hall_map_config_t *map);

/**
 * @brief 读取原始ADC值
 * @param sensor 传感器对象指针
 * @return 原始ADC值
 */
uint16_t hall_sensor_read_raw(hall_sensor_t *sensor);

/**
 * @brief 读取滤波后的ADC值
 * @param sensor 传感器对象指针
 * @return 滤波后的ADC值
 */
uint16_t hall_sensor_read_filtered(hall_sensor_t *sensor);

/**
 * @brief 读取油门值（经过滤波和映射）
 * @param sensor 传感器对象指针
 * @return 油门值（0-throttle_max）
 */
uint16_t hall_sensor_read_throttle(hall_sensor_t *sensor);

/**
 * @brief 重置传感器（清空滤波器）
 * @param sensor 传感器对象指针
 */
void hall_sensor_reset(hall_sensor_t *sensor);

/**
 * @brief 使能传感器
 * @param sensor 传感器对象指针
 */
void hall_sensor_enable(hall_sensor_t *sensor);

/**
 * @brief 失能传感器
 * @param sensor 传感器对象指针
 */
void hall_sensor_disable(hall_sensor_t *sensor);

#endif // _HALL_SENSOR_H_
