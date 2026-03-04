#include <stdio.h>
#include "driver_bk24.h"
#include "driver_timer.h"
#include "bk3633_reglist.h"
#include "user_config.h"
#include <string.h> 
#include "hal_drv_rf.h"
#include "timer_handler.h"



#define UART_PRINTF    uart_printf

SYSTEM_STRUCT_DATA system_data;
uint32_t RF_flag=0;
#define LIMITS(x, min, max)  ((x)<(min)?(min):((x)>(max)?(max):(x)))


// 配置模拟寄存器，固定值不可改
static uint32_t Cfg_0c_Val[4] = {0x1F, 0x1B, 0x73, 0x00};
static uint32_t Cfg_0d_Val[4] = {0x36, 0xB4, 0x80, 0x28};


#include "hal_drv_rf.h"
#include "bk3633_reglist.h" // 包含硬件寄存器定义
// #include "driver_timer.h" // 如果需要微秒延时函数

static void RF_Write_fifo(uint8_t *pBuf, uint8_t bytes)
{
    uint8_t i;
    __HAL_RF_CMD_FLUSH_TXFIFO();

    __HAL_RF_CMD_W_TX_PAYLOAD();
    for(i=0; i<bytes; i++)
    {
        TRX_FIFO = pBuf[i];
    }
    __HAL_RF_CMD_NOP();
}
static void RF_Write_fifo_NoACK(uint8_t *pBuf, uint8_t bytes)
{
    uint8_t i;
    __HAL_RF_CMD_W_TX_PAYLOAD_NOACK();
    for(i=0; i<bytes; i++)
    {
        TRX_FIFO = pBuf[i];
    }
    __HAL_RF_CMD_NOP();
}

static void RF_Read_fifo(uint8_t *pBuf, uint8_t bytes)
{
    uint8_t i;
    __HAL_RF_CMD_R_RX_PAYLOAD();
    for(i=0; i<bytes; i++)
    {
        pBuf[i] = TRX_FIFO;
    }
    __HAL_RF_CMD_NOP();
}


static void RF_ReadBuf(uint8_t cmd, uint8_t *pBuf, uint8_t len) 
{
    uint8_t i;
    TRX_CMD = cmd;
    for(i = 0; i < len; i++) {
        pBuf[i] = TRX_FIFO;
    }
    TRX_CMD = 0x00;
}

static void RF_MemCpy(volatile uint32 *pDest, uint32_t *pSrc, uint8_t len) 
{
    uint8_t i;
    for (i = 0; i < len; i++) 
    {
        pDest[i] = pSrc[i];
    }
}

static void RF_DelayUs(uint32_t us) 
{
    volatile int x, y;
    for(y = 0; y < us; y ++ )
    {
        for(x = 0; x < 10; x++);
    }
}


/*---------------------HAL 接口实现------------------*/

/**
  * @brief  初始化 RF 模块
  * @param  hrf: RF 句柄
  */
