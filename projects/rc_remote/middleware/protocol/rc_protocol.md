# RC Protocol 通信协议文档

## 1. 概述

遥控器(REMOTE)与从机(电控ESC、电池BATTERY、踏频CADENCE)之间的 2.4G RF 通信协议。

最大包长 32 字节。遥控器为主机(PTX)，外设为从机(PRX)，从机通过 ACK Payload 回传数据。

### 协议分层

```
┌─────────────────────────────────────────┐
│  应用层: 各设备自定义 PAYLOAD 数据包      │  ← 电控/电池/踏频各自定义
├─────────────────────────────────────────┤
│  传输层: 通用帧格式 + 确认机制            │  ← 协议核心
├─────────────────────────────────────────┤
│  物理层: 2.4G RF (Enhanced ShockBurst)   │  ← 芯片BK3633
└─────────────────────────────────────────┘
```

- **传输层** 定义通用帧格式(帧头、INFO、CMD、SEQ、CRC)
- **应用层** 定义各设备 PAYLOAD 的具体内容，由 CMD 区分，独立演进

---

## 2. 传输层: 通用帧格式

```
[HEAD: 2B] [INFO: 1B] [CMD: 1B] [SEQ: 1B] [PAYLOAD: nB] [CRC16: 2B]
```

| 字段   | 偏移 | 大小 | 说明                              |
|--------|------|------|-----------------------------------|
| HEAD   | 0    | 2B   | 固定帧头 `0xCA 0xBD`              |
| INFO   | 2    | 1B   | 长度+保留 (位域见下)               |
| CMD    | 3    | 1B   | 命令码 (传输方向和设备类型)        |
| SEQ    | 4    | 1B   | 包序列号 (下行=seq, 上行=ack_seq) |
| PAYLOAD| 5    | nB   | 应用层数据 (最大25B)               |
| CRC16  | 5+n  | 2B   | CRC16-CCITT, 小端存储              |

**协议开销: 7 字节**, 最大帧长: 7 + 25 = 32B

### 2.1 INFO 位域

```
bit[7:3] = LEN (5bit)  payload字节数, 0~31
bit[2:0] = 保留 (3bit) 置0, 留作扩展
```

### 2.2 CRC

- 算法: CRC16-CCITT, poly=0x1021, init=0xFFFF
- 范围: INFO + CMD + SEQ + PAYLOAD
- 存储: 小端 (低字节在前)

### 2.3 确认机制 (ACK Payload 异步回传)

遥控器(PTX)主动发送，从机(PRX)通过 RF 硬件 ACK Payload 回传数据。

由于 ACK Payload 至少延迟一帧，采用 seq/ack_seq 匹配确认:

```
遥控(PTX)                         从机(PRX)
   |                                  |
   |--- data(seq=5) ----------------->|
   |<-- ack_payload(ack_seq=4) -------|  (上一帧的ACK)
   |                                  |
   |--- data(seq=6) ----------------->|
   |<-- ack_payload(ack_seq=5) -------|  (确认了seq=5)
   |                                  |
```

1. 遥控每帧携带递增的 `seq` (uint8_t, 0~255 自然回绕)
2. 从机收到后将 `seq` 存入 `ack_seq`, 随下一次 ACK Payload 回传
3. 遥控收到 `ack_seq` 后与记录的 `change_seq` 比较:
   - `(int8_t)(ack_seq - change_seq) >= 0` → 数据已确认, 停止重发
   - 否则 → 数据未确认, 继续发送
4. 用 `(int8_t)` 差值比较处理 uint8_t 溢出回绕

---

## 3. 命令码分配

CMD 隐含通信方向和设备类型

| CMD    | 方向              | 名称           | 说明               |
|--------|-------------------|----------------|--------------------|
| `0x31` | REMOTE → ESC      | CMD_ESC_CTRL    | 电控控制数据下发   |
| `0x32` | REMOTE → BATTERY  | CMD_BAT_QUERY  | 电池查询 (预留)    |
| `0x61` | ESC → REMOTE      | CMD_MC_STATUS  | 电控状态回传       |
| `0x62` | BATTERY → REMOTE  | CMD_BAT_STATUS | 电池状态回传 (预留) |

