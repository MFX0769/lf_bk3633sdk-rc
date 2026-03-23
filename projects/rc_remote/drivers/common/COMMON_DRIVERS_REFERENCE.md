# Common Drivers API Documentation

通用驱动工具集 - 队列、调试工具

---

## 模块概览

```
drivers/common/
├── my_queue.c/h    # 环形队列（FIFO）
└── debug.c/h       # 调试工具（寄存器打印、VOFA+协议）
```

---

## 1. 环形队列 (my_queue)

### 概述

通用环形队列（FIFO），支持固定大小元素的入队/出队操作。

### 数据结构

```c
typedef struct {
    uint8_t *p_buffer;      // 内存池首地址
    uint16_t item_size;     // 每项数据大小（字节）
    uint16_t item_count;    // 队列项数
    volatile uint16_t in;   // 写索引
    volatile uint16_t out;  // 读索引
    uint32_t overwrite_cnt; // 覆盖写入计数
} my_queue_t;
```

### API

#### queue_init()
```c
void queue_init(my_queue_t *queue, void *buffer,
                uint16_t buffer_size, uint16_t item_count, uint16_t item_size);
```
初始化队列。

**参数：**
- `queue`: 队列对象
- `buffer`: 内存池（静态数组）
- `buffer_size`: 内存池大小
- `item_count`: 队列容量
- `item_size`: 单个元素大小

**示例：**
```c
my_queue_t rx_queue;
uint8_t rx_buffer[32 * 10];  // 10个32字节元素

queue_init(&rx_queue, rx_buffer, sizeof(rx_buffer), 10, 32);
```

#### queue_push()
```c
uint8_t queue_push(my_queue_t *queue, const void *p_src);
```
入队（队列满时失败）。

**返回值：** 1=成功，0=队列满

#### queue_push_overwrite()
```c
uint8_t queue_push_overwrite(my_queue_t *queue, const void *p_src);
```
入队（队列满时覆盖最旧数据）。

#### queue_pop()
```c
uint8_t queue_pop(my_queue_t *queue, void *p_dest);
```
出队。

**返回值：** 1=成功，0=队列空

#### queue_peek()
```c
uint8_t queue_peek(my_queue_t *queue, void *p_dest);
```
查看队首元素（不出队）。

#### queue_peek_at()
```c
uint8_t queue_peek_at(my_queue_t *queue, uint16_t offset, void *p_dest);
```
查看指定偏移的元素。

### 宏定义

```c
queue_clear(q)          // 清空队列
queue_get_counts(q)     // 获取元素数量
queue_is_empty(q)       // 判断是否为空
queue_is_full(q)        // 判断是否已满
```

### 使用示例

```c
// 1. 定义队列和缓冲区
my_queue_t rx_queue;
uint8_t rx_buffer[32 * 10];

// 2. 初始化
queue_init(&rx_queue, rx_buffer, sizeof(rx_buffer), 10, 32);

// 3. 入队
uint8_t data[32] = {0x01, 0x02, 0x03};
if (queue_push(&rx_queue, data)) {
    uart_printf("入队成功\n");
}

// 4. 出队
uint8_t recv[32];
if (queue_pop(&rx_queue, recv)) {
    uart_printf("出队成功\n");
}

// 5. 查询状态
uint16_t count = queue_get_counts(&rx_queue);
uart_printf("队列中有 %d 个元素\n", count);
```

---

## 2. 调试工具 (debug)

### 概述

提供RF寄存器打印、十六进制数据打印、VOFA+协议支持。

### API

#### debug_print_rf_registers()
```c
void debug_print_rf_registers(void);
```
打印RF芯片所有寄存器状态（调试用）。

#### debug_print_txrx_addr()
```c
void debug_print_txrx_addr(void);
```
打印Tx/Rx地址配置。

#### debug_print_hex()
```c
void debug_print_hex(void *msg, void *dat, int len);
```
以十六进制格式打印数据。

**参数：**
- `msg`: 消息前缀
- `dat`: 数据指针
- `len`: 数据长度

**示例：**
```c
uint8_t data[10] = {0x01, 0x02, 0x03};
debug_print_hex("RX Data", data, 10);
// 输出: RX Data: 01 02 03 04 05 06 07 08 09 0A
```

#### vofa_senddata()
```c
void vofa_senddata(float *data, uint8_t ch_count);
```
发送数据到VOFA+上位机（JustFloat协议）。

**参数：**
- `data`: float数组指针
- `ch_count`: 通道数量

**示例：**
```c
float wave[4] = {1.23, 4.56, 7.89, 0.12};
vofa_senddata(wave, 4);  // 发送4通道数据到VOFA+
```

---

## 使用场景

### 场景1：RF接收队列

```c
my_queue_t rf_rx_queue;
uint8_t rf_rx_buffer[32 * 20];  // 20个包

void rf_init(void) {
    queue_init(&rf_rx_queue, rf_rx_buffer, sizeof(rf_rx_buffer), 20, 32);
}

void rf_rx_callback(uint8_t *data, uint8_t len) {
    if (!queue_push(&rf_rx_queue, data)) {
        uart_printf("队列满，丢包\n");
    }
}

void app_loop(void) {
    uint8_t packet[32];
    if (queue_pop(&rf_rx_queue, packet)) {
        process_packet(packet);
    }
}
```

### 场景2：调试RF通信

```c
void debug_rf_status(void) {
    debug_print_rf_registers();
    debug_print_txrx_addr();
}

void debug_rx_packet(uint8_t *data, uint8_t len) {
    debug_print_hex("RX", data, len);
}
```

### 场景3：VOFA+波形监控

```c
void monitor_sensor_data(void) {
    float data[3];
    data[0] = hall_sensor_read_throttle(&hall);
    data[1] = battery.data.voltage_mv / 1000.0f;
    data[2] = battery.data.soc;

    vofa_senddata(data, 3);  // 实时波形显示
}
```

---

## 注意事项

1. **队列缓冲区必须是静态或全局变量**
   ```c
   static uint8_t buffer[320];  // ✓ 正确
   uint8_t buffer[320];         // ✗ 错误（栈变量）
   ```

2. **队列非线程安全**
   - 单生产者单消费者：安全
   - 多生产者或多消费者：需加锁

3. **VOFA+协议格式**
   - JustFloat：4字节float + 尾帧（0x00 0x00 0x80 0x7F）
   - 波特率建议：115200或更高

---

## 版本历史

- **v1.0** (2026-03-21)
  - 环形队列实现
  - RF调试工具
  - VOFA+协议支持
