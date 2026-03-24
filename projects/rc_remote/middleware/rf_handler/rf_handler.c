#include "rf_handler.h"
#include "hal_drv_rf.h"
#include "user_config.h"
#include "my_queue.h"
#include "Types.h"
#include "timer_handler.h"


/* rf_txQueue每一项是固定长度为32+1字节，第一字节存放净荷长度，发送时只发送净荷 */

uint8_t rf_send_queue_buffer[sizeof(Rf_txQueueItem_t)*rf_send_queue_len]; //发送队列缓冲区
my_queue_t rf_txQueue;               // 发送队列
uint8_t rf_recv_queue_buffer[sizeof(Rf_rxQueueItem_t)*rf_recv_queue_len]; //接收队列缓冲区
my_queue_t rf_rxQueue;               // 接收队列


RF_HandleTypeDef hrf; //全局RF句柄
//全局中断计数
volatile uint32_t rf_int_count_rxdr = 0;
volatile uint32_t rf_int_count_txds = 0;
volatile uint32_t rf_int_count_maxrt = 0;

//接收中断回调函数,数据push到接收队列
void rxdr_callback(RF_HandleTypeDef *hrf)
{
    uart_printf("rxdr!");
    if(hrf->RxBuff_valid == 1) {
        hrf->RxBuff_valid = 0;

        //封包入队列
        Rf_rxQueueItem_t temp;
        temp.pipes = hrf->RxPipes;
        temp.len = hrf->RxLen;
        memcpy(temp.payload, hrf->RxBuff, hrf->RxLen);

        queue_push_overwrite(&rf_rxQueue, (Rf_rxQueueItem_t*)&temp);
    }
    rf_int_count_rxdr++;
}
//发送完成中断回调函数 - 业务模式(ACK Payload)
void txds_callback_normal(RF_HandleTypeDef *hrf)
{
    rf_int_count_txds++;
    // ACK Payload模式：不切换，保持TX
}
//达到最大重传中断回调函数 - 业务模式
void maxrt_callback_normal(RF_HandleTypeDef *hrf)
{
    rf_int_count_maxrt++;
    // ACK Payload模式：不切换
}

//发送完成中断回调函数 - 配对模式
void txds_callback_pair(RF_HandleTypeDef *hrf)
{
    rf_int_count_txds++;
    // 配对模式：发送完成后切RX
    HAL_RF_SetRxMode(hrf);
}
//达到最大重传中断回调函数 - 配对模式
void maxrt_callback_pair(RF_HandleTypeDef *hrf)
{
    rf_int_count_maxrt++;
    // 配对模式：发送失败后也切RX
    HAL_RF_SetRxMode(hrf);
}

//初始化参数配置结构体 - 业务模式(ACK Payload)
RF_ConfgTypeDef Init_Normal_AckPayload =
{
    .Mode = MODE_TX,
    .DataRate = BPS_1M,
    .TxPower = RF_TX_POWER_P0p5_dBm,
    .Channel = 0x05,
    .Protocol ={
        .AddressWidth = 5,
        .TxAddress = {0x10, 0x11, 0x36, 0x00, 0x00},
        .Support_NoAck = 1,
        .AutoRetransmitDelay = 5,
        .AutoRetransmitCount = 3,

        .RxPipes[0] = {
            .PipeNum = 0,
            .Enable = 1,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x10, 0x11, 0x36, 0x00, 0x00}
        },
        .RxPipes[1] = {
            .PipeNum = 1,
            .Enable = 0,  // 禁用不必要的Pipe
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x10, 0x56, 0x24, 0xCD, 0x78}
        },
        .RxPipes[2]={
            .PipeNum = 2,
            .Enable = 0,  // 禁用
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x11, 0, 0, 0, 0}
        },
        .RxPipes[3]={
            .PipeNum = 3,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x12, 0, 0, 0, 0}
        },
        .RxPipes[4]={
            .PipeNum = 4,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x13, 0, 0, 0, 0}
        },
        .RxPipes[5]={
            .PipeNum = 5,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x14, 0, 0, 0, 0}
        }
    },
    .IRQ = {
        .RxDR = {
            .enable = 1,
            .user_cb = rxdr_callback
        },
        .TxDS = {
            .enable = 1,
            .user_cb = txds_callback_normal 
        },
        .MaxRT = {
            .enable = 1,
            .user_cb = maxrt_callback_normal
        }
    }
};