HAL_StatusTypeDef HAL_RF_Init(RF_HandleTypeDef* hrf,RF_ConfgTypeDef *Init)
{
    if (Init == NULL || hrf == NULL) return HAL_ERROR;

    hrf->State = HAL_RF_STATE_RESET;
    
    // 1. 底层硬件复位/时钟开启 (MSP Params)
    //HAL_RF_MspParams(hrf);
    
    //清除外设电源掉电位 (Enable Peripheral Clock)
    clrf_SYS_Reg0x3_bk24_pwd;

    /* 配置默认模拟寄存器参数 (原厂Magic Numbers) */
    RF_MemCpy(&TRX_CFG_0C_0, (uint32_t*)Cfg_0c_Val, 4);
    RF_MemCpy(&TRX_CFG_0D_0, (uint32_t*)Cfg_0d_Val, 4);


    TRX_CONFIG = 0x0C; // Default: PTX mode, CRC(2 bytes) enabled, PWR_UP=0
    
    /* tx / rx配置 */
    if (Init->Mode == MODE_TX) {
       TRX_CONFIG &= ~(1<<0); // Clear PRIM_RX bit for TX mode
    } else if (Init->Mode == MODE_RX) {
        TRX_CONFIG |= (1<<0);  // Set PRIM_RX bit for RX mode
    } else {
        return HAL_ERROR;
    }
 
    /* 地址宽度 */
    uint8_t tmp_addr_width=Init->Protocol.AddressWidth;
    if(tmp_addr_width < 3 || tmp_addr_width > 5)
        return HAL_ERROR; // 地址宽度无效
    __HAL_RF_SET_ADDR_WIDTH(tmp_addr_width);

    //发送地址转uint32_t类型数组
    uint32_t tmp_addr_buffer[tmp_addr_width];
    for(uint8_t i=0; i<tmp_addr_width; i++){
        tmp_addr_buffer[i] = Init->Protocol.TxAddress[i];
    }
    RF_MemCpy(&TRX_TX_ADDR_0, tmp_addr_buffer, tmp_addr_width);   // TX 发送地址
    Init->Protocol.Support_NoAck ? __HAL_RF_EN_NOACK() : __HAL_RF_DIS_NOACK();    // 是否支持无ACK发送命令
    __HAL_RF_TX_RETRY(Init->Protocol.AutoRetransmitDelay, Init->Protocol.AutoRetransmitCount); //自动重传设置

    /* 接收pipes设置 */
    for(uint8_t i=0; i<6; i++)
    {
        Init->Protocol.RxPipes[i].Enable ? __HAL_RF_EN_RXADDR_PIPE(i) : __HAL_RF_DIS_RXADDR_PIPE(i);
        Init->Protocol.RxPipes[i].AutoAck ? __HAL_RF_EN_AA_PIPE(i) : __HAL_RF_DIS_AA_PIPE(i);
        
        //动态包长依赖 EN_DPL and ENAA
        if(Init->Protocol.RxPipes[i].EnDynamicPayload){
            __HAL_RF_EN_DPL();      //EN_DPL
            __HAL_RF_EN_AA_PIPE(i); //EN_AA
            __HAL_RF_EN_DPL_PIPE(i);//EN_DPL_PIPE
        }
        else{
            __HAL_RF_DIS_DPL_PIPE(i);
        }

        /* 无论是否是动态包长，都设置一个固定包长;启用动态包长时这些寄存器无效，不影响运行 */
        uint8_t PW = Init->Protocol.RxPipes[i].PayloadWidth;
        if(PW>32) PW=32; //最大32字节
        switch(i)
        {
            case 0: TRX_RX_PW_P0 = PW; break;
            case 1: TRX_RX_PW_P1 = PW; break;
            case 2: TRX_RX_PW_P2 = PW; break;
            case 3: TRX_RX_PW_P3 = PW; break;
            case 4: TRX_RX_PW_P4 = PW; break;
            case 5: TRX_RX_PW_P5 = PW; break;
            default: break;
        }

        Init->Protocol.RxPipes[i].Support_Payload_Attach_ACK ? __HAL_RF_EN_ACK_PAY() : __HAL_RF_DIS_ACK_PAY();
    }
    //接收地址设置
    uint32_t tmp_rxaddr_buffer[tmp_addr_width];
    for(uint8_t i=0; i<tmp_addr_width; i++){
        tmp_rxaddr_buffer[i] = Init->Protocol.RxPipes[0].Address[i];
    }
    RF_MemCpy(&TRX_RX_ADDR_P0_0, tmp_rxaddr_buffer, tmp_addr_width);
    
    for(uint8_t i=0; i<tmp_addr_width; i++){
        tmp_rxaddr_buffer[i] = Init->Protocol.RxPipes[1].Address[i];
    }
    RF_MemCpy(&TRX_RX_ADDR_P1_0, tmp_rxaddr_buffer, tmp_addr_width);
    
    TRX_RX_ADDR_P2 = Init->Protocol.RxPipes[2].Address[0];
    TRX_RX_ADDR_P3 = Init->Protocol.RxPipes[3].Address[0];
    TRX_RX_ADDR_P4 = Init->Protocol.RxPipes[4].Address[0];
    TRX_RX_ADDR_P5 = Init->Protocol.RxPipes[5].Address[0];


    /* ---------------- 射频物理层配置 ---------------- */
    //信道
    //TRX_RF_CH =hrf->CurrentChannel; //还不太理解这个参数，姑且写着0x05
    TRX_RF_CH =0x05;

    __HAL_RF_PowerDown();

    /* XVR 模拟参数初始化 */
    addXVR_Reg0x24 = 0x000e0782;
    addXVR_Reg0x3b = 0x36340048;
    uint32_t bps=0;
    /* 数据速率 */
    switch ( Init->DataRate )
    {
        case BPS_250K :  // 250kbps
            addXVR_Reg0x2e &= 0xfffffe00 ;
            addXVR_Reg0x2e |= 0x100 ;
            addXVR_Reg0x26 &= 0xe000ffff;
            addXVR_Reg0x26 |= 0x04080000;
            addXVR_Reg0x30 &= ~(0x1ff<<8);
            addXVR_Reg0x30 |= 0x28<<8;
            TRX_RF_SETUP = 0x27;
            bps = 250000;
            break;
        case BPS_1M : // 1Mbps
            addXVR_Reg0x2e &= 0xfffffe00 ;
            addXVR_Reg0x2e |= 0x100 ;
            addXVR_Reg0x26 &= 0xe000ffff;
            addXVR_Reg0x26 |= 0x10200000;
            TRX_RF_SETUP = 0x07;
            bps = 1000000;
            break;
        case BPS_2M : // 2Mbps
            addXVR_Reg0x2e &= 0xfffffe00 ;
            addXVR_Reg0x2e |= 0x100 ;
            addXVR_Reg0x26 &= 0xe000ffff;
            addXVR_Reg0x26 |= 0x10200000;
            TRX_RF_SETUP = 0x0F;
            bps = 2000000;
            break;
        default:
            return HAL_ERROR;
            break;
    }

    uart_printf("set bps success\r\n", __FILE__, __LINE__);

    xvr_reg_initial_24(); // 配置 XVR 寄存器

    __HAL_RF_PowerUp();
    __HAL_RF_CMD_FLUSH_RXFIFO();
    __HAL_RF_CMD_FLUSH_TXFIFO();
    __HAL_RF_CLEAR_IRQ_FLAGS((IRQ_RX_DR_MASK|IRQ_TX_DS_MASK|IRQ_MAX_RT_MASK|RX_P_NO));

    /* 中断配置 */
    Init->IRQ.RxDR.enable ? __HAL_RF_EN_IRQ_RX_DR() : __HAL_RF_DIS_IRQ_RX_DR();
    Init->IRQ.TxDS.enable ? __HAL_RF_EN_IRQ_TX_DS() : __HAL_RF_DIS_IRQ_TX_DS();
    Init->IRQ.MaxRT.enable ? __HAL_RF_EN_IRQ_MAX_RT() : __HAL_RF_DIS_IRQ_MAX_RT();

    /* 发射功率 */
    HAL_RF_SetTxPower(hrf, Init->TxPower);

    // 3. 初始化 RF_HandleTypeDef 结构体
    hrf->Params = *Init;

    //整包发送时间（us） = (包长字节数 × 8 × 1,000,000) / bps
    //超时时间=(1+maxrt)*整包发送时间 +重发间隔*maxrt +冗余时间
    uint16_t pack_time_us =  ( (32*8*1000000) / bps ); //最大包长32字节
    uint32_t tx_timeout_us = ( (1+Init->Protocol.AutoRetransmitCount)*pack_time_us + 
                             (Init->Protocol.AutoRetransmitDelay+1)*250*Init->Protocol.AutoRetransmitCount + 
                             500 //500是冗余时间
                             ); 
    uart_printf("tx_timeout_us=%d\r\n", tx_timeout_us);

    hrf->TimeManager.Tx_TimeOut = (tx_timeout_us / 1000);
    hrf->State = HAL_RF_STATE_READY;

    return HAL_OK;
}

