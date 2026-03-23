# RF Service Architecture Documentation

RF服务层架构 - 配对管理、地址池、配置存储的协调机制

---

## 模块概览

```
middleware/rf_service/
├── rf_config.c/h       # Flash配置管理（地址存储/读取）
├── rf_pair.c/h         # 配对协议实现（主从配对状态机）
├── addr_pool.c/h       # 地址池管理（ID分配/冲突避免）
└── rf_pair_protocol.md # 配对协议文档
```

---

## 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    应用层                                │
│  • 启动配对流程                                          │
│  • 读取/保存配对信息                                     │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│              RF Service 中间件层                         │
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  rf_pair     │  │  rf_config   │  │  addr_pool   │  │
│  │  配对协议    │←→│  配置管理    │←→│  地址池      │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│       ↓                   ↓                   ↓          │
└─────────────────────────────────────────────────────────┘
         ↓                   ↓                   ↓
┌─────────────────────────────────────────────────────────┐
│                   底层驱动                               │
│  • RF Driver (hal_drv_rf)                               │
│  • Flash Driver                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 模块协调流程

### 1. 系统启动流程

```
[上电] → rf_config_load_from_flash()
           ↓
       读取Flash配置
           ↓
       ┌─有效配置?─┐
       │           │
      是          否
       ↓           ↓
   加载设备地址   使用默认配对地址
       ↓           ↓
   [正常通信]   [等待配对]
```

### 2. 主机配对流程（Host）

```
[启动配对] → Host_Pairing_Task()
               ↓
          HOST_PAIR_WAIT_REQ
          (监听配对请求)
               ↓
          收到 CMD_PAIR_REQ
               ↓
          解析设备类型和地址分配模式
               ↓
     ┌─地址分配模式?─┐
     │              │
  HOST_ASSIGN    SLAVE_PROVIDE
  遥控器分配      从机自带地址
     ↓              ↓
  addr_pool       使用从机
  分配新地址       提供的地址
     ↓              ↓
  HOST_PAIR_SEND_RESP
  (发送 CMD_PAIR_RESP，携带了新的地址)
     ↓
  HOST_PAIR_VERIFY_PHASE
  (切换到新地址发送 PING，等待 PONG)
     ↓
  验证成功 → rf_config_update_device_addr()
     ↓
  rf_config_save_to_flash()
     ↓
  [配对完成]
```

### 3. 从机配对流程（Slave）

```
[启动配对] → Slave_Pairing_SetConfig()
               ↓
          SLAVE_PAIR_SEND_REQ
          (发送 CMD_PAIR_REQ)
               ↓
          SLAVE_PAIR_WAIT_RESP
          (等待 CMD_PAIR_RESP)
               ↓
          收到新地址和信道
               ↓
          切换到新地址
               ↓
          SLAVE_PAIR_VERIFY_PHASE
          (被动回复 PONG)
               ↓
          验证成功 → rf_config_update_device_addr()
               ↓
          rf_config_save_to_flash()
               ↓
          [配对完成]
```

---

## 核心模块详解

### 1. rf_config - 配置管理

**职责：**
- Flash配置读写
- 设备地址管理
- 随机地址生成

**数据结构：**
```c
typedef struct {
    uint32_t magic;                    // 0x5AA5BEEF
    uint8_t  esc_addr[5];              // 电控地址
    uint8_t  battery_addr[5];          // 动力电池地址
    uint8_t  cadence_addr[5];          // 踏频地址
    uint8_t  valid_mask;               // bit0=电控, bit1=电池, bit2=踏频
    uint8_t  checksum;
} rf_config_t;
```

**关键API：**
```c
// 启动时把rf_config_t对象g_rf_config从flash加载到内存
void rf_config_load_from_flash(void);

// 更新内存中的变量g_rf_config
void rf_config_update_device_addr(device_type_t dev, const uint8_t *addr);

// 把g_rf_config从内存保存到Flash
void rf_config_save_to_flash(void);

// 读取设备地址(g_rf_config.dev)
uint8_t rf_config_read_device_addr(device_type_t dev, uint8_t *addr);

// 生成随机地址
void rf_config_generate_addr(uint8_t *out_addr);
```