//初始化参数配置结构体 - 配对模式(队列+切换)
RF_ConfgTypeDef Init_Pair_Queue =
{
    .Mode = MODE_TX,
    .DataRate = BPS_1M,
    .TxPower = RF_TX_POWER_P0p5_dBm,
    .Channel = 0x05, // 频道5
    .Protocol ={
        .AddressWidth = 5,
        .TxAddress = {0x10, 0x11, 0x36, 0x00, 0x00},
        .Support_NoAck = 1,             
        .AutoRetransmitDelay = 2,        // 最大重传延迟 3*250=
        .AutoRetransmitCount = 3,        // 最大重传次数 3
        
        .RxPipes[0] = {
            .PipeNum = 0,
            .Enable = 1,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x10, 0x11, 0x36, 0x00, 0x00}
        },
        .RxPipes[1] = {
            .PipeNum = 1,
            .Enable = 0,  // 禁用不必要的Pipe
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x10, 0x56, 0x24, 0xCD, 0x78}
        },
        .RxPipes[2]={
            .PipeNum = 2,
            .Enable = 0,  // 禁用
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x11, 0, 0, 0, 0}
        },
        .RxPipes[3]={
            .PipeNum = 3,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x12, 0, 0, 0, 0}
        },
        .RxPipes[4]={
            .PipeNum = 4,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x13, 0, 0, 0, 0}
        },
        .RxPipes[5]={
            .PipeNum = 5,
            .Enable = 0,
            .AutoAck = 1,
            .EnDynamicPayload = 1,
            .PayloadWidth = 32,
            .Support_Payload_Attach_ACK = 1,
            .Address = {0x14, 0, 0, 0, 0}
        }
    },
    .IRQ = {
        .RxDR = {
            .enable = 1,
            .user_cb = rxdr_callback
        },
        .TxDS = {
            .enable = 1,
            .user_cb = txds_callback_pair  // 配对模式：发送完成后切RX
        },
        .MaxRT = {
            .enable = 1,
            .user_cb = maxrt_callback_pair
        }
    }
};


/* 业务模式初始化：ACK Payload模式，遥控保持TX */
void RF_Handler_Init_ToNormal(void)
{
    HAL_RF_Init(&hrf, &Init_Normal_AckPayload);
    HAL_RF_TimeManager_register(&hrf, Get_SysTick_ms);

    /* 默认初始化为发送模式 */
    HAL_RF_SetTxMode(&hrf);

    /* 初始化发送和接收队列 */
    queue_init(&rf_txQueue, rf_send_queue_buffer,
                sizeof(rf_send_queue_buffer), rf_send_queue_len, sizeof(Rf_txQueueItem_t));
    queue_init(&rf_rxQueue, rf_recv_queue_buffer,
                sizeof(rf_recv_queue_buffer), rf_recv_queue_len, sizeof(Rf_rxQueueItem_t));
}

/* 配对模式初始化：队列+切换模式 */
void RF_Handler_Init_ToPair(void)
{
    HAL_RF_Init(&hrf, &Init_Pair_Queue);
    HAL_RF_TimeManager_register(&hrf, Get_SysTick_ms);

    /* 配对模式默认RX */
    HAL_RF_SetRxMode(&hrf);

    /* 初始化发送和接收队列 */
    queue_init(&rf_txQueue, rf_send_queue_buffer,
                sizeof(rf_send_queue_buffer), rf_send_queue_len, sizeof(Rf_txQueueItem_t));
    queue_init(&rf_rxQueue, rf_recv_queue_buffer,
                sizeof(rf_recv_queue_buffer), rf_recv_queue_len, sizeof(Rf_rxQueueItem_t));
}

/* 兼容旧接口：默认使用业务模式 */
void RF_Handler_Init(void)
{
    RF_Handler_Init_ToNormal();
}

/* 发送数据入队函数
 * dest_addr: 目标地址指针,传入5字节地址
 * data_pack: 指向要发送的数据包指针
 * len: 数据包长度，最大不超过 max_rf_payload_len 32字节
 */
void RF_txQueue_Send(uint8_t *dest_addr,uint8_t *data_pack, uint8_t len)
{
    if(len > max_rf_payload_len) {
        len = max_rf_payload_len; //限制最大长度
    }

    /* 封包 */
    Rf_txQueueItem_t temp_item;
    temp_item.len = len;

    memcpy(temp_item.dest_addr, dest_addr, sizeof(temp_item.dest_addr));
    memcpy(temp_item.payload, data_pack, temp_item.len);
    
    //入队
    queue_push_overwrite(&rf_txQueue, (Rf_txQueueItem_t*)&temp_item);
}

