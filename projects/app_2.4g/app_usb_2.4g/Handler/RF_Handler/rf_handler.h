#ifndef __RF_HANDLER_H
#define __RF_HANDLER_H

#include "Types.h"
#include "hal_drv_rf.h"
#include "hal_drv_rf.h"
#include "my_queue.h"




#define max_rf_payload_len   32 //RF单次发送接收最大净荷长度 

#define rf_send_queue_len    8
#define rf_recv_queue_len    8

//发送队列项结构体定义
#pragma pack(1)
typedef struct {
    uint8_t dest_addr[5];                 // 这包数据的目标地址
    uint8_t len;                          // 数据长度
    uint8_t payload[max_rf_payload_len];  // 数据载荷
} Rf_txQueueItem_t;
#pragma pack()

//接收队列项结构体定义
#pragma pack(1)
typedef struct {
    uint8_t pipes;                        // 这包数据的pipe
    uint8_t len;                          // 数据长度
    uint8_t payload[max_rf_payload_len];  // 数据载荷
} Rf_rxQueueItem_t;
#pragma pack()


extern my_queue_t rf_rxQueue; 
extern my_queue_t rf_txQueue;
extern RF_HandleTypeDef hrf; //全局RF句柄

extern volatile uint32_t rf_int_count_rxdr;
extern volatile uint32_t rf_int_count_txds;
extern volatile uint32_t rf_int_count_maxrt;

void RF_Handler_Init(void);
void RF_txQueue_Send(uint8_t *dest_addr,uint8_t *data_pack, uint8_t len);
void RF_txQueue_Clear(void);
uint8_t RF_rxQueue_Recv(const uint8_t **data_ptr, uint8_t *out_len, uint8_t *pipes);
uint8_t RF_rxQueue_clear(void);
void RF_Service_Handler(RF_HandleTypeDef *hrf);

#endif /* __RF_HANDLER_H */