/**
  * @brief  注册系统时间获取函数，用于超时管理，一定要注册！发送函数依赖此函数
  * @param  hrf: RF 句柄
  * @param  GetSysTimeMs: 获取系统时间函数指针，单位ms
  */
HAL_StatusTypeDef HAL_RF_TimeManager_register(RF_HandleTypeDef *hrf, uint32_t (*GetSysTimeMs)(void))
{
    if (hrf == NULL || GetSysTimeMs == NULL) return HAL_ERROR;

    hrf->TimeManager.GetSysTimeMs = GetSysTimeMs;

    return HAL_OK;
}

/* IO 操作函数 */

/**
  * @brief  阻塞式发送数据(调通)
  * @return
  */
HAL_StatusTypeDef HAL_RF_Transmit_ACK(RF_HandleTypeDef *hrf, uint8_t *pData, uint8_t Size)
{
    HAL_StatusTypeDef ret;
    uint8_t irq_status;         //中断挂起状态
    uint8_t isTimeout = 0;      // 是否超时标志
    uint32_t txWaitCounter = 0; // 发送等待计数器（用于超时判断）
    const uint16_t MAX_TX_TIMEOUT = 100; // 最大等待时间，单位10us (10000*10us=100ms)
    static uint16_t send_cnt=0;
    if (hrf == NULL || pData == NULL || Size == 0) return HAL_RF_STATE_ERROR;

    //进入发送模式
    HAL_RF_SetTxMode(hrf);

    //清空FIFO和中断
    __HAL_RF_CMD_FLUSH_RXFIFO();
    __HAL_RF_CMD_FLUSH_TXFIFO();
    __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK | IRQ_TX_DS_MASK | IRQ_MAX_RT_MASK | RX_P_NO);

    __HAL_RF_CHIP_EN();
    RF_Write_fifo(pData, Size);

    do{
        irq_status = __HAL_RF_GET_IRQ_Status();
        RF_DelayUs(10);
        txWaitCounter++;
        if (txWaitCounter > MAX_TX_TIMEOUT) {
            isTimeout = 1;
            uart_printf("TX timeout!irq=0x%x\r\n", irq_status);
            break; 
        }
    }
    while((irq_status & IRQ_TX_DS_MASK) == 0x00); //没有发送完成标志则继续等待

    //如果发送完成(收到ACK)，则清除标志位和FIFO
    if (irq_status & IRQ_TX_DS_MASK) {
        __HAL_RF_CMD_FLUSH_TXFIFO();
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_TX_DS_MASK);
        uart_printf("TX DS!send_cnt=%d\r\n", send_cnt++);
        ret = HAL_RF_STATE_READY;
    }

    //如果在发送时，接收FIFO里有数据，忽略之
    if (irq_status & IRQ_RX_DR_MASK) {
        __HAL_RF_CMD_FLUSH_RXFIFO();
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK);
        // ret = HAL_RF_STATE_READY;
    }

    //如果达到最大重发次数仍未收到ACK，则视为发送超时，重新初始化RF(原厂写法，有待商榷)
    if ((irq_status & IRQ_MAX_RT_MASK) || isTimeout) {
        __HAL_RF_CMD_FLUSH_RXFIFO();
        __HAL_RF_CMD_FLUSH_TXFIFO();
        __HAL_RF_PowerDown();
        __HAL_RF_PowerUp();
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_MAX_RT_MASK);

        hrf->State = HAL_RF_STATE_TIMEOUT;
        ret = HAL_RF_STATE_TIMEOUT; //发送超时
    }

    __HAL_RF_CHIP_DIS();
    return ret;
}

