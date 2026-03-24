/**
 ****************************************************************************************
 *
 * @file hall_sensor.c
 *
 * @brief 霍尔传感器
 *
 ****************************************************************************************
 */

#include "hall_sensor.h"
#include "adc.h"
#include "user_config.h"
#include "drv_gpio.h"
#include <string.h>

static void delay_us(int num)
{
    int x, y;
    for(y = 0; y < num; y++)
    {
        for(x = 0; x < 5; x++);
    }
}

// ============================================================================
// 硬件层实现
// ============================================================================

void hall_hw_init(const hall_hw_config_t *config)
{
    // 配置GPIO
    gpio_config(config->en_ctrl.gpio, GPIO_OUTPUT, GPIO_PULL_NONE);
    gpio_config(config->power_ctrl.gpio, GPIO_OUTPUT, GPIO_PULL_NONE);

    // 初始状态：失能、下电
    gpio_set(config->en_ctrl.gpio, !config->en_ctrl.active_level);      // 失能（非有效电平）
    gpio_set(config->power_ctrl.gpio, !config->power_ctrl.active_level); // 下电（非有效电平）

    // 初始化ADC
    adc_init(config->adc_channel, 1);
}

void hall_hw_enable(const hall_hw_config_t *config)
{
    gpio_set(config->en_ctrl.gpio, config->en_ctrl.active_level);  // 使能（有效电平）
    //gpio_set(config->power_ctrl.gpio, config->power_ctrl.active_level);
    delay_us(5);
}

void hall_hw_disable(const hall_hw_config_t *config)
{
    gpio_set(config->en_ctrl.gpio, !config->en_ctrl.active_level);  // 失能（非有效电平）
    //gpio_set(config->power_ctrl.gpio, !config->power_ctrl.active_level);
    //uart_printf("hall_hw_disable:gpio=%d,level=%d\r\n", config->power_ctrl.gpio, !config->power_ctrl.active_level);
    delay_us(5);
}

uint16_t hall_hw_read_adc(const hall_hw_config_t *config)
{
    uint16_t adc_value;

    // 初始化ADC通道
    adc_init(config->adc_channel, 1);
    delay_us(5);

    // 使能霍尔传感器
    hall_hw_enable(config);

    // 读取ADC值
    adc_value = adc_get_value(config->adc_channel, 1);

    // 关闭霍尔传感器
    hall_hw_disable(config);

    // 去初始化ADC
    adc_deinit(config->adc_channel);

    return adc_value;
}

// ============================================================================
// 滤波器实现
// ============================================================================

void hall_filter_init(hall_filter_t *filter, uint16_t *buf, uint8_t size)
{
    if (size == 0) size = 1;  // 最小为1
    if (size > 128) size = 128;  // 最大为128

    filter->window_buf = buf;
    filter->window_size = size;
    filter->window_index = 0;
    filter->window_sum = 0;

    // 清空缓冲区
    memset(buf, 0, size * sizeof(uint16_t));
}

void hall_filter_reset(hall_filter_t *filter)
{
    filter->window_index = 0;
    filter->window_sum = 0;
    memset(filter->window_buf, 0, filter->window_size * sizeof(uint16_t));
}

uint16_t hall_filter_update(hall_filter_t *filter, uint16_t raw_value)
{
    // 减去即将被替换的旧值
    filter->window_sum -= filter->window_buf[filter->window_index];

    // 加上新值
    filter->window_sum += raw_value;

    // 更新窗口
    filter->window_buf[filter->window_index] = raw_value;
    filter->window_index = (filter->window_index + 1) % filter->window_size;

    // 返回平均值（使用除法）
    return (uint16_t)(filter->window_sum / filter->window_size);
}

// ============================================================================
// 映射层实现
// ============================================================================

void hall_map_init_default(hall_map_config_t *map)
{
    map->adc_min = 100;           // ADC最小值
    map->adc_max = 900;           // ADC最大值
    map->deadzone_low = 20;       // 下限死区20
    map->deadzone_high = 20;      // 上限死区20
    map->throttle_min = 0;        // 油门最小0
    map->throttle_max = 100;      // 油门最大100
    map->map_type = HALL_MAP_LINEAR;
    map->reverse = false;
}

/**
 * @brief 线性映射
 */
