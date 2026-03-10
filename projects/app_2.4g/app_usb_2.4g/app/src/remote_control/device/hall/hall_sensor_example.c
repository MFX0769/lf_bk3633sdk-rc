/**
 ****************************************************************************************
 * @file hall_sensor_example.c
 * @brief 霍尔传感器面向对象使用示例
 ****************************************************************************************
 */

#include "hall_sensor.h"
#include "my_drv_uart.h"
#include "timer_handler.h"
#include "drv_gpio.h"

// ============================================================================
// 示例1：基本使用（使用默认映射配置）
// ============================================================================

void example_basic_usage(void)
{
    // 1. 定义传感器对象和滤波器缓冲区
    static hall_sensor_t hall;
    static uint16_t filter_buf[8];  // 8点滑动窗口

    // 2. 配置硬件参数
    hall_hw_config_t hw_config = {
        .adc_channel = 2,
        .en_ctrl = {
            .gpio = Port_Pin(1, 2),
            .active_level = 0,      // 低电平使能
        },
        .power_ctrl = {
            .gpio = Port_Pin(1, 0),
            .active_level = 1,      // 高电平上电
        },
    };

    // 3. 初始化传感器（传NULL使用默认映射配置）
    hall_sensor_init(&hall, &hw_config, NULL, filter_buf, 8);

    // 4. 读取油门值
    uint16_t throttle = hall_sensor_read_throttle(&hall);
    uart_printf("Throttle: %d%%\r\n", throttle);
}

// ============================================================================
// 示例2：初始化时指定映射配置
// ============================================================================

void example_custom_mapping(void)
{
    static hall_sensor_t hall;
    static uint16_t filter_buf[8];

    // 硬件配置
    hall_hw_config_t hw_config = {
        .adc_channel = 2,
        .en_ctrl = {
            .gpio = Port_Pin(1, 2),
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = Port_Pin(1, 0),
            .active_level = 1,
        },
    };

    // 自定义映射配置
    hall_map_config_t map_config = {
        .adc_min = 100,              // ADC最小值
        .adc_max = 900,              // ADC最大值
        .deadzone_low = 30,          // 下限死区30
        .deadzone_high = 30,         // 上限死区30
        .throttle_min = 0,           // 油门0%
        .throttle_max = 100,         // 油门100%
        .map_type = HALL_MAP_LINEAR, // 线性映射
        .reverse = true,             // 反向
    };

    // 初始化时传入映射配置
    hall_sensor_init(&hall, &hw_config, &map_config, filter_buf, 8);

    // 读取油门值
    while(1){
        uint16_t throttle = hall_sensor_read_throttle(&hall);
        uart_printf("Throttle: %d%%\r\n", throttle);
    }
    
}

// ============================================================================
// 示例3：不同映射曲线对比
// ============================================================================

void example_mapping_curves(void)
{
    static hall_sensor_t hall;
    static uint16_t filter_buf[8];

    hall_hw_config_t hw_config = {
        .adc_channel = 2,
        .en_ctrl = {
            .gpio = Port_Pin(1, 2),
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = Port_Pin(1, 0),
            .active_level = 1,
        },
    };

    // 使用默认配置初始化
    hall_sensor_init(&hall, &hw_config, NULL, filter_buf, 8);

    // 读取滤波后的ADC值
    uint16_t adc_filtered = hall_sensor_read_filtered(&hall);

    // 测试不同映射曲线
    hall_map_config_t map;
    hall_map_init_default(&map);

    uart_printf("ADC: %d\r\n", adc_filtered);

    // 线性映射
    map.map_type = HALL_MAP_LINEAR;
    uint16_t linear = hall_map_adc_to_throttle(&map, adc_filtered);
    uart_printf("  Linear:      %d%%\r\n", linear);

    // 指数映射（加速响应）
    map.map_type = HALL_MAP_EXPONENTIAL;
    uint16_t exponential = hall_map_adc_to_throttle(&map, adc_filtered);
    uart_printf("  Exponential: %d%%\r\n", exponential);

    // 对数映射（平滑响应）
    map.map_type = HALL_MAP_LOGARITHMIC;
    uint16_t logarithmic = hall_map_adc_to_throttle(&map, adc_filtered);
    uart_printf("  Logarithmic: %d%%\r\n", logarithmic);

    // S曲线映射
    map.map_type = HALL_MAP_S_CURVE;
    uint16_t s_curve = hall_map_adc_to_throttle(&map, adc_filtered);
    uart_printf("  S-Curve:     %d%%\r\n", s_curve);
}

// ============================================================================
// 示例4：反向映射（ADC大 -> 油门小）
// ============================================================================