/**
  * @brief  借助中断方式实现发送状态机(调通)
  * @param  hrf: RF 句柄
  * @param  pData: 待发送数据指针
  * @param  Size: 待发送数据长度
  * @return HAL 状态
  */
HAL_StatusTypeDef HAL_RF_Transmit_IT(RF_HandleTypeDef *hrf, uint8_t *pData, uint8_t Size)
{
    /* 入参检查 */
    if (hrf == NULL || pData == NULL || Size == 0) return HAL_ERROR;
    /* 查询当前模式 */
    if(__HAL_RF_Get_TRxMode_Bit() !=0) {
        uart_printf("Current mode is not TX,return \r\n");
        HAL_RF_SetTxMode(hrf);
    }   

    /* 查询是否空闲 */
     /* 如果正在发送，检查是否超时 */
    if (hrf->TxState != TX_IDLE) {
        uart_printf("Tx-busy\r\n");
        uart_printf("cur_time:%d, start_time:%d, timeout:%d,Mode=%d\r\n",
                    hrf->TimeManager.GetSysTimeMs(),
                    hrf->TimeManager.Tx_start_time,
                    hrf->TimeManager.Tx_TimeOut,
                    __HAL_RF_Get_TRxMode_Bit());

        uint32_t cur_time = hrf->TimeManager.GetSysTimeMs();
        uint32_t elapsed_time = cur_time - hrf->TimeManager.Tx_start_time;
        if (elapsed_time >= (hrf->TimeManager.Tx_TimeOut)) {
            /* 发送超时处理 */
            /* 按照经验，这个超时不会触发，一直触发就是没切换到tx(maxrt和tx_ds中断都不触发)*/
            uart_printf("TX timeout detected,\r\n");
            hrf->TxState = TX_TIMEOUT;
            hrf->TimeManager.Tx_Timeout_cnt++;
            /* 清除缓冲区 */
            __HAL_RF_CMD_FLUSH_TXFIFO();
            /* 重置为IDLE状态*/
            hrf->TxState = TX_IDLE;
        }else{
            /* 仍在发送中，返回忙碌状态 */
            return HAL_BUSY;
        }
    }
    
    /* 准备发送 */
    hrf->TxState = TX_BUSY_Tramsmit;
    hrf->TimeManager.Tx_TimeOut_flag=0;
    hrf->TimeManager.Tx_start_time = hrf->TimeManager.GetSysTimeMs();
    RF_Write_fifo(pData, Size);

    return HAL_OK;

}