static uint16_t map_linear(uint16_t input, uint16_t in_min, uint16_t in_max,
                          uint16_t out_min, uint16_t out_max)
{
    if (input <= in_min) return out_min;
    if (input >= in_max) return out_max;

    uint32_t range_in = in_max - in_min;
    uint32_t range_out = out_max - out_min;
    uint32_t value = input - in_min;

    return out_min + (uint16_t)((value * range_out) / range_in);
}

/**
 * @brief 指数映射（加速响应，低端不敏感，高端敏感）
 */
static uint16_t map_exponential(uint16_t input, uint16_t in_min, uint16_t in_max,
                               uint16_t out_min, uint16_t out_max)
{
    if (input <= in_min) return out_min;
    if (input >= in_max) return out_max;

    // 归一化到0-100
    uint32_t normalized = ((uint32_t)(input - in_min) * 100) / (in_max - in_min);

    // 平方映射 (x^2 / 100)
    uint32_t squared = (normalized * normalized) / 100;

    // 映射到输出范围
    uint32_t range_out = out_max - out_min;
    return out_min + (uint16_t)((squared * range_out) / 100);
}

/**
 * @brief 对数映射（平滑响应，低端敏感，高端不敏感）
 * 使用整数平方根算法
 */
static uint16_t map_logarithmic(uint16_t input, uint16_t in_min, uint16_t in_max,
                               uint16_t out_min, uint16_t out_max)
{
    if (input <= in_min) return out_min;
    if (input >= in_max) return out_max;

    // 归一化到0-10000（扩大100倍以提高精度）
    uint32_t normalized = ((uint32_t)(input - in_min) * 10000) / (in_max - in_min);

    // 整数平方根（牛顿迭代法）
    // sqrt(normalized)
    uint32_t x = normalized;
    uint32_t y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + normalized / x) / 2;
    }

    // x 现在是 sqrt(normalized)
    // 归一化回0-100范围：sqrt(10000) = 100
    uint32_t sqrt_result = (x * 100) / 100;  // sqrt(10000) = 100

    // 映射到输出范围
    uint32_t range_out = out_max - out_min;
    return out_min + (uint16_t)((sqrt_result * range_out) / 100);
}

/**
 * @brief S曲线映射（中间敏感，两端平缓）
 */
static uint16_t map_s_curve(uint16_t input, uint16_t in_min, uint16_t in_max,
                           uint16_t out_min, uint16_t out_max)
{
    if (input <= in_min) return out_min;
    if (input >= in_max) return out_max;

    // 归一化到0-100
    int32_t normalized = ((int32_t)(input - in_min) * 100) / (in_max - in_min);

    // S曲线: y = 3x^2 - 2x^3 (平滑的0到1曲线)
    // 简化为: y = x * (300 - 2*x^2) / 100
    int32_t x = normalized;
    int32_t s_value = (x * (300 - 2 * x * x / 100)) / 100;

    if (s_value < 0) s_value = 0;
    if (s_value > 100) s_value = 100;

    // 映射到输出范围
    uint32_t range_out = out_max - out_min;
    return out_min + (uint16_t)((s_value * range_out) / 100);
}

