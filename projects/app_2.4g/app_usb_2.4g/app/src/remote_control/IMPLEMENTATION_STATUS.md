# 低功耗框架实现总结

## 已完成的模块

### 1. app_shutdown (电源管理)
**文件**: `app/app_shutdown.c` + `app/app_shutdown.h`

**功能**:
- `app_shutdown_init()`: 拉高SYS_PWR_EN锁定电源（在RC_Scheduler_Init中调用）
- `app_shutdown_check()`: 200ms周期检测关机条件
  - 检测KEY_SYS_PWR长按3秒
  - TODO: 低电量自动关机
  - TODO: 链路断开超时关机
- `app_shutdown_trigger()`: 触发关机，拉低SYS_PWR_EN断电
- `app_shutdown_get_flag()`: 获取关机标志

**硬件逻辑**:
```
开机: 按下KEY_SYS_PWR → 硬件上电 → BK3633启动 → 代码拉高SYS_PWR_EN → 松开按键 → 系统保持供电
关机: 长按KEY_SYS_PWR 3秒 → 代码拉低SYS_PWR_EN → 系统断电
```

---

### 2. rc_scheduler (任务调度器)
**文件**: `app/rc_scheduler.c` + `app/rc_scheduler.h`

**功能**: 实现PDF低功耗框架的task_list调度

**任务列表**:
| 周期 | 任务函数 | 状态 |
|------|---------|------|
| 10ms | app_key_scan() | ✅ 已实现 |
| 20ms | RF_Service_Handler() | ✅ 已实现 |
| 50ms | app_hall_update() + app_throttle_set() | ⏳ TODO |
| 100ms | app_lcd_refresh() | ⏳ TODO |
| 200ms | app_shutdown_check() | ✅ 已实现 |
| 500ms | app_heartbeat_send() + app_check_link() | ⏳ TODO |
| 700ms | app_bat_status_process() | ⏳ TODO |
| 动态 | app_enter_sleep_with_wakeup_by_timer() | ✅ 已实现 |

**核心逻辑**:
```c
while(1) {
    // 执行所有到期任务
    // 计算下次最近任务的剩余时间
    // 进入睡眠，sleep_ms后唤醒
    RC_Scheduler_Task(&rc_sched);
}
```

---

### 3. app_key (按键管理)
**文件**: `app/app_key.c` + `app/app_key.h`

**功能**:
- `app_key_init()`: 初始化KEY1和KEY2
- `app_key_scan()`: 10ms周期扫描
- `app_key_get_pair_flag()`: 获取配对标志
- `app_key_clear_pair_flag()`: 清除配对标志

**按键定义**:
- **KEY1** (P1.6): 功能按键1
- **KEY2** (P0.2): 功能按键2，连续5次短按进入配对
- **KEY_SYS_PWR** (P3.7): 电源按键，长按3秒关机（在app_shutdown中处理）

**注意**: 按键有效电平是**高电平**（按下=1）

---

### 4. app_sleep (睡眠管理)
**文件**: `app/app_sleep.c` + `app/app_sleep.h`

**功能**:
- `app_enter_sleep_with_wakeup_by_timer(sleep_ms, flag)`: 进入深度睡眠
  - 使用Timer1_1作为唤醒源
  - 调用`cpu_24_reduce_voltage_sleep()`
  - 功耗: 860uA

---

## 待实现的模块

### 1. app_throttle (油门控制) - P1
**需要实现**:
```c
void app_throttle_init(void);
void app_throttle_set(void);  // 50ms调用
```
**逻辑**: 读取霍尔传感器 → 通过RF发送油门命令

---

### 2. app_heartbeat (心跳) - P2
**需要实现**:
```c
void app_heartbeat_init(void);
void app_heartbeat_send(void);  // 500ms调用
```
**逻辑**: 通过RF发送心跳包

---

### 3. app_link (链路检测) - P2
**需要实现**:
```c
void app_link_init(void);
void app_link_check(void);  // 500ms调用
uint8_t app_link_is_connected(void);
```
**逻辑**: 超时检测，更新链路状态

---

### 4. app_battery (电池管理) - P2
**需要实现**:
```c
void app_battery_init(void);
void app_battery_query(void);  // 700ms调用
void app_battery_process(void);
uint8_t app_battery_get_soc(void);
```
**逻辑**: 查询电池SOC，CRC校验

---

### 5. app_lcd (LCD显示) - P4
**需要实现**:
```c
void app_lcd_init(void);
void app_lcd_refresh(void);  // 100ms调用
```
**逻辑**: 显示油门、电池、链路状态（暂缓，等确定型号）

---

## 主函数集成

```c
int main(void)
{
    // 基础初始化
    icu_init();
    intc_init();
    uart_init(115200);
    xvr_reg_initial();
    Timer_Handler_Init();
    flash_init();
    mcu_clk_switch(MCU_CLK_16M);
    GLOBAL_INT_START();

    uart_printf("System Boot...\r\n");

    // 应用模块初始化
    app_key_init();
    RF_Handler_Init();

    // 初始化调度器（会调用app_shutdown_init锁定电源）
    RC_Scheduler_t rc_sched;
    RC_Scheduler_Init(&rc_sched);

    uart_printf("System Ready\r\n");

    // 主循环
    while(1) {
        RC_Scheduler_Task(&rc_sched);
    }
}
```

---

## 功耗优化

### 当前实现
- **Active状态**: 执行task_list，动态睡眠
- **Sleep功耗**: 860uA（Timer唤醒）

### 预期效果
- 按键/霍尔活动时: Active状态，正常功耗
- 无活动时: 自动进入睡眠，860uA
- 关机后: 完全断电，0uA

---

## 下一步工作

1. **测试当前框架**: 编译、烧录、验证开关机和按键功能
2. **实现油门控制**: app_throttle + 霍尔传感器集成
3. **实现RF通信**: 心跳、链路检测、电池查询
4. **功耗测试**: 测量实际功耗是否达到860uA
5. **LCD显示**: 等确定型号后实现

---

## 引脚定义 (pinmap_v1.h)

```c
// 按键
#define KEY1            Port_Pin(1, 6)  // P16 - 功能按键1
#define KEY2            Port_Pin(0, 2)  // P02 - 功能按键2
#define KEY_SYS_PWR     Port_Pin(3, 7)  // P37 - 电源按键
#define SYS_PWR_EN      Port_Pin(3, 3)  // P33 - 电源使能

// 霍尔传感器
#define HALL_PWR_EN     Port_Pin(1, 1)  // P11 - 霍尔电源使能
#define HALL_ADC_OUT    Port_Pin(3, 1)  // P31 - 霍尔ADC输出

// LCD
#define LCD_SPI_SCK     Port_Pin(0, 4)  // P04
#define LCD_SPI_MOSI    Port_Pin(0, 5)  // P05
#define LCD_SPI_CS      Port_Pin(0, 6)  // P06
#define LCD_DCX         Port_Pin(0, 7)  // P07
#define LCD_RST         Port_Pin(0, 3)  // P03
```