/**
  * @brief  无ACK发送数据(调通)
  * @return
  */
HAL_StatusTypeDef HAL_RF_Transmit_NoACK(RF_HandleTypeDef *hrf, uint8_t *pData, uint8_t Size)
{

    if(hrf->Params.Protocol.Support_NoAck !=1)
    {
        return HAL_RF_STATE_ERROR;
    }
    
    uint8_t irq_status;
    __HAL_RF_CMD_FLUSH_RXFIFO();
    __HAL_RF_CMD_FLUSH_TXFIFO();
    __HAL_RF_CHIP_EN();
    RF_Write_fifo_NoACK(pData, Size);

    //sleep and wake up by rf interrupt
    //ENABLE_RF_WAKEUP;
    //driver_idle_osc8mhz();
    //DISABLE_RF_WAKEUP;
    do {
        irq_status = TRX_IRQ_STATUS;
    }
    while((irq_status & IRQ_TX_DS_MASK) == 0x00); //等待发送完成标志
    
    if(irq_status & IRQ_TX_DS_MASK){
        //RF_CMD_FLUSH_TX;
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_TX_DS_MASK);
    }

    __HAL_RF_CHIP_DIS();
    return 1;

}
/**  
  * @brief  RF轮询接收数据
  * @param  pData: 接收缓冲区
  * @param  Size:  期望读取长度 (如果 FIFO 数据小于此长度，将只读取实际长度)
  * @return 
  */
HAL_StatusTypeDef HAL_RF_Receive(RF_HandleTypeDef *hrf, uint8_t *pData, uint8_t* Size)
{
    uint8_t irq_status;
    uint8_t fifo_status;
    uint8_t len;
    uint8_t rx_cnt = 0;

    if (hrf == NULL || pData == NULL ) return HAL_RF_STATE_ERROR;

    irq_status = __HAL_RF_GET_IRQ_Status();
    uart_printf("irq_status: 0x%02X\r\n", irq_status);

    if (irq_status & IRQ_RX_DR_MASK){  //接收标志位置1，接收FIFO中的数据就绪
        do {
            len = TRX_RX_RPL_WIDTH;
            if (len <= RF_MAX_PACKET_LEN) { //数据长度有效，读取
                RF_Read_fifo(pData, len);
                rx_cnt++;
            } else {                        //数据长度无效，清空FIFO
                __HAL_RF_CMD_FLUSH_RXFIFO();
                break;
            }
            if(Size) {
                *Size = len; //返回实际读取长度
            }
            fifo_status = __HAL_RF_GET_IRQ_Status();
        } while (__HAL_RF_GET_FIFO_STATUS_FLAGS(RF_RX_EMPTY)!=0); //FIFO非空则继续

        // 清除中断标志
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK);

        hrf->State = HAL_RF_STATE_READY;
        return HAL_OK;
    }

    return HAL_BUSY; // 没有数据
}

