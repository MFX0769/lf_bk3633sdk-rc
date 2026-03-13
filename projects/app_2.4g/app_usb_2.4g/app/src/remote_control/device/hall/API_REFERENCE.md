# Hall Sensor Driver API Documentation

霍尔传感器驱动 - 面向对象设计，支持多种映射曲线和双端死区

---

## 快速开始

```c
#include "hall_sensor.h"

// 1. 定义对象和缓冲区
hall_sensor_t hall;
static uint16_t filter_buf[8];

// 2. 初始化
void app_init(void) {
    // 硬件配置
    hall_hw_config_t hw = {
        .adc_channel = 2,
        .en_ctrl = {.gpio = Port_Pin(1, 2), .active_level = 0},
        .power_ctrl = {.gpio = Port_Pin(1, 0), .active_level = 1},
    };

    // 映射配置
    hall_map_config_t map = {
        .adc_min = 96,              // 实测最小值
        .adc_max = 919,             // 实测最大值
        .deadzone_low = 10,         // 下限死区半径
        .deadzone_high = 10,        // 上限死区半径
        .throttle_min = 0,
        .throttle_max = 100,
        .map_type = HALL_MAP_LINEAR,
        .reverse = true,            // ADC大→油门小
    };

    hall_sensor_init(&hall, &hw, &map, filter_buf, 8);
}

// 3. 读取油门值
void app_loop(void) {
    uint16_t throttle = hall_sensor_read_throttle(&hall);
    // 使用throttle值...
}
```

---

## 驱动架构

```
┌─────────────────────────────────────────┐
│         应用层 API                       │
│  hall_sensor_read_throttle()            │  ← 推荐使用
│  hall_sensor_read_filtered()            │
│  hall_sensor_read_raw()                 │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         映射层 (Mapping)                 │
│  • 线性/指数/对数/S曲线                  │
│  • 双端中心对称死区                      │
│  • 反向映射                              │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         滤波层 (Filter)                  │
│  • 滑动窗口平均                          │
│  • O(1)增量更新                          │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         硬件层 (HAL)                     │
│  • GPIO控制（可配置有效电平）            │
│  • ADC读取                               │
└─────────────────────────────────────────┘
```

**设计特点：**
- 数据行为分离：配置结构体 + 操作函数
- 统一接口：所有API使用 `hall_sensor_t*` 参数
- 多实例支持：可同时使用多个传感器

---

## 核心数据类型

### gpio_ctrl_t
GPIO控制配置（GPIO引脚+有效电平封装）

```c
typedef struct {
    uint8_t gpio;           // GPIO引脚（Port_Pin格式）
    uint8_t active_level;   // 有效电平：0=低电平有效，1=高电平有效
} gpio_ctrl_t;
```

### hall_hw_config_t
硬件配置

```c
typedef struct {
    uint8_t      adc_channel;   // ADC通道号
    gpio_ctrl_t  en_ctrl;       // 使能控制
    gpio_ctrl_t  power_ctrl;    // 电源控制
} hall_hw_config_t;
```

### hall_map_config_t
映射配置

```c
typedef struct {
    uint16_t adc_min;           // ADC下限中心值
    uint16_t adc_max;           // ADC上限中心值
    uint16_t deadzone_low;      // 下限死区半径 [adc_min±deadzone_low]
    uint16_t deadzone_high;     // 上限死区半径 [adc_max±deadzone_high]
    uint16_t throttle_min;      // 油门输出最小值（通常0）
    uint16_t throttle_max;      // 油门输出最大值（通常100）
    hall_map_type_t map_type;   // 映射类型
    bool     reverse;           // 反向映射
} hall_map_config_t;
```

### hall_map_type_t
映射曲线类型

```c
typedef enum {
    HALL_MAP_LINEAR = 0,      // 线性：均匀响应
    HALL_MAP_EXPONENTIAL,     // 指数：低端不敏感，高端敏感（竞速）
    HALL_MAP_LOGARITHMIC,     // 对数：低端敏感，高端不敏感（精细控制）
    HALL_MAP_S_CURVE,         // S曲线：中间敏感，两端平缓（舒适）
} hall_map_type_t;
```

### hall_sensor_t
传感器对象（封装所有状态）

```c
typedef struct {
    hall_hw_config_t  hw;       // 硬件配置
    hall_filter_t     filter;   // 滤波器
    hall_map_config_t map;      // 映射配置
    bool              enabled;  // 使能标志
} hall_sensor_t;
```

---

## API 参考

### 初始化

