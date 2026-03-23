# RF Pair Protocol v2 配对协议文档

## 1. 概述

遥控器(Host/REMOTE)与从机(Slave: ESC/BATTERY/CADENCE)之间的配对协议。

### 配对信道

所有设备在公共配对信道(默认 CH=78)和公共配对地址(默认 `AA BB CC DD EE`)上完成配对握手，成功后切换到专属地址通信。

---

## 2. 地址分配策略

| addr_mode | 名称          | 地址由谁生成 | 适用场景                         |
|-----------|---------------|-------------|----------------------------------|
| 0         | HOST_ASSIGN   | Host(遥控器) | 默认策略，从机无随机数能力        |
| 1         | SLAVE_PROVIDE | Slave(从机)  | 从机自带地址，如电池有独立MCU     |

- **HOST_ASSIGN (0)**: Slave 发 REQ(不含地址) → Host 生成随机地址放在 RESP 中下发
- **SLAVE_PROVIDE (1)**: Slave 发 REQ(附带5B地址) → Host 直接采用该地址放在 RESP 中确认

---

## 3. 数据包定义

### 3.1 配对请求 (PAIR_REQ, Slave → Host)

CMD = `0x10`

| 偏移 | 大小 | 字段      | 说明                                          |
|------|------|-----------|-----------------------------------------------|
| 0    | 1B   | cmd       | `0x10`                                        |
| 1    | 1B   | dev_type  | 设备类型枚举 (0=REMOTE, 1=ESC, 2=BATTERY, 3=CADENCE) |
| 2    | 1B   | addr_mode | 地址分配策略 (0=HOST_ASSIGN, 1=SLAVE_PROVIDE) |
| 3    | 5B   | addr      | 从机提供的地址 (addr_mode=1时有效，addr_mode=0时填0) |

总长: **9B** (固定)

### 3.2 配对响应 (PAIR_RESP, Host → Slave)

CMD = `0x20`

| 偏移 | 大小 | 字段     | 说明                           |
|------|------|----------|--------------------------------|
| 0    | 1B   | cmd      | `0x20`                         |
| 1    | 5B   | new_addr | 通信地址 (Host分配或确认Slave提供的) |
| 2    | 1B   | new_chn  | 通信信道 (预留, 当前为0)        |

总长: **7B**

### 3.3 验证 PING (Host → Slave)

CMD = `0x40`

| 偏移 | 大小 | 字段         | 说明        |
|------|------|--------------|-------------|
| 0    | 1B   | cmd          | `0x40`      |
| 1    | 1B   | magic_number | `0xA5`      |

总长: **2B**

### 3.4 验证 PONG (Slave → Host)

CMD = `0x50`

| 偏移 | 大小 | 字段         | 说明        |
|------|------|--------------|-------------|
| 0    | 1B   | cmd          | `0x50`      |
| 1    | 1B   | magic_number | `0x5A`      |

总长: **2B**

---

## 4. 配对流程

### 4.1 状态机时序

```
Slave(PTX)                  公共配对地址                  Host(PRX)
   |                                                        |
   |  ============ 阶段1: 请求/响应 ============             |
   |                                                        |
   |--- PAIR_REQ(dev_type, addr_mode, [addr]) ------------->|
   |                                           超时100ms重发 |
   |<-- PAIR_RESP(new_addr, new_chn) x5 (burst) -----------|
   |                                                        |
   |  ============ 阶段2: 切换到新地址 ============          |
   |                                                        |
   |  [双方切换到 new_addr]                                  |
   |                                                        |
   |  ============ 阶段3: Ping-Pong 验证 ============       |
   |                                                        |
   |<-- VERIFY_PING (0xA5) ---------------------------------|  50ms间隔
   |--- VERIFY_PONG (0x5A) -------------------------------->|
   |<-- VERIFY_PING (0xA5) ---------------------------------|
   |--- VERIFY_PONG (0x5A) -------------------------------->|
   |         ...重复N次...                                   |
   |                                                        |
   |  ============ 阶段4: 完成 ============                  |
   |                                                        |
   |  Host达到验证次数 → 停止PING → 保存地址                  |
   |  Slave检测Host静默3s → 保存地址 → 完成                   |
```

### 4.2 Slave 状态机

```
IDLE → SEND_REQ → WAIT_RESP → VERIFY_PHASE → DONE
              ↑       |              |
              └───────┘              |  (验证超时)
              (响应超时)              └──→ SEND_REQ
```

| 状态           | 行为                                              | 超时        |
|----------------|---------------------------------------------------|-------------|
| SEND_REQ       | 发送 PAIR_REQ                                     | —           |
| WAIT_RESP      | 等待 PAIR_RESP，收到后切换新地址进入验证            | 100ms→重发  |
| VERIFY_PHASE   | 收到 PING 回复 PONG，计数达标后等待 Host 静默      | 5s→回退重发 |
| DONE           | 保存地址到 Flash，退出配对                         | —           |

### 4.3 Host 状态机

```
IDLE → WAIT_REQ → SEND_RESP → WAIT_TX_DONE → VERIFY_PHASE → DONE
                                                    |
                                                    | (验证超时)
                                                    └──→ WAIT_REQ
```

| 状态           | 行为                                              | 超时          |
|----------------|---------------------------------------------------|---------------|
| WAIT_REQ       | 监听 PAIR_REQ，解析 dev_type + addr_mode           | —             |
| SEND_RESP      | Burst 发送 5 包 PAIR_RESP                          | —             |
| WAIT_TX_DONE   | 等待发送完成，切换新地址                            | 50ms→回退     |
| VERIFY_PHASE   | 50ms 间隔发送 PING，接收 PONG 计数                 | 4s→回退重发   |
| DONE           | 按 dev_type 保存地址到 Flash，退出配对              | —             |

---

## 6. 设备类型编码

复用 `rf_config.h` 中的 `device_type_t`:

| 值 | 名称     | 说明     |
|----|----------|----------|
| 0  | REMOTE   | 遥控器   |
| 1  | ESC      | 电控     |
| 2  | BATTERY  | 动力电池 |
| 3  | CADENCE  | 踏频     |

---

## 7. 超时与重试参数

| 参数                    | 值     | 说明                          |
|-------------------------|--------|-------------------------------|
| REQ 重发间隔            | 100ms  | Slave 未收到 RESP 时重发      |
| RESP Burst 数量         | 5 包   | 提高 Slave 接收成功率         |
| Verify PING 间隔        | 50ms   | Host 发送 PING 的频率         |
| Host Verify 目标次数    | 10     | Host 收到 10 次 PONG 即成功   |
| Slave Verify 目标次数   | 5      | Slave 回复 5 次 PONG 即达标   |
| Slave Verify 超时       | 5s     | 未收到 PING 则回退重发        |
| Host Verify 超时        | 4s     | 未收到 PONG 则回退等待 REQ    |
| Slave 静默检测          | 3s     | 达标后 Host 静默 3s 视为完成  |
