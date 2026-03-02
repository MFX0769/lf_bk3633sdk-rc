#ifndef __RF_PROTOCOL_H__
#define __RF_PROTOCOL_H__

#include <stdint.h>

// 通用协议头
typedef struct {
    uint8_t header;  // 包类型：0xE1=电控命令, 0xE2=电控响应, 0xB1=电池查询, 0xB2=电池响应
    uint8_t seq;     // 序列号
} __attribute__((packed)) RF_Header_t;

// 电控数据包
typedef struct {
    RF_Header_t hdr;
    uint16_t data;    // 油门值/速度值
    uint16_t crc16;
} __attribute__((packed)) ESC_Pkt_t;

// 电池数据包
typedef struct {
    RF_Header_t hdr;
    uint8_t data;     // 命令/SOC
    uint16_t crc16;
} __attribute__((packed)) BAT_Pkt_t;

// 协议接口
void RF_BuildESCCmd(uint8_t *buf, uint16_t throttle, uint8_t seq);
int RF_ParseESCResp(uint8_t *data, uint8_t len, ESC_Pkt_t *out);
void RF_BuildBATQuery(uint8_t *buf, uint8_t seq);
int RF_ParseBATResp(uint8_t *data, uint8_t len, BAT_Pkt_t *out);

#endif // __RF_PROTOCOL_H__