#### hall_sensor_init()
```c
void hall_sensor_init(hall_sensor_t *sensor,
                      const hall_hw_config_t *hw_config,
                      const hall_map_config_t *map_config,  // NULL=使用默认
                      uint16_t *filter_buf,
                      uint8_t filter_size);
```
初始化传感器对象。`map_config`传NULL使用默认配置。

### 读取数据

#### hall_sensor_read_throttle()
```c
uint16_t hall_sensor_read_throttle(hall_sensor_t *sensor);
```
读取油门值（经过滤波和映射）。**推荐使用**。

#### hall_sensor_read_filtered()
```c
uint16_t hall_sensor_read_filtered(hall_sensor_t *sensor);
```
读取滤波后的ADC值（未映射）。

#### hall_sensor_read_raw()
```c
uint16_t hall_sensor_read_raw(hall_sensor_t *sensor);
```
读取原始ADC值（未滤波）。

### 配置管理

#### hall_sensor_config_map()
```c
void hall_sensor_config_map(hall_sensor_t *sensor, const hall_map_config_t *map);
```
运行时修改映射配置。

#### hall_sensor_reset()
```c
void hall_sensor_reset(hall_sensor_t *sensor);
```
重置滤波器（清空历史数据）。

### 使能/失能控制

#### hall_sensor_enable()
```c
void hall_sensor_enable(hall_sensor_t *sensor);
```
使能传感器硬件。

#### hall_sensor_disable()
```c
void hall_sensor_disable(hall_sensor_t *sensor);
```
失能传感器硬件（省电）。

注意，有些型号的hall有单独的使能失能en引脚，可以只在每次读取时使能达到省电的目的；
但是有些引脚hall没有en失能控制，只能不断上电下电，可能导致不稳定。。。可以按需要修改hall_sensor_enable()和hall_sensor_disable()函数

---

## 核心组件设计

### 1. 滤波器（O(1)增量更新）

**算法：** 滑动窗口平均

**实现：**
```c
// 维护窗口累加和，每次更新只需：
sum -= old_value;      // 减去旧值
sum += new_value;      // 加上新值
average = sum >> 3;    // 除以窗口大小（位移优化）
```

**特点：**
- 时间复杂度：O(1)
- 窗口大小：自动取整到最接近的2的幂次
  - 传入10 → 实际使用8
  - 传入7 → 实际使用8
  - 传入3 → 实际使用4
- 推荐值：8（平衡响应速度和平滑度）

### 2. 双端中心对称死区

**原理：** 以adc_min/adc_max为中心，向两侧扩展死区

```
下限死区：[adc_min - deadzone_low, adc_min + deadzone_low] → 0%
上限死区：[adc_max - deadzone_high, adc_max + deadzone_high] → 100%
```

**作用：**
- 消除ADC漂移（如90→98，919→905）
- 稳定两端输出（0%和100%）

**配置建议：**
```c
deadzone = 实测漂移量 × 1.2  // 留20%余量
```

### 3. 映射曲线

#### 线性映射
```
y = x
```
- 均匀响应，最常用
- 适合大多数场景

#### 指数映射
```
y = x²
```
- 低端不敏感，高端敏感
- 适合竞速（精细控制高速）

#### 对数映射
```
y = √x
```
- 低端敏感，高端不敏感
- 适合精细控制低速

#### S曲线映射
```
y = 3x² - 2x³
```
- 中间敏感，两端平缓
- 适合舒适驾驶（平滑加减速）

**曲线对比：**
```
输入   线性  指数  对数  S曲线
0%     0%    0%    0%    0%
25%    25%   6%    50%   16%
50%    50%   25%   71%   50%
75%    75%   56%   87%   84%
100%   100%  100%  100%  100%
```

### 4. 反向映射

当硬件特性为"ADC大→油门小"时，设置 `reverse=true`：

```c
// 反向处理（在映射前）
if (reverse) {
    input = adc_max - (adc_value - adc_min);
}
```

---

## 使用场景

### 场景1：基本使用（默认配置）

```c
hall_hw_config_t hw = {
    .adc_channel = 2,
    .en_ctrl = {.gpio = Port_Pin(1, 2), .active_level = 0},
    .power_ctrl = {.gpio = Port_Pin(1, 0), .active_level = 1},
};

hall_sensor_init(&hall, &hw, NULL, filter_buf, 8);  // NULL=默认配置
```

### 场景2：自定义映射

```c
hall_map_config_t map = {
    .adc_min = 96,
    .adc_max = 919,
    .deadzone_low = 10,
    .deadzone_high = 10,
    .throttle_min = 0,
    .throttle_max = 100,
    .map_type = HALL_MAP_EXPONENTIAL,  // 指数映射
    .reverse = true,
};

hall_sensor_init(&hall, &hw, &map, filter_buf, 8);
```