/**
  * @brief  附加应答负载数据到指定接收管道
  * @param  pipes: 接收管道号 (0-5)
  * @param  pBuf: 负载数据指针
  * @param  len:  负载数据长度 (最大32字节)
  */
HAL_StatusTypeDef HAL_RF_Attach_PL2ACK(RF_HandleTypeDef *hrf, uint8_t pipes, uint8_t *pBuf, uint8_t len) 
{
    /* 入参检查 */
    if(pipes > 5 || pBuf == NULL || len == 0 || len > 32) {
        return HAL_RF_STATE_ERROR; 
    }
    if(hrf->Params.Protocol.RxPipes[pipes].Support_Payload_Attach_ACK != 1) {
        return HAL_RF_STATE_ERROR; 
    }

    uint8_t i;
    __HAL_RF_CMD_RX_W_ACK_PAYLOAD(pipes);
    for(i = 0; i < len; i++) 
    {
        TRX_FIFO = pBuf[i];
    }
    __HAL_RF_CMD_NOP();

    return HAL_RF_STATE_READY;
}


/* 不太理解这个，还需要斟酌 */
void HAL_RF_SetChannel(RF_HandleTypeDef *hrf, uint8_t channel)
{
    hrf->CurrentChannel = channel;
    __HAL_RF_CHIP_DIS();
    TRX_RF_CH = 0x80 + channel; 
    __HAL_RF_CHIP_EN();
}

/**
  * @brief  设置发送地址
  * @param  hrf: RF 句柄
  * @param  dev_addr: 发送地址指针,uint8_t类型，内部转成uint32类型赋值给寄存器
  * @param  length: 地址宽度 必须传入和AddressWidth相同的值！
  */
HAL_StatusTypeDef HAL_RF_SetTxAddress(RF_HandleTypeDef *hrf, uint8_t *dev_addr,uint8_t length)
{   
    if(length!=hrf->Params.Protocol.AddressWidth) 
        return HAL_RF_STATE_ERROR;

    memcpy(hrf->Params.Protocol.TxAddress, dev_addr, sizeof(uint8_t)*length);

    uint32_t tmp_buffer[length];
    for(uint8_t i=0; i<length; i++){
        tmp_buffer[i] = hrf->Params.Protocol.TxAddress[i];
    }

    RF_MemCpy(&TRX_TX_ADDR_0, tmp_buffer, hrf->Params.Protocol.AddressWidth);
    return HAL_RF_STATE_READY;
}



/**
  * @brief  设置接收通道地址
  * @param  hrf: RF 句柄
  * @param  dev_addr: 接收地址指针,uint8类型，内部转成uint32类型
  * @param  pipe: 接收管道号 (0-5)
  * @param  length: 地址宽度 必须传入和AddressWidth相同的值
  */
HAL_StatusTypeDef HAL_RF_SetRxAddress(RF_HandleTypeDef *hrf, uint8_t pipe, uint8_t *dev_addr,uint8_t length)
{
    if(pipe >5 || dev_addr == NULL) return HAL_RF_STATE_ERROR;
    if(length != hrf->Params.Protocol.AddressWidth)
        return HAL_RF_STATE_ERROR;

    memcpy(hrf->Params.Protocol.RxPipes[pipe].Address, dev_addr, sizeof(uint8_t)*length);

    // 转换地址类型
    uint32_t tmp_buffer[length];
    for(uint8_t i=0; i<length; i++){
        tmp_buffer[i] = hrf->Params.Protocol.RxPipes[pipe].Address[i];
    }

   
    switch(pipe){
        case 0:
            RF_MemCpy(&TRX_RX_ADDR_P0_0, tmp_buffer, hrf->Params.Protocol.AddressWidth);
            break;
        case 1:
            RF_MemCpy(&TRX_RX_ADDR_P1_0, tmp_buffer, hrf->Params.Protocol.AddressWidth);
            break;
        case 2:
            TRX_RX_ADDR_P2 = hrf->Params.Protocol.RxPipes[2].Address[0];
            break;
        case 3:
            TRX_RX_ADDR_P3 = hrf->Params.Protocol.RxPipes[3].Address[0];
            break;
        case 4:
            TRX_RX_ADDR_P4 = hrf->Params.Protocol.RxPipes[4].Address[0];
            break;
        case 5:
            TRX_RX_ADDR_P5 = hrf->Params.Protocol.RxPipes[5].Address[0];
            break;
        default:
            break;
    }
    return HAL_RF_STATE_READY;
}