**命令码分区规划:**

| 范围           | 用途                       |
|----------------|----------------------------|
| `0x31` ~ `0x3F`| 下行 (REMOTE → 外设)       |
| `0x31`         | → ESC 控制                  |
| `0x32`         | → BATTERY 查询              |
| `0x33` ~ `0x3F`| → 预留 (踏频等)             |
| `0x61` ~ `0x6F`| 上行 (外设 → REMOTE)        |
| `0x61`         | ← ESC 状态                  |
| `0x62`         | ← BATTERY 状态              |
| `0x63` ~ `0x6F`| ← 预留 (踏频等)             |

---

## 4. 应用层: 数据包定义

> 以下为各设备 PAYLOAD 的具体内容，由 CMD 区分。
> 各数据包独立定义，可各自演进，不影响传输层。

### 4.1 电控通信 (REMOTE ↔ ESC)

#### 4.1.1 控制帧 PAYLOAD (CMD=0x31, REMOTE → ESC, 3B)

| 偏移 | 大小 | 字段     | 说明                                          |
|------|------|----------|-----------------------------------------------|
| 0    | 1B   | gear     | 输出功率档位 1~4 (1=25%, 2=50%, 3=75%, 4=100%) |
| 1    | 1B   | mode     | 0x00=助力模式, 0x01=训练模式                   |
| 2    | 1B   | throttle | 油门值百分比 0~100                             |

完整帧示例 (10B):
```
CA BD [INFO] 31 [SEQ] [gear] [mode] [throttle] [CRC_L] [CRC_H]
 0  1    2    3    4     5      6       7          8       9
```

#### 4.1.2 状态回复帧 PAYLOAD (CMD=0x61, ESC → REMOTE, 6B)

| 偏移 | 大小 | 字段    | 说明           |
|------|------|---------|----------------|
| 0    | 2B   | speed   | 速度 (小端)    |
| 2    | 4B   | mileage | 里程 (小端)    |

完整帧示例 (13B):
```
CA BD [INFO] 61 [ACK_SEQ] [speed_L speed_H] [mile_0 mile_1 mile_2 mile_3] [CRC_L] [CRC_H]
 0  1    2    3     4         5       6         7      8      9     10        11     12
```

### 4.2 电池通信 (REMOTE ↔ BATTERY)

#### 4.2.1 查询帧 PAYLOAD (CMD=0x32, REMOTE → BATTERY, 0B)

无 PAYLOAD，LEN=0。仅帧头+INFO+CMD+SEQ+CRC 即可触发电池回传状态。

完整帧示例 (7B):
```
CA BD [INFO] 32 [SEQ] [CRC_L] [CRC_H]
 0  1    2    3    4      5       6
```

#### 4.2.2 状态回复帧 PAYLOAD (CMD=0x62, BATTERY → REMOTE, 3B)

| 偏移 | 大小 | 字段        | 说明                          |
|------|------|-------------|-------------------------------|
| 0    | 1B   | temperature | 温度 (偏移+40°C, 即0=−40°C)   |
| 1    | 1B   | soc         | 电量百分比 0~100              |
| 2    | 1B   | status      | 状态标志位                    |

完整帧示例 (10B):
```
CA BD [INFO] 62 [ACK_SEQ] [temperature] [soc] [status] [CRC_L] [CRC_H]
 0  1    2    3     4          5          6       7        8       9
```

### 4.3 踏频通信 (REMOTE ↔ CADENCE) — 待定义

> TODO: 待产品需求确定后定义

---

## 5. 帧长汇总

| 帧类型     | CMD    | 方向             | PAYLOAD | 帧总长 |
|------------|--------|------------------|---------|--------|
| 电控控制帧 | `0x31` | REMOTE → ESC     | 3B      | 10B    |
| 电控状态帧 | `0x61` | ESC → REMOTE     | 6B      | 13B    |
| 电池查询帧 | `0x32` | REMOTE → BATTERY | 0B      | 7B     |
| 电池状态帧 | `0x62` | BATTERY → REMOTE | 3B      | 10B    |