### 场景3：运行时切换映射

```c
// 初始化时用线性
hall_sensor_init(&hall, &hw, &map, filter_buf, 8);

// 运行时切换到S曲线
map.map_type = HALL_MAP_S_CURVE;
hall_sensor_config_map(&hall, &map);
```

### 场景4：多传感器实例

```c
static hall_sensor_t hall1, hall2;
static uint16_t buf1[8], buf2[8];

hall_hw_config_t hw1 = {.adc_channel = 2, ...};
hall_hw_config_t hw2 = {.adc_channel = 3, ...};

hall_sensor_init(&hall1, &hw1, NULL, buf1, 8);
hall_sensor_init(&hall2, &hw2, NULL, buf2, 8);

uint16_t throttle1 = hall_sensor_read_throttle(&hall1);
uint16_t throttle2 = hall_sensor_read_throttle(&hall2);
```

---

## 配置指南

### 1. 确定ADC范围

```c
void calibrate_adc_range(void) {
    uint16_t min_val = 1023, max_val = 0;

    uart_printf("请将油门拨到最低位置...\r\n");
    for (int i = 0; i < 100; i++) {
        uint16_t val = hall_sensor_read_raw(&hall);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        Delay_ms(100);
    }
    uart_printf("最低位置: %d ~ %d\r\n", min_val, max_val);

    // 重复测试最高位置...
}
```

### 2. 选择死区大小

| 实测漂移 | 推荐死区 | 说明 |
|---------|---------|------|
| ±5      | 8       | 漂移小 |
| ±10     | 12      | 中等漂移（推荐） |
| ±15     | 18      | 漂移大 |

**公式：** `deadzone = 漂移量 × 1.2`

### 3. 选择映射曲线

| 场景 | 推荐曲线 | 说明 |
|------|---------|------|
| 新手/日常 | LINEAR | 均匀响应 |
| 竞速 | EXPONENTIAL | 高速精细控制 |
| 精细操作 | LOGARITHMIC | 低速精细控制 |
| 舒适驾驶 | S_CURVE | 平滑加减速 |

### 4. 选择滤波器大小

| 窗口大小 | 响应速度 | 平滑度 | 适用场景 |
|---------|---------|--------|---------|
| 4       | 快      | 低     | 竞速 |
| 8       | 中      | 中     | 日常（推荐） |
| 16      | 慢      | 高     | 稳定优先 |

---

## 注意事项

1. **滤波器缓冲区必须是静态或全局变量**
   ```c
   static uint16_t filter_buf[8];  // ✓ 正确
   uint16_t filter_buf[8];         // ✗ 错误（栈变量）
   ```

2. **窗口大小自动调整**
   ```c
   // 传入的size会自动取整到最接近的2的幂次
   hall_sensor_init(&hall, &hw, &map, buf, 8);   // 使用8
   hall_sensor_init(&hall, &hw, &map, buf, 10);  // 自动调整为8（更接近）
   hall_sensor_init(&hall, &hw, &map, buf, 7);   // 自动调整为8（更接近）
   hall_sensor_init(&hall, &hw, &map, buf, 3);   // 自动调整为4（更接近）
   hall_sensor_init(&hall, &hw, &map, buf, 12);  // 自动调整为16（更接近）
   ```

3. **死区不要重叠**
   ```c
   // 检查：(adc_min + deadzone_low) < (adc_max - deadzone_high)
   (96 + 10) < (919 - 10)  // ✓ 106 < 909
   ```

4. **死区总和不要太大**
   ```c
   // 建议：有效映射范围 > 总范围的80%
   有效范围 = (adc_max - deadzone_high) - (adc_min + deadzone_low)
   占比 = 有效范围 / (adc_max - adc_min)
   ```

---


## 故障排查

| 问题 | 可能原因 | 解决方法 |
|------|---------|---------|
| 油门值始终为0 | GPIO/ADC配置错误 | 检查硬件配置，用read_raw()测试 |
| 油门值抖动 | 滤波器太小 | 增大窗口（8→16） |
| 响应慢 | 滤波器太大 | 减小窗口（16→8） |
| 两端不稳定 | 死区太小 | 增大死区 |
| 映射异常 | 配置错误 | 检查adc_min/max和reverse |

---

## 版本历史

- **v1.0** (2026-03-10)
  - 面向对象重构
  - 双端中心对称死区防止漂移
  - 修复对数映射bug（使用牛顿迭代法）
  - GPIO控制支持可配置有效电平
  - O(1)滤波器优化