HAL_StatusTypeDef HAL_RF_GetRxAddress(RF_HandleTypeDef *hrf, uint8_t pipe, uint8_t *dev_addr)
{
    //从hrf中获取pipe的地址
    if(pipe >5 || dev_addr == NULL) 
        return HAL_RF_STATE_ERROR;
    memcpy(dev_addr, hrf->Params.Protocol.RxPipes[pipe].Address, sizeof(uint8_t)*hrf->Params.Protocol.AddressWidth);
    return HAL_RF_STATE_READY;
}

/**
  * @brief  设置为发送模式
  * @param  hrf: RF 句柄
  */
HAL_StatusTypeDef HAL_RF_SetTxMode(RF_HandleTypeDef *hrf)
{
     
    if(__HAL_RF_Get_TRxMode_Bit() == 0){ //已经是发送模式
        __HAL_RF_PowerUp();
        __HAL_RF_CHIP_EN(); //使能返回
        return HAL_RF_STATE_READY; 
    }
    uart_printf("Tx:%d\n",Get_SysTick_ms());
    hrf->Params.Mode = MODE_TX;

    __HAL_RF_PowerUp();
    __HAL_RF_CMD_FLUSH_TXFIFO();
    __HAL_RF_CHIP_DIS();
    __HAL_RF_Set_TxMode_Bit();
    __HAL_RF_CHIP_EN();

    hrf->State = HAL_RF_STATE_READY;
    return HAL_RF_STATE_READY;
}


/**
  * @brief  设置为接收模式
  * @param  hrf: RF 句柄
  * @param  dev_addr: 发送地址指针
  * @param  length: 地址宽度 必须传入和AddressWidth相同的值
  */
HAL_StatusTypeDef HAL_RF_SetRxMode(RF_HandleTypeDef *hrf)
{
    

    if(__HAL_RF_Get_TRxMode_Bit() ==1){ //已经是接收模式
        // __HAL_RF_PowerUp();
        // __HAL_RF_CHIP_EN(); //使能返回
        return HAL_RF_STATE_READY; 
    }
    uart_printf("Rx:%d\n",Get_SysTick_ms());

    hrf->Params.Mode = MODE_RX;
    __HAL_RF_PowerUp();
    __HAL_RF_CMD_FLUSH_RXFIFO();

    __HAL_RF_CHIP_DIS();
    __HAL_RF_Set_RxMode_Bit() ;
    __HAL_RF_CHIP_EN();

    hrf->State = HAL_RF_STATE_READY;
    return HAL_RF_STATE_READY;
}
/**
  * @brief  RF 中断处理函数，在intc.c的intc_irq/intc_fiq中调用
  * @param  hrf: RF 句柄
  */