void example_reverse_mapping(void)
{
    static hall_sensor_t hall;
    static uint16_t filter_buf[8];

    hall_hw_config_t hw_config = {
        .adc_channel = 2,
        .en_ctrl = {
            .gpio = Port_Pin(1, 2),
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = Port_Pin(1, 0),
            .active_level = 1,
        },
    };

    // 配置反向映射
    hall_map_config_t map_config = {
        .adc_min = 100,
        .adc_max = 900,
        .deadzone = 20,
        .throttle_min = 0,
        .throttle_max = 100,
        .map_type = HALL_MAP_LINEAR,
        .reverse = true,  // 反向映射
    };

    // 初始化时传入反向映射配置
    hall_sensor_init(&hall, &hw_config, &map_config, filter_buf, 8);

    uint16_t throttle = hall_sensor_read_throttle(&hall);
    uart_printf("Throttle (reversed): %d%%\r\n", throttle);
}

// ============================================================================
// 示例5：周期性读取
// ============================================================================

void example_periodic_read(void)
{
    static hall_sensor_t hall;
    static uint16_t filter_buf[8];
    static uint32_t last_time = 0;
    static bool initialized = false;

    // 初始化（只执行一次）
    if (!initialized) {
        hall_hw_config_t hw_config = {
            .adc_channel = 2,
            .en_gpio = Port_Pin(1, 2),
            .power_gpio = Port_Pin(1, 0),
        };

        // 配置映射
        hall_map_config_t map_config = {
            .adc_min = 100,
            .adc_max = 900,
            .deadzone_low = 20,
            .deadzone_high = 20,
            .throttle_min = 0,
            .throttle_max = 100,
            .map_type = HALL_MAP_LINEAR,
            .reverse = false,
        };

        // 初始化时传入配置
        hall_sensor_init(&hall, &hw_config, &map_config, filter_buf, 8);

        initialized = true;
    }

    // 每100ms读取一次
    uint32_t now = Get_SysTick_ms();
    if (now - last_time >= 100) {
        last_time = now;

        uint16_t raw = hall_sensor_read_raw(&hall);
        uint16_t filtered = hall_sensor_read_filtered(&hall);
        uint16_t throttle = hall_sensor_read_throttle(&hall);

        uart_printf("Raw: %4d, Filtered: %4d, Throttle: %3d%%\r\n",
                   raw, filtered, throttle);
    }
}

// ============================================================================
// 示例6：多传感器实例
// ============================================================================

void example_multiple_sensors(void)
{
    // 定义两个传感器对象
    static hall_sensor_t hall1, hall2;
    static uint16_t filter_buf1[8], filter_buf2[8];

    // 传感器1配置
    hall_hw_config_t hw1 = {
        .adc_channel = 2,
        .en_gpio = Port_Pin(1, 2),
        .power_gpio = Port_Pin(1, 0),
    };
    hall_sensor_init(&hall1, &hw1, NULL, filter_buf1, 8);

    // 传感器2配置（假设有第二个霍尔传感器）
    hall_hw_config_t hw2 = {
        .adc_channel = 3,
        .en_gpio = Port_Pin(1, 3),
        .power_gpio = Port_Pin(1, 1),
    };
    hall_sensor_init(&hall2, &hw2, NULL, filter_buf2, 8);

    // 分别读取
    uint16_t throttle1 = hall_sensor_read_throttle(&hall1);
    uint16_t throttle2 = hall_sensor_read_throttle(&hall2);

    uart_printf("Throttle1: %d%%, Throttle2: %d%%\r\n", throttle1, throttle2);
}

// ============================================================================
// 示例7：完整应用示例
// ============================================================================

static hall_sensor_t g_hall;
static uint16_t g_filter_buf[8];

void hall_app_init(void)
{
    // 硬件配置
    hall_hw_config_t hw_config = {
        .adc_channel = 2,
        .en_ctrl = {
            .gpio = Port_Pin(1, 2),
            .active_level = 0,
        },
        .power_ctrl = {
            .gpio = Port_Pin(1, 0),
            .active_level = 1,
        },
    };

    // 配置映射（根据实际硬件调整）
    hall_map_config_t map_config = {
        .adc_min = 100,              // 根据实测调整
        .adc_max = 900,              // 根据实测调整
        .deadzone = 20,              // 死区
        .throttle_min = 0,
        .throttle_max = 100,
        .map_type = HALL_MAP_LINEAR, // 可选：EXPONENTIAL, LOGARITHMIC, S_CURVE
        .reverse = false,
    };

    // 初始化传感器
    hall_sensor_init(&g_hall, &hw_config, &map_config, g_filter_buf, 8);

    uart_printf("Hall sensor initialized\r\n");
}

void hall_app_task(void)
{
    static uint32_t last_time = 0;
    uint32_t now = Get_SysTick_ms();

    if (now - last_time >= 50) {  // 每50ms读取一次
        last_time = now;

        uint16_t throttle = hall_sensor_read_throttle(&g_hall);

        // 根据油门值执行相应操作
        if (throttle < 5) {
            uart_printf("Throttle: IDLE\r\n");
        } else if (throttle < 30) {
            uart_printf("Throttle: LOW (%d%%)\r\n", throttle);
        } else if (throttle < 70) {
            uart_printf("Throttle: MEDIUM (%d%%)\r\n", throttle);
        } else {
            uart_printf("Throttle: HIGH (%d%%)\r\n", throttle);
        }
    }
}
