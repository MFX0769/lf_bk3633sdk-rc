# Battery Management Driver API Documentation

电池管理驱动 - 面向对象设计，支持电压检测、SOC计算、充电状态检测、开关机管理

---

## 驱动架构

```
┌─────────────────────────────────────────┐
│         应用层 API                       │
│  bat_manage_init()                      │
│  bat_manage_update()                    │  ← 周期调用
│  bat_manage_power_on/off()              │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         数据处理层                       │
│  • ADC → 电压转换                        │
│  • 电压 → SOC映射                        │
│  • 充电状态解析                          │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│         硬件层                           │
│  • ADC采样（使能→采样→关闭）             │
│  • BQ24040状态读取（PGOOD+CHRG）         │
│  • 电源控制（PWR_EN）                    │
└─────────────────────────────────────────┘
```

**设计特点：**
- 面向对象：配置结构体 + 操作函数
- 省电设计：ADC分压电路仅在ADC采样时开启，避免漏电流

---

## 数据类型

### bat_chg_state_t
充电状态枚举（BQ24040芯片）

```c
typedef enum {
    CHG_STATE_NO_POWER = 0,   // 无外部电源
    CHG_STATE_CHARGING,       // 充电中
    CHG_STATE_FULL,           // 充满
    CHG_STATE_FAULT,          // 故障
} bat_chg_state_t;
```

**BQ24040状态表：**
| PGOOD | CHRG | 状态 |
|-------|------|------|
| LOW | LOW | 充电中 |
| LOW | HIGH | 充满 |
| HIGH | HIGH | 无电源 |
| HIGH | LOW | 故障 |

### bat_hw_config_t
硬件配置结构体

```c
typedef struct {
    uint8_t  pwr_en_gpio;         // SYS_PWR_EN 引脚
    uint8_t  pwr_key_gpio;        // 电源按键输入引脚
    uint8_t  adc_channel;         // ADC通道
    uint8_t  adc_pwr_en_gpio;     // ADC分压电阻使能引脚
    uint8_t  pgood_gpio;          // PGOOD引脚（低=电源好）
    uint8_t  chrg_gpio;           // CHRG引脚（低=充电中）
    float    divider_ratio;       // 分压系数 V_adc = V_bat * ratio
    uint16_t bat_empty_mv;        // 空电电压 (如 3000mV)
    uint16_t bat_full_mv;         // 满电电压 (如 4200mV)
} bat_hw_config_t;
```

### bat_data_t
电池状态数据

```c
typedef struct {
    uint16_t         adc_raw;     // ADC原始值
    uint16_t         voltage_mv;  // 电池电压 (mV)
    uint8_t          soc;         // 电量百分比 0-100
    bat_chg_state_t  chg_state;   // 充电状态
} bat_data_t;
```

### bat_manager_t
电池管理对象

```c
typedef struct {
    bat_hw_config_t  hw;          // 硬件配置
    bat_data_t       data;        // 状态数据
    uint8_t          initialized; // 初始化标志
} bat_manager_t;
```

---

## API 参考

### 初始化

#### bat_manage_init()
```c
void bat_manage_init(bat_manager_t *bat, const bat_hw_config_t *hw);
```
初始化电池管理模块，配置GPIO和ADC。

**参数：**
- `bat`: 电池管理对象指针
- `hw`: 硬件配置指针

---

### 数据更新

#### bat_manage_update()
```c
void bat_manage_update(bat_manager_t *bat);
```
更新电池状态（ADC采样 + 充电检测）。**必须周期调用**。

**更新内容：**
- `data.adc_raw`: ADC原始值
- `data.voltage_mv`: 电池电压
- `data.soc`: 电量百分比
- `data.chg_state`: 充电状态

**示例：**
```c
// 在100ms定时器中调用
void timer_100ms_task(void) {
    bat_manage_update(&battery);
}
```

---

### 电源控制

#### bat_manage_power_on()
```c
void bat_manage_power_on(bat_manager_t *bat);
```
锁定电源（上电后马上调用，保持系统供电）。

#### bat_manage_power_off()
```c
void bat_manage_power_off(bat_manager_t *bat);
```
关机（释放电源使能）。

---

## 快速开始

### 场景1：基本使用