void HAL_RF_IRQ_Handler(RF_HandleTypeDef *hrf)
{
    if(__HAL_RF_GET_IRQ_FLAGS(IRQ_RX_DR_MASK)){
         uart_printf("in RX_DR\r\n");
        /* 读取数据到hrf队列 */
        
        //fifo是三层结构，每一层32字节，一次读一层，三层 都没数据则fifo为空
        uint8_t fifo_status;
        do { 
            hrf->RxLen = __HAL_RF_GET_RX_RPL_WIDTH(); //动态载荷长度
            hrf->RxPipes = __HAL_RF_GET_RX_PIPEID();  //接收管道号
            
            RF_Read_fifo((uint8_t*)(hrf->RxBuff), hrf->RxLen);
            hrf->RxBuff_valid = 1;

            if(hrf->Params.IRQ.RxDR.user_cb != NULL){
                hrf->Params.IRQ.RxDR.user_cb(hrf);
            }

            //检查fifo是否为空
            fifo_status = __HAL_RF_GET_FIFO_STATUS();
        }while ( (fifo_status & RF_RX_EMPTY) == 0 ); 

        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK);
        __HAL_RF_CMD_FLUSH_RXFIFO();
    }

    if(__HAL_RF_GET_IRQ_FLAGS(IRQ_TX_DS_MASK)){
         uart_printf("in TX_DS\r\n");
        hrf->TxState = TX_Tramsmit_SUCCESS;

        if(hrf->Params.IRQ.TxDS.user_cb != NULL){
            hrf->Params.IRQ.TxDS.user_cb(hrf);
        }
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_TX_DS_MASK);
        //__HAL_RF_CMD_FLUSH_TXFIFO();
        hrf->TxState = TX_IDLE;
    }

    if(__HAL_RF_GET_IRQ_FLAGS(IRQ_MAX_RT_MASK)){
         uart_printf("in MAX_RT\r\n");
        hrf->TxState = TX_Tramsmit_FAIL;
        if(hrf->Params.IRQ.MaxRT.user_cb != NULL){
            hrf->Params.IRQ.MaxRT.user_cb(hrf);
        }
        __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_MAX_RT_MASK);
        __HAL_RF_CMD_FLUSH_TXFIFO();//发送失败必须要清空TX FIFO，下次才能继续写FIFO发送

        hrf->TxState = TX_IDLE;
    }

   // return;

}


/**
  * @brief  清除指定的中断标志
  * @param  hrf: RF 句柄
  * @param  _Flags: 要清除的中断标志位掩码组合 IRQ_StatusBitsTypeDef
  */
void HAL_RF_ClearIRQFlags(RF_HandleTypeDef *hrf, IRQ_StatusBitsTypeDef _Flags)
{
    (void)hrf;
    // 写 1 清除
    TRX_IRQ_STATUS = _Flags;
}

/**
  * @brief  检查某个中断标志位是否置位
  * @param  hrf: RF 句柄
  * @param  Flags: 要检查的中断标志位掩码组合 IRQ_StatusBitsTypeDef
  * @return 注意返回的是0或者非0值,非零值不一定是1
  */
uint8_t HAL_RF_GetIRQFlags(RF_HandleTypeDef *hrf, IRQ_StatusBitsTypeDef IRQ_Flags)
{
    (void)hrf;
    
    return (uint8_t)((TRX_IRQ_STATUS & (IRQ_Flags)));
}

/**
  * @brief  检查FIFO状态标志位是否置位
  * @param  hrf: RF 句柄
  * @param  Flags: 要检查的FIFO状态标志位掩码组合 RF_FIFOStatusBitsTypeDef
  * @return 注意返回的是0或者非0值,非零值不一定是1；  判断返回值时请与0做比较
  */
uint8_t HAL_RF_Get_FifoStatus_Flags(RF_HandleTypeDef *hrf, RF_FIFOStatusBitsTypeDef Flags)
{
    (void)hrf;

    return (uint8_t)(TRX_FIFO_STATUS & (Flags));
}

uint8_t HAL_RF_GetRSSI(RF_HandleTypeDef *hrf)
{
    (void)hrf;
    return (uint8_t)get_XVR_Reg0x13_rssi_ind_o;
}

/**
  * @brief  设置发射功率
  * @param  hrf: RF 句柄
  * @param  power_level: 发射功率等级,取值0~ff,数值越大功率越大
  */
uint8_t HAL_RF_SetTxPower(RF_HandleTypeDef *hrf, RF_TxPowerTypeDef power_level)
{
    LIMITS(power_level,0,0x0f);
    hrf->Params.TxPower = power_level;

    uint32_t XVR_REG24_BAK;
    uint32_t regv_0x24;

    extern volatile uint32_t XVR_ANALOG_REG_BAK[32];

    XVR_ANALOG_REG_BAK[4] |=1<<29;
    addXVR_Reg0x4 = XVR_ANALOG_REG_BAK[4];
    regv_0x24=addXVR_Reg0x24&0xFFFFF87F;
    regv_0x24=regv_0x24|(power_level<<7);
    addXVR_Reg0x24 = regv_0x24;
    XVR_REG24_BAK = addXVR_Reg0x24;
    return 1;
}
