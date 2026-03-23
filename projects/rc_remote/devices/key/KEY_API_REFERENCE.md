# Key Scan Driver API Documentation

按键扫描驱动 - 事件驱动框架，支持消抖、长按、组合键

---

## 快速开始

```c
#include "key_scan.h"

// 1. 配置按键
const key_config_t my_keys[] = {
    {KEY_ID_LEFT,  Port_Pin(0, 1), 1000, false},
    {KEY_ID_RIGHT, Port_Pin(0, 2), 1000, false},
    {KEY_ID_PAIR,  Port_Pin(1, 5), 1500, false},
    {KEY_ID_POWER, Port_Pin(1, 3), 2000, false},
};

// 2. 回调函数（处理所有按键事件）
static uint8_t power_step = 0;
static uint32_t wait_timer = 0;

void key_event_handler(key_id_t id, key_event_t event) {
    // 左键：短按5次进入配对
    static uint8_t left_click_cnt = 0;
    if (id == KEY_ID_LEFT && event == KEY_EVT_SHORT_PRESS) {
        left_click_cnt++;
        if (left_click_cnt >= 5) {
            enter_pairing_mode();
            left_click_cnt = 0;
        }
    }

    // 电源键：短按+长按关机
    if (id == KEY_ID_POWER) {
        switch (power_step) {
            case 0:
                if (event == KEY_EVT_SHORT_PRESS) {
                    power_step = 1;
                    wait_timer = 50; // 500ms窗口
                }
                break;
            case 1:
                if (event == KEY_EVT_DOWN) power_step = 2;
                else if (wait_timer == 0) power_step = 0;
                break;
            case 2:
                if (event == KEY_EVT_LONG_PRESS) {
                    power_off();
                    power_step = 0;
                }
                break;
        }
    }

    // 配对键+右键：组合长按清除配对
    if (event == KEY_EVT_LONG_PRESS) {
        if (key_check_combo((1 << KEY_ID_PAIR) | (1 << KEY_ID_RIGHT))) {
            clear_pairing();
        }
    }
}

// 3. 初始化
void app_init(void) {
    key_init(my_keys, 4);
    key_register_callback(key_event_handler);
}

// 4. 定时调用（10ms）
while (1) {
    uint32_t now = Get_SysTick_ms();
    if (now - ts >= 10) {
        ts = now;
        key_scan(10);
        if (power_step == 1 && wait_timer > 0) wait_timer--;
    }
}
```

---

## 架构

```
用户层：初始化 + 提供回调函数
   ↓
框架层：key_scan()定时调用，自动检测事件并触发回调
   ↓
内部：消抖(20ms)判断 + 长按检测 + GPIO状态监测
```

---

## 数据类型

```c
// 按键ID（根据实际修改）
typedef enum {
    KEY_ID_LEFT, KEY_ID_RIGHT, KEY_ID_PAIR, KEY_ID_POWER, KEY_ID_MAX
} key_id_t;

// 事件类型
typedef enum {
    KEY_EVT_NONE,           // 无事件
    KEY_EVT_DOWN,           // 按下瞬间
    KEY_EVT_UP,             // 释放瞬间
    KEY_EVT_SHORT_PRESS,    // 短按（释放后触发）
    KEY_EVT_LONG_PRESS,     // 长按触发
    KEY_EVT_LONG_HOLD,      // 长按保持（连发）
    KEY_EVT_LONG_RELEASE,   // 长按后释放
} key_event_t;

// 配置结构
typedef struct {
    key_id_t id;             // 按键ID
    gpio_num_t pin;          // GPIO引脚
    uint16_t long_press_ms;  // 长按时间(ms)，0=默认1000ms
    bool active_level;       // 有效电平：false=低电平有效（推荐）
} key_config_t;

// 回调函数类型
typedef void (*key_callback_t)(key_id_t id, key_event_t event);
```

---

## API

### 用户API

```c
// 初始化
void key_init(const key_config_t *p_config, uint8_t count);

// 注册回调
void key_register_callback(key_callback_t cb);

// 周期调用（10ms）
void key_scan(uint16_t period_ms);
```

### 辅助API

```c
// 查询按键是否按下（用于组合键）
bool key_is_pressed(key_id_t key);

// 检测组合键
bool key_check_combo(uint32_t key_mask);
// 示例：key_check_combo((1 << KEY_ID_LEFT) | (1 << KEY_ID_RIGHT))
```

---

## 配置指南

| 项目 | 推荐值 | 说明 |
|------|--------|------|
| 有效电平 | false | 按键接GND，GPIO上拉 |
| 长按时间 | 1000ms | 普通键；电源键2000ms |
| 扫描周期 | 10ms | 平衡响应和CPU占用 |

---

## 注意事项

1. **必须周期调用 key_scan()**，否则无法检测事件
2. **回调中避免阻塞**（如Delay_ms），应设置标志位在主循环处理
3. **事件触发规则**：
   - SHORT_PRESS：释放后触发一次
   - LONG_PRESS：达到长按时间触发一次
   - LONG_HOLD：长按后每次扫描都触发（连发）

---

## 版本历史

- **v1.0** (2026-03-18) - 初始版本
