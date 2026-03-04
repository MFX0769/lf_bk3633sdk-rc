#include "rf_protocol.h"
#include <string.h>





// CRC16 Modbus计算（假设已有实现，如果没有需要添加）
extern uint16_t crc16_modbus(uint8_t *data, uint16_t len);

// 构造电控命令包
void RF_BuildESCCmd(uint8_t *buf, uint16_t throttle, uint8_t seq)
{
    ESC_Pkt_t *pkt = (ESC_Pkt_t*)buf;
    pkt->hdr.header = 0xE1;
    pkt->hdr.seq = seq;
    pkt->data = throttle;
    pkt->crc16 = crc16_modbus((uint8_t*)pkt, sizeof(ESC_Pkt_t) - 2);
}

// 解析电控响应包
int RF_ParseESCResp(uint8_t *data, uint8_t len, ESC_Pkt_t *out)
{
    if(len < sizeof(ESC_Pkt_t)) return -1;

    ESC_Pkt_t *pkt = (ESC_Pkt_t*)data;
    if(pkt->hdr.header != 0xE2) return -1;

    // 校验CRC
    uint16_t crc = crc16_modbus(data, len - 2);
    if(crc != pkt->crc16) return -1;

    memcpy(out, pkt, sizeof(ESC_Pkt_t));
    return 0;
}

// 构造电池查询包
void RF_BuildBATQuery(uint8_t *buf, uint8_t seq)
{
    BAT_Pkt_t *pkt = (BAT_Pkt_t*)buf;
    pkt->hdr.header = 0xB1;
    pkt->hdr.seq = seq;
    pkt->data = 0x01;  // 查询命令
    pkt->crc16 = crc16_modbus((uint8_t*)pkt, sizeof(BAT_Pkt_t) - 2);
}

// 解析电池响应包
int RF_ParseBATResp(uint8_t *data, uint8_t len, BAT_Pkt_t *out)
{
    if(len < sizeof(BAT_Pkt_t)) return -1;

    BAT_Pkt_t *pkt = (BAT_Pkt_t*)data;
    if(pkt->hdr.header != 0xB2) return -1;

    // 校验CRC
    uint16_t crc = crc16_modbus(data, len - 2);
    if(crc != pkt->crc16) return -1;

    memcpy(out, pkt, sizeof(BAT_Pkt_t));
    return 0;
}