/* txQueue清空函数
 */
void RF_txQueue_Clear(void)
{
    my_queue_t* queue = &rf_txQueue;
    queue->in = queue->out; //直接将写索引等于读索引，清空队列
}


/**
 * @brief  从接收队列取出一帧数据
 * @param   data_ptr:  输出参数， 指向接收到的数据指针
 * @param   out_len:   输出参数， 指向接收到的数据长度
 * @param   pipes:     输出参数， 指向接收到的数据管道号
 * @return  1:成功, 0:失败(空)
 */
uint8_t RF_rxQueue_Recv(const uint8_t **data_ptr, uint8_t *out_len, uint8_t *pipes)
{   
    static Rf_rxQueueItem_t temp_item;
    uint8_t flag = queue_pop(&rf_rxQueue, &temp_item);
    if(flag == 1) {
        if(out_len) 
            *out_len = temp_item.len;              // 数据长度
        if(pipes)
            *pipes = temp_item.pipes;              // 接收管道
        if(data_ptr) 
            *data_ptr = temp_item.payload;         // 指向有效数据
        return 1; // 读取成功
    } else {
        return 0; // 队列为空，读取失败
    }
}

/**
 * @brief  将接收队列数据清空
 * @param   data_ptr:  输出参数，指向接收到的数据指针
 * @param   out_len:   输出参数， 指向接收到的数据长度
 * @param   pipes:     输出参数， 指向接收到的数据管道号
 * @return  1:成功
 */
uint8_t RF_rxQueue_clear(void)
{
    my_queue_t* queue = &rf_rxQueue;
    queue->in = queue->out; //直接将写索引等于读索引，清空队列
    return 1;
}


// RF 服务处理函数，放在循环/任务中定时运行,自动管理tx/rx模式切换
void RF_Service_Handler(RF_HandleTypeDef *hrf)
{
    // peek - try - pop 
    // 处理发送队列
    Rf_txQueueItem_t tx_item;

    // 检查队列是否为空，不要直接pop
    if(!queue_is_empty(&rf_txQueue)) {
        
        // 使用Peek看队头数据，但不移除,这样即使硬件忙发送失败，数据也不会丢失
        queue_peek(&rf_txQueue, &tx_item);
        if(__HAL_RF_Get_TRxMode_Bit() == 1) {
            HAL_RF_SetTxMode(hrf);
        }

        // 设置目标地址
        HAL_RF_SetTxAddress(hrf, tx_item.dest_addr, 5);
        HAL_RF_SetRxAddress(hrf, 0, tx_item.dest_addr, 5);
        
        // 尝试调用发送函数
        // 如果返回 BUSY，说明它内部运行了超时检测逻辑但还没超时
        // 如果返回 OK，说明硬件空闲并接受了新数据
        if(HAL_RF_Transmit_IT(hrf, tx_item.payload, tx_item.len) == HAL_OK){
            //uart_printf("rf_send_service len:%d\n", tx_item.len);
            //只有把queue数据放入硬件FIFO了，才pop出来
            queue_pop(&rf_txQueue, &tx_item);
        }
        
    }
    else{
        // 队列虽然空了，但如果发送还没完成 (BUSY)，则不能切RX
        if(hrf->TxState != TX_BUSY_Tramsmit) {
            // 中断回调已处理地址恢复，这里只确保RX模式
            HAL_RF_SetRxMode(hrf);
        }
    }
}


/**
 * @brief  直接发送数据(不经过队列)，用于ACK payload模式下的单帧发送
 * @param  dest_addr: 目标地址指针,5字节
 * @param  data: 要发送的数据
 * @param  len: 数据长度
 * @return HAL_OK=成功, 其他=失败(BUSY等)
 */
int8_t RF_Send(uint8_t *dest_addr, const uint8_t *data, uint8_t len)
{
    if(len > max_rf_payload_len) {
        len = max_rf_payload_len; //限制最大长度
    }

    // 确保TX模式
    if(__HAL_RF_Get_TRxMode_Bit() == 1) {
        HAL_RF_SetTxMode(&hrf);
    }

    // 设置目标地址
    HAL_RF_SetTxAddress(&hrf, dest_addr, 5);
    HAL_RF_SetRxAddress(&hrf, 0, dest_addr, 5);

    // 直接发送，不经过队列
    return HAL_RF_Transmit_IT(&hrf, (uint8_t*)data, len);
}