**Flash布局：**
```
0x7E000: rf_config_t (配对信息)
```

---

### 2. rf_pair - 配对协议

**职责：**
- 主从配对状态机
- 配对协议实现
- 超时和重试管理

**配对命令：**
```c
typedef enum {
    CMD_PAIR_REQ = 0x10,         // Slave → Host
    CMD_PAIR_RESP = 0x20,        // Host → Slave
    CMD_PAIR_VERIFY_PING = 0x40, // Host → Slave
    CMD_PAIR_VERIFY_PONG = 0x50  // Slave → Host
} rf_pair_cmd_t;
```

**地址分配策略：**
```c
#define ADDR_MODE_HOST_ASSIGN   0   // Host生成随机地址（默认）
#define ADDR_MODE_SLAVE_PROVIDE 1   // Slave自带地址
```

**关键API：**
```c
// 从机配对（非阻塞）
void Slave_Pairing_Task(uint8_t *flag);

// 主机配对（非阻塞）
void Host_Pairing_Task(uint8_t *flag);

// 设置从机配对参数
void Slave_Pairing_SetConfig(uint8_t dev_type, uint8_t addr_mode,
                              const uint8_t *addr);
```

**状态机：**
```c
// 从机状态
typedef enum {
    SLAVE_PAIR_IDLE,
    SLAVE_PAIR_SEND_REQ,
    SLAVE_PAIR_WAIT_RESP,
    SLAVE_PAIR_VERIFY_PHASE,
    SLAVE_PAIR_DONE
} slave_pair_state_t;

// 主机状态
typedef enum {
    HOST_PAIR_IDLE,
    HOST_PAIR_WAIT_REQ,
    HOST_PAIR_SEND_RESP,
    HOST_PAIR_WAIT_TX_DONE,
    HOST_PAIR_VERIFY_PHASE,
    HOST_PAIR_DONE
} host_pair_state_t;
```

---

### 3. addr_pool - 地址池管理

**职责：**
- 管理256个ID的分配
- 避免地址冲突
- 支持保留ID

**数据结构：**
```c
typedef struct {
    uint8_t bitmap[32];      // 256位占用位图
    uint8_t reserved_id[32]; // 256位保留位图
} SingleByteAddrPool_t;
```

**关键API：**
```c
// 初始化（带保留ID）
ADDR_POOL_INIT(&pool, 0x00, 0xFF);

// 初始化（无保留ID）
ADDR_POOL_INIT_NORESERVED(&pool);

// 顺序分配
int8_t addrpool_alloc_addr_first(SingleByteAddrPool_t* pool, uint8_t *out_id);

// 随机分配
int8_t addrpool_alloc_addr_random(SingleByteAddrPool_t* pool, uint8_t *out_id);

// 占用/释放
void addrpool_occupy(SingleByteAddrPool_t* pool, uint8_t id);
void addrpool_free(SingleByteAddrPool_t* pool, uint8_t id);
```

**地址组成：**
```
完整地址 = [BASE_BYTE0, BASE_BYTE1, BASE_BYTE2, BASE_BYTE3, ID]
         = [0x11,       0x22,       0x33,       0x44,       0x00~0xFF]
```

---

## 模块间协调机制

### 协调关系图

```
┌──────────────┐
│   rf_pair    │
│  (配对协议)  │
└──────┬───────┘
       │ 1. 请求分配地址
       ↓
┌──────────────┐
│  addr_pool   │
│  (地址池)    │
└──────┬───────┘
       │ 2. 返回可用ID
       ↓
┌──────────────┐
│  rf_config   │
│  (配置管理)  │
└──────┬───────┘
       │ 3. 保存到Flash
       ↓
    [完成]
```

### 典型调用序列