uint16_t hall_map_adc_to_throttle(const hall_map_config_t *map, uint16_t adc_value)
{
    uint16_t input = adc_value;

    // 反向处理
    if (map->reverse) {
        if (adc_value <= map->adc_min) {
            input = map->adc_max;
        } else if (adc_value >= map->adc_max) {
            input = map->adc_min;
        } else {
            input = map->adc_max - (adc_value - map->adc_min);
        }
    }

    // 双端死区处理（中心对称）
    // 下限死区：[adc_min - deadzone_low, adc_min + deadzone_low]
    int32_t deadzone_low_min = (int32_t)map->adc_min - map->deadzone_low;
    int32_t deadzone_low_max = (int32_t)map->adc_min + map->deadzone_low;

    // 上限死区：[adc_max - deadzone_high, adc_max + deadzone_high]
    int32_t deadzone_high_min = (int32_t)map->adc_max - map->deadzone_high;
    int32_t deadzone_high_max = (int32_t)map->adc_max + map->deadzone_high;

    // 下限死区：在死区范围内，输出最小油门
    if (input >= deadzone_low_min && input <= deadzone_low_max) {
        return map->throttle_min;
    }

    // 上限死区：在死区范围内，输出最大油门
    if (input >= deadzone_high_min && input <= deadzone_high_max) {
        return map->throttle_max;
    }

    // 调整输入范围（去除两端死区）
    uint16_t adjusted_min = deadzone_low_max;
    uint16_t adjusted_max = deadzone_high_min;

    // 根据映射类型选择映射函数
    switch (map->map_type) {
        case HALL_MAP_LINEAR:
            return map_linear(input, adjusted_min, adjusted_max,
                            map->throttle_min, map->throttle_max);

        case HALL_MAP_EXPONENTIAL:
            return map_exponential(input, adjusted_min, adjusted_max,
                                 map->throttle_min, map->throttle_max);

        case HALL_MAP_LOGARITHMIC:
            return map_logarithmic(input, adjusted_min, adjusted_max,
                                 map->throttle_min, map->throttle_max);

        case HALL_MAP_S_CURVE:
            return map_s_curve(input, adjusted_min, adjusted_max,
                             map->throttle_min, map->throttle_max);

        default:
            return map_linear(input, adjusted_min, adjusted_max,
                            map->throttle_min, map->throttle_max);
    }
}

// ============================================================================
// 传感器对象实现
// ============================================================================

void hall_sensor_init(hall_sensor_t *sensor,
                       hall_hw_config_t *hw_config,
                       hall_map_config_t *map_config,
                      uint16_t *filter_buf,
                      uint8_t filter_size)
{
    
    uart_printf("sensor->hw: gpio=%d, active_level=%d\r\n", sensor->hw.power_ctrl.gpio, sensor->hw.power_ctrl.active_level);

    // 复制硬件配置
    sensor->hw = (hall_hw_config_t)*hw_config;
    //打印sensor->hw全部成员
    uart_printf("sensor->hw: gpio=%d, active_level=%d,adc_channel=%d\r\n", sensor->hw.power_ctrl.gpio, sensor->hw.power_ctrl.active_level, sensor->hw.adc_channel);
    
    uart_printf("hw_config=%d sensor->hw_new: %d\r\n",hw_config->adc_channel, sensor->hw.adc_channel);

    // 初始化硬件
    hall_hw_init(&sensor->hw);
    uart_printf("init1 :%d,%d\r\n", hw_config->power_ctrl.gpio, sensor->hw.power_ctrl.gpio);

    // 初始化滤波器
    hall_filter_init(&sensor->filter, filter_buf, filter_size);
    uart_printf("init2:%d,%d\r\n", hw_config->power_ctrl.gpio, sensor->hw.power_ctrl.gpio);
    // 初始化映射配置
    if (map_config != NULL) {
        // 使用用户提供的配置
        sensor->map = *map_config;
         uart_printf("init3:%d,%d\r\n", hw_config->power_ctrl.gpio, sensor->hw.power_ctrl.gpio);
    } else {
        // 使用默认配置
        hall_map_init_default(&sensor->map);
         uart_printf("init4:%d,%d\r\n", hw_config->power_ctrl.gpio, sensor->hw.power_ctrl.gpio);
    }
    uart_printf("ini5 :%d,%d\r\n", hw_config->power_ctrl.gpio, sensor->hw.power_ctrl.gpio);
    sensor->enabled = true;
}

void hall_sensor_config_map(hall_sensor_t *sensor, const hall_map_config_t *map)
{
    sensor->map = *map;
}

void hall_sensor_update(hall_sensor_t *sensor)
{
    if (!sensor->enabled) return;

    sensor->data.raw      = hall_hw_read_adc(&sensor->hw);
    sensor->data.filtered = hall_filter_update(&sensor->filter, sensor->data.raw);
    sensor->data.throttle = hall_map_adc_to_throttle(&sensor->map, sensor->data.filtered);
}

void hall_sensor_reset(hall_sensor_t *sensor)
{   
    
    hall_filter_reset(&sensor->filter);
}

void hall_sensor_enable(hall_sensor_t *sensor)
{
    hall_hw_enable(&sensor->hw);
    sensor->enabled = true;
}

void hall_sensor_disable(hall_sensor_t *sensor)
{   
    hall_hw_disable(&sensor->hw);
    sensor->enabled = false;
}