```c
#include "bat_manage.h"

// 1. 定义对象
bat_manager_t battery;

// 2. 初始化
void app_bat_init(void) {
    bat_hw_config_t hw = {
        .pwr_en_gpio = Port_Pin(1, 4),      // 电源使能引脚
        .pwr_key_gpio = Port_Pin(1, 3),     // 电源按键引脚
        .adc_channel = 3,                   // ADC通道
        .adc_pwr_en_gpio = Port_Pin(1, 5),  // ADC分压电路开关
        .pgood_gpio = Port_Pin(0, 6),       // BQ24040 PGOOD
        .chrg_gpio = Port_Pin(0, 7),        // BQ24040 CHRG
        .divider_ratio = 0.207f,            // 分压比 47K/(180K+47K)
        .bat_empty_mv = 3000,               // 空电电压
        .bat_full_mv = 4200,                // 满电电压
    };

    bat_manage_init(&battery, &hw);
    bat_manage_power_on(&battery);  // 锁定电源
}

// 3. 周期更新（如100ms）
void app_loop(void) {
    bat_manage_update(&battery);

    uart_printf("电压: %dmV, 电量: %d%%, 充电: %d\r\n",
        battery.data.voltage_mv,
        battery.data.soc,
        battery.data.chg_state);
}

```

### 场景2：低电量报警

```c
void check_low_battery(void) {
    bat_manage_update(&battery);

    if (battery.data.soc < 10) {
        uart_printf("低电量警告！\r\n");
        led_blink_fast();
    }

    if (battery.data.soc < 5) {
        uart_printf("电量过低，即将关机\r\n");
        bat_manage_power_off(&battery);
    }
}
```

### 场景3：充电状态显示

```c
void display_charging_status(void) {
    bat_manage_update(&battery);

    switch(battery.data.chg_state) {
        case CHG_STATE_NO_POWER:
            oled_print("电池供电");
            break;
        case CHG_STATE_CHARGING:
            oled_print("充电中 %d%%", battery.data.soc);
            led_blink_slow();
            break;
        case CHG_STATE_FULL:
            oled_print("充电完成");
            led_on();
            break;
        case CHG_STATE_FAULT:
            oled_print("充电故障");
            led_blink_fast();
            break;
    }
}
```

---

## 配置指南

### 1. 计算分压比

**电路：**
```
V_bat ──┬── R_up (180K) ──┬── ADC
        │                 │
       GND            R_down (47K)
```

**公式：**
```
divider_ratio = R_down / (R_up + R_down)
              = 47K / (180K + 47K)
              = 0.207
```

### 2. 确定电压范围

| 电池类型 | 空电电压 | 满电电压 |
|---------|---------|---------|
| 单节锂电池 | 3000mV | 4200mV |
| 磷酸铁锂 | 2500mV | 3650mV |
| 两节串联 | 6000mV | 8400mV |

### 3. 更新周期选择

| 周期 | 功耗 | 响应速度 | 适用场景 |
|------|------|---------|---------|
| 100ms | 高 | 快 | 调试 |
| 500ms | 中 | 中 | 推荐 |
| 1000ms | 低 | 慢 | 省电优先 |

---

## 注意事项

1. **必须先锁定电源**
   ```c
   bat_manage_init(&battery, &hw);
   bat_manage_power_on(&battery);  // 必须调用，否则会掉电
   ```

2. **ADC分压使能省电设计**
   - 仅采样时使能分压电阻
   - 采样完成后立即关闭
   - 避免持续漏电流

3. **SOC计算为线性映射**
   - 实际锂电池放电曲线非线性
   - 如需精确SOC，建议使用查表法

4. **充电检测引脚配置**
   ```c
   // BQ24040为开漏输出，需要上拉
   gpio_config(hw->pgood_gpio, GPIO_INPUT, GPIO_PULL_HIGH);
   gpio_config(hw->chrg_gpio, GPIO_INPUT, GPIO_PULL_HIGH);
   ```

---

## 故障排查

| 问题 | 可能原因 | 解决方法 |
|------|---------|---------|
| 电压读数为0 | ADC配置错误 | 检查通道号和分压使能 |
| 电压偏高/偏低 | 分压比错误 | 重新计算divider_ratio |
| SOC不准确 | 电压范围错误 | 校准bat_empty_mv和bat_full_mv |
| 充电状态错误 | 引脚配置错误 | 检查PGOOD和CHRG引脚 |
| 系统掉电 | 未调用power_on | 上电后立即锁定电源 |

---

## 版本历史

- **v1.0** (2026-03-23)
  - 初始版本
  - 支持电压检测、SOC计算
  - BQ24040充电状态检测
  - 开关机管理