**主机配对：**
```c
// 1. 收到配对请求
Host_Pairing_Task(&flag);
  ↓
// 2. 分配地址（如果是HOST_ASSIGN模式）
addrpool_alloc_addr_random(&pool, &id);
  ↓
// 3. 组装完整地址
new_addr = [0x11, 0x22, 0x33, 0x44, id];
  ↓
// 4. 发送响应
send_pair_resp(new_addr);
  ↓
// 5. 验证成功后保存
rf_config_update_device_addr(dev_type, new_addr);
rf_config_save_to_flash();
```

**从机配对：**
```c
// 1. 设置配对参数
Slave_Pairing_SetConfig(DEV_TYPE_ESC, ADDR_MODE_HOST_ASSIGN, NULL);
  ↓
// 2. 启动配对
Slave_Pairing_Task(&flag);
  ↓
// 3. 收到新地址后保存
rf_config_update_device_addr(dev_type, new_addr);
rf_config_save_to_flash();
```

---

## 使用示例

### 示例1：主机启动配对

```c
void start_host_pairing(void) {
    // 1. 加载配置
    rf_config_load_from_flash();

    // 2. 切换到配对频道
    HAL_RF_SetChannel(&hrf, PAIR_CH_DEFAULT);

    // 3. 启动配对任务
    host_pair_success_flag = 0;
    while (!host_pair_success_flag) {
        Host_Pairing_Task(&host_pair_success_flag);
    }

    uart_printf("配对成功\n");
}
```

### 示例2：从机启动配对

```c
void start_slave_pairing(void) {
    // 1. 设置配对参数
    Slave_Pairing_SetConfig(DEV_TYPE_ESC, ADDR_MODE_HOST_ASSIGN, NULL);

    // 2. 切换到配对频道
    HAL_RF_SetChannel(&hrf, PAIR_CH_DEFAULT);

    // 3. 启动配对任务
    slave_pair_success_flag = 0;
    while (!slave_pair_success_flag) {
        Slave_Pairing_Task(&slave_pair_success_flag);
    }

    uart_printf("配对成功\n");
}
```

### 示例3：读取已配对设备

```c
void load_paired_devices(void) {
    rf_config_load_from_flash();

    uint8_t esc_addr[5];
    if (rf_config_read_device_addr(DEV_TYPE_ESC, esc_addr)) {
        uart_printf("电控地址: ");
        for (int i = 0; i < 5; i++) {
            uart_printf("%02X ", esc_addr[i]);
        }
        uart_printf("\n");
    } else {
        uart_printf("电控未配对\n");
    }
}
```

---

## 配对协议时序

### 成功配对时序

```
Slave                          Host
  |                              |
  |--- CMD_PAIR_REQ ------------>|
  |    (设备类型 + 地址模式)      |
  |                              |
  |<-- CMD_PAIR_RESP ------------|
  |    (新地址 + 新信道)          |
  |                              |
  [切换到新地址]              [切换到新地址]
  |                              |
  |<-- CMD_PAIR_VERIFY_PING -----|
  |                              |
  |--- CMD_PAIR_VERIFY_PONG ---->|
  |                              |
  [保存配置]                  [保存配置]
  |                              |
 完成                           完成
```

### 超时重试机制

```
Slave                          Host
  |                              |
  |--- CMD_PAIR_REQ ------------>|
  |                              |
  [等待超时]                      |
  |                              |
  |--- CMD_PAIR_REQ ------------>|  (重发)
  |                              |
  |<-- CMD_PAIR_RESP ------------|
  |                              |
```

---

## 注意事项

1. **配对频道固定**
   - 默认使用频道78（PAIR_CH_DEFAULT）
   - 配对完成后切换到工作频道

2. **地址冲突避免**
   - 使用地址池管理，避免重复分配
   - 支持保留特殊ID

3. **Flash写入保护**
   - 写入前强制解锁
   - 使用校验和验证数据完整性

4. **非阻塞设计**
   - 配对任务为非阻塞状态机
   - 需周期调用 Pairing_Task()

5. **验证阶段**
   - 配对后需验证新地址通信
   - 验证失败会回退到配对地址

---

## 版本历史

- **v1.0** (2026-03-21)
  - 配对协议实现
  - Flash配置管理
  - 地址池管理
  - 主从配对状态机
