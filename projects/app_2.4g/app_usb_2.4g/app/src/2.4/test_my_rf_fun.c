// #include "app.h"
// #include "bk3633_reglist.h"
// #include "icu.h"
// #include "driver_timer.h"
// #include <string.h>

// #include "intc.h"            // interrupt controller
// #include "my_drv_uart.h"            // uart definitions
// #include "BK3633_Reglist.h"
// #include "gpio.h"
// #include "spi.h"
// #include "i2c.h"
// #include "icu.h"
// #include "dma.h"
// #include "reg_intc.h"
// #include "driver_timer.h"
// #include "app.h"


// #include "hal_drv_rf.h"

// #define UART_PRINTF    uart_printf
// int uart_printf(const char *fmt,...);

// #if 0
// void rf_simple_init(void)
// {
//     Rf_Init();           // 射频模块初始化
//     SetDataRate(2);      // 设置速率为2Mbps

//     uart_printf("in 2.4 mode1===============\r\n");
//     xvr_reg_initial_24();
//     PowerUp_Rf();      // 射频模块上电
//     RF_CHIP_ENABLE;    // 使能射频芯片
//     RF_CMD_FLUSH_TX;   // 清空发送FIFO
// }

// void bk24_send_data(void)
// {
//     static uint32_t packet_count = 0;
//     uint8_t rf_fifo_data[32];

//     for(uint8_t i = 0; i < MAX_PACKET_LEN; i++) 
//     {
//         rf_fifo_data[i] = i;
//     }

//     SwitchToTxMode();

//     while(1)
//     {
//         RF_CMD_FLUSH_TX;              // 清空TX FIFO
//         RF_CMD_CLEAR_STATUS_ALL;      // 清除中断标志
        
//         // 写入数据并发送
//         W_TX_PAYLOAD(rf_fifo_data, MAX_PACKET_LEN);
//         RF_CHIP_ENABLE;
//         Delay_us(15);
//         RF_CHIP_DISABLE;
        
//         packet_count++;
        
//         // 简单打印，每100包打印一次
//         if(packet_count % 100 == 0) 
//         {
//             uart_printf("send %ld packet\n", packet_count);
//         }
        
//         // 简单延时，控制发送速率
//         Delay_us(100);
//     }
// }

// void rf_simple_receive()
// {
//     SwitchToRxMode();
//     uint8_t rec[32];
//     uint16_t count_packet; 
//     uint16_t count_printf; 
//     while(1)
//     {
//         if (TRX_IRQ_STATUS & STATUS_RX_DR)
//         {
//             R_RX_PAYLOAD(rec,32);
//             RF_CMD_CLEAR_STATUS_RX;
//             count_packet++;
//             uart_printf("recieve %ld packet: ", count_packet);
//             // 按照十进制打印接收到的数据
//             for(uint8_t i = 0; i < 32; i++)
//             {
//                 uart_printf("%d ", rec[i]);
//             }
//             uart_printf("\n");
//         }
//         else
//         {
//             count_printf++;
//             if(count_printf % 10000 == 0)
//             {
//                 uart_printf("not received\n");
//             }

//         }

//     }

// }

// #endif

// volatile uint16_t isq_count_rxdr = 0;
// volatile uint16_t isq_count_txds = 0;
// volatile uint16_t isq_count_maxrt = 0;
// volatile uint32_t packt_count_intc=0;

// RF_HandleTypeDef hrf;
// void rxdr_callback(RF_HandleTypeDef *hrf)
// {
//     isq_count_rxdr++;
// }
// void txds_callback(RF_HandleTypeDef *hrf)
// {
//     isq_count_txds++;
//     uart_printf("%d,%d\n", isq_count_txds, packt_count_intc);
//     // packt_count_intc++;
   
// }
// void maxrt_callback(RF_HandleTypeDef *hrf)
// {
//     isq_count_maxrt++;
//     uart_printf("%d\n", isq_count_maxrt);
// }

// RF_ConfgTypeDef Init_S_normal=
// {
//     .Mode = MODE_TX,
//     .DataRate = BPS_2M,
//     .TxPower = RF_TX_POWER_0dBm,
//     .Channel = 0x05, // 频道5
//     .Protocol ={
//         .AddressWidth = 5,
//         .TxAddress = {0x10, 0x11, 0x36, 0x00, 0x00},
//         .Support_NoAck = 1,             
//         .AutoRetransmitDelay = 2,        // 最大重传延迟 750us
//         .AutoRetransmitCount = 3,        // 最大重传次数 15
        
//         .RxPipes[0] = {
//             .PipeNum = 0,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x10, 0x11, 0x36, 0x00, 0x00}
//         },
//         .RxPipes[1] = {
//             .PipeNum = 1,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x10, 0x56, 0x24, 0xCD, 0x78}
//         },
//         .RxPipes[2]={
//             .PipeNum = 2,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x11, 0, 0, 0, 0}
//         },
//         .RxPipes[3]={
//             .PipeNum = 3,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x12, 0, 0, 0, 0}
//         },
//         .RxPipes[4]={
//             .PipeNum = 4,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x13, 0, 0, 0, 0}
//         },
//         .RxPipes[5]={
//             .PipeNum = 5,
//             .Enable = 1,
//             .AutoAck = 1,
//             .EnDynamicPayload = 1,
//             .PayloadWidth = 32,
//             .Support_Payload_Attach_ACK = 1,
//             .Address = {0x14, 0, 0, 0, 0}
//         }
//     },
//     .IRQ = {
//         .RxDR = {
//             .enable = 1,
//             .user_cb = rxdr_callback
//         },
//         .TxDS = {
//             .enable = 1,
//             .user_cb = txds_callback
//         },
//         .MaxRT = {
//             .enable = 1,
//             .user_cb = maxrt_callback
//         }
//     }
// };

// uint8_t RF_ADDRESS_LEN=5;

// static printf_all_registers(void)
// {
//     uart_printf("TRX_CONFIG     = 0x%02X\r\n", TRX_CONFIG);
//     uart_printf("TRX_EN_AA      = 0x%02X\r\n", TRX_EN_AA);
//     uart_printf("TRX_EN_RXADDR  = 0x%02X\r\n", TRX_EN_RXADDR);
//     uart_printf("TRX_SETUP_AW   = 0x%02X\r\n", TRX_SETUP_AW);
//     uart_printf("TRX_SETUP_RETR = 0x%02X\r\n", TRX_SETUP_RETR);
//     uart_printf("TRX_RF_CH      = 0x%02X\r\n", TRX_RF_CH);
//     uart_printf("TRX_RF_SETUP   = 0x%02X\r\n", TRX_RF_SETUP);

//     uart_printf("TRX_RX_ADDR_P0 = ");
//     for(int i=0;i<RF_ADDRESS_LEN;i++) uart_printf("%02X ", ((volatile uint8_t*)&TRX_RX_ADDR_P0_0)[i]);
//     uart_printf("\r\n");

//     uart_printf("TRX_RX_ADDR_P1 = ");
//     for(int i=0;i<RF_ADDRESS_LEN;i++) uart_printf("%02X ", ((volatile uint8_t*)&TRX_RX_ADDR_P1_0)[i]);
//     uart_printf("\r\n");

//     uart_printf("TRX_RX_ADDR_P2 = 0x%02X\r\n", TRX_RX_ADDR_P2);
//     uart_printf("TRX_RX_ADDR_P3 = 0x%02X\r\n", TRX_RX_ADDR_P3);
//     uart_printf("TRX_RX_ADDR_P4 = 0x%02X\r\n", TRX_RX_ADDR_P4);
//     uart_printf("TRX_RX_ADDR_P5 = 0x%02X\r\n", TRX_RX_ADDR_P5);

//     uart_printf("TRX_TX_ADDR    = ");
//     for(int i=0;i<RF_ADDRESS_LEN;i++) uart_printf("%02X ", ((volatile uint8_t*)&TRX_TX_ADDR_0)[i]);
//     uart_printf("\r\n");

//     uart_printf("TRX_RX_PW_P0   = 0x%02X\r\n", TRX_RX_PW_P0);
//     uart_printf("TRX_RX_PW_P1   = 0x%02X\r\n", TRX_RX_PW_P1);
//     uart_printf("TRX_RX_PW_P2   = 0x%02X\r\n", TRX_RX_PW_P2);
//     uart_printf("TRX_RX_PW_P3   = 0x%02X\r\n", TRX_RX_PW_P3);
//     uart_printf("TRX_RX_PW_P4   = 0x%02X\r\n", TRX_RX_PW_P4);
//     uart_printf("TRX_RX_PW_P5   = 0x%02X\r\n", TRX_RX_PW_P5);

//     uart_printf("TRX_DYNPD      = 0x%02X\r\n", TRX_DYNPD);
//     uart_printf("TRX_FEATURE    = 0x%02X\r\n", TRX_FEATURE);

//     uart_printf("addXVR_Reg0x24 = 0x%08lX\r\n", addXVR_Reg0x24);
//     uart_printf("addXVR_Reg0x3b = 0x%08lX\r\n", addXVR_Reg0x3b);
//     uart_printf("addXVR_Reg0x2e = 0x%08lX\r\n", addXVR_Reg0x2e);
//     uart_printf("addXVR_Reg0x26 = 0x%08lX\r\n", addXVR_Reg0x26);
//     uart_printf("addXVR_Reg0x2  = 0x%08lX\r\n", addXVR_Reg0x2);
//     uart_printf("addXVR_Reg0x2c = 0x%08lX\r\n", addXVR_Reg0x2c);
//     uart_printf("addXVR_Reg0x2d = 0x%08lX\r\n", addXVR_Reg0x2d);
//     uart_printf("addXVR_Reg0x3a = 0x%08lX\r\n", addXVR_Reg0x3a);
// }

// static void memcpy_2461(volatile uint32 *p, uint32 *val, uint8 len)
// {
//     uint8_t i = 0;
//     for ( i = 0; i < len ; i++ )
//     {
//         p[i] = val[i];
//     }
// }
// void rf_simple_init_old(void)
// {
//     clrf_SYS_Reg0x3_bk24_pwd;
   
//     uint32 Cfg_0c_Val[4] = {0x1F, 0x1B, 0x73, 0x00};
//     uint32 Cfg_0d_Val[4] = {0x36, 0xB4, 0x80, 0x28};
//     uint32 P0_Address[5] = {0x10, 0x11, 0x36, 0x0, 0x0};  
//     uint32 P1_Address[5] = {0x10, 0x56, 0x24, 0xCD, 0x78};

//     memcpy_2461(&TRX_CFG_0C_0, Cfg_0c_Val, 4);
//     memcpy_2461(&TRX_CFG_0D_0, Cfg_0d_Val, 4);
// //    memcpy_2461(&TRX_RAMP_TABLE_0, Ramp_Table_Val, 11);
//     TRX_CONFIG     = 0x0C;     // default PTX
//     TRX_EN_AA      = 0x3F;     // 使能6个管道自动应答
//     TRX_EN_RXADDR  = 0X3F;     // 使能6个管道接收地址
//     TRX_SETUP_AW   = 0x03;      // 地址宽度5字节
//     TRX_SETUP_RETR = 0x33;      // 自动重传延时750us, 重传3次
//     TRX_RF_CH      = 0x05;       // 频道5
//     TRX_RF_SETUP   = 0x0f;      // 2Mbps
    
//     //管道0~5接收地址 pipeline address
//     memcpy_2461(&TRX_RX_ADDR_P0_0, P0_Address, RF_ADDRESS_LEN); 
//     memcpy_2461(&TRX_RX_ADDR_P1_0, P1_Address, RF_ADDRESS_LEN);
//     TRX_RX_ADDR_P2 = 0x11;
//     TRX_RX_ADDR_P3 = 0x12;
//     TRX_RX_ADDR_P4 = 0x13;
//     TRX_RX_ADDR_P5 = 0x14;

//     memcpy_2461(&TRX_TX_ADDR_0, P0_Address, RF_ADDRESS_LEN);
    
//     //pipline 0~5 单次数据包长度
//     TRX_RX_PW_P0 = 0x20; //32字节
//     TRX_RX_PW_P1 = 0x20;
//     TRX_RX_PW_P2 = 0x20;
//     TRX_RX_PW_P3 = 0x20;
//     TRX_RX_PW_P4 = 0x20;
//     TRX_RX_PW_P5 = 0x20;

//     TRX_DYNPD    = 0x3F; 
//     TRX_FEATURE  = 0x07; 
// //    TRX_PLL_SDM &= 0xEF;    // open_loop_en=0
   
//     TRX_CONFIG &= 0xFD;
    
//     //设置xvr寄存器初始值
//     addXVR_Reg0x24 = 0x000e0782;
//     addXVR_Reg0x3b = 0x36340048;

//     addXVR_Reg0x2e &= 0xfffffe00 ;
//     addXVR_Reg0x2e |= 0x100 ;
//     addXVR_Reg0x26 &= 0xe000ffff;
//     addXVR_Reg0x26 |= 0x10200000;
//     TRX_RF_SETUP = 0x0F;

//     uart_printf("in 2.4 mode1===============\r\n");

//     xvr_reg_initial_24();
//     TRX_CONFIG |= 0x02; // 射频模块上电
//     TRX_CE = 0x01;     // 使能射频芯片
//     TRX_CMD = 0xA0;    // 清空发送FIFO
    
//     uart_printf("--------------now show all registers in old_init-------------------\r\n");
//     printf_all_registers();
//     uart_printf("--------------now end all registers in old_init-------------------\r\n");
//     uart_printf("------------------------------------------------------------------\r\n");
//     uart_printf("-------------------------------------------------------------------\r\n");
// }


// extern volatile uint32_t XVR_ANALOG_REG_BAK[32];
// extern  uint32_t XVR_REG24_BAK = 0;
// static void rf_ouput_power_value_set(uint8_t power_level)
// {
//     uint32_t regv_0x24;
//     XVR_ANALOG_REG_BAK[4] |=1<<29;
//     addXVR_Reg0x4 = XVR_ANALOG_REG_BAK[4];
//     regv_0x24=addXVR_Reg0x24&0xFFFFF87F;
//     regv_0x24=regv_0x24|(power_level<<7);
//     addXVR_Reg0x24 = regv_0x24;
//     XVR_REG24_BAK = addXVR_Reg0x24;
// }

// __IO uint32_t SysTick_Value_ms;
// uint32_t get_system_time_ms(void)
// {
//     return SysTick_Value_ms;
// }
// RF_HandleTypeDef hrf;
// void rf_simple_init(void)
// {

//     HAL_RF_Init(&hrf,&Init_S_normal);
//     HAL_RF_TimeManager_register(&hrf, get_system_time_ms);
//     rf_ouput_power_value_set(0);

//     uart_printf("--------------now show all registers in new_init-------------------\r\n");
//     printf_all_registers();
//     uart_printf("--------------now end all registers in new_init-------------------\r\n");
//     uart_printf("------------------------------------------------------------------\r\n");
//     uart_printf("-------------------------------------------------------------------\r\n");
   
// }

// static void RF_Write_fifo(uint8_t *pBuf, uint8_t bytes)
// {
//     uint8_t i;
//     __HAL_RF_CMD_W_TX_PAYLOAD();
//     for(i=0; i<bytes; i++)
//     {
//         TRX_FIFO = pBuf[i];
//     }
//     __HAL_RF_CMD_NOP();
// }
// static void RF_Write_fifo_Noack(uint8_t *pBuf, uint8_t bytes)
// {
//     uint8_t i;
//     __HAL_RF_CMD_W_TX_PAYLOAD_NOACK();
//     for(i=0; i<bytes; i++)
//     {
//         TRX_FIFO = pBuf[i];
//     }
//     __HAL_RF_CMD_NOP();
// }

// static void RF_Read_fifo(uint8_t *pBuf, uint8_t bytes)
// {
//     uint8_t i;
//     __HAL_RF_CMD_R_RX_PAYLOAD();
//     for(i=0; i<bytes; i++)
//     {
//         pBuf[i] = TRX_FIFO;
//     }
//     __HAL_RF_CMD_NOP();
// }


// void bk24_send_data(void)
// {
//     static uint32_t packet_count = 0;
//     uint8_t rf_fifo_data[32];
//     uint16_t delay;

//     for(uint8_t i = 0; i < 32; i++) 
//     {
//         rf_fifo_data[i] = i;
//     }

//     HAL_RF_SetTxMode(&hrf);

//     while(1)
//     {
//         __HAL_RF_CMD_FLUSH_TXFIFO();      // 清空TX FIFO        
        
//         rf_fifo_data[0] = packet_count & 0xFF;
//         // 写入数据并发送
//         RF_Write_fifo_Noack(rf_fifo_data, 32);

//         // __HAL_RF_CHIP_EN();
//         // Delay_us(15);
//         // __HAL_RF_CHIP_DIS();
//         packet_count++;
//         packt_count_intc ++;
//         //uart_printf("%d %d\n", packt_count_intc, isq_count_txds);

//         // 简单打印，每1000包打印一次
//         delay++;
//         if(delay % 10 == 0) {
//         // uart_printf("send %ld packet\n", packet_count);
//        // uart_printf("counts rxdr: %d txds: %d maxrt: %d\n", isq_count_rxdr, isq_count_txds, isq_count_maxrt);
//         }

//         // 简单延时，控制发送速率
//         Delay_us(10000);
//     }
// }

// // void bk24_send_data(void)
// // {
// //     static uint32_t packet_count = 0;
// //     uint8_t rf_fifo_data[32];
// //     uint16_t delay;

// //     for(uint8_t i = 0; i < 32; i++) 
// //     {
// //         rf_fifo_data[i] = i;
// //     }

// //     HAL_RF_SetTxMode(&hrf);

// //     while(1)
// //     {
// //         __HAL_RF_CMD_FLUSH_TXFIFO();      // 清空TX FIFO        
        
// //         rf_fifo_data[0] = packet_count & 0xFF;
// //         // 写入数据并发送
// //         //RF_Write_fifo_Noack(rf_fifo_data, 8);
// //         HAL_RF_Transmit_ACK(&hrf, rf_fifo_data, 32);

// //         // __HAL_RF_CHIP_EN();
// //         // Delay_us(15);
// //         // __HAL_RF_CHIP_DIS();
// //         packet_count++;
// //         packt_count_intc ++;
// //         //uart_printf("%d %d\n", packt_count_intc, isq_count_txds);

// //         // 简单打印，每1000包打印一次
// //         delay++;
// //         if(delay % 10 == 0) {
// //         // uart_printf("send %ld packet\n", packet_count);
// //        // uart_printf("counts rxdr: %d txds: %d maxrt: %d\n", isq_count_rxdr, isq_count_txds, isq_count_maxrt);
// //         }

// //         // 简单延时，控制发送速率
// //         Delay_us(10000);
// //     }
// // }



// void bk24_send_data_intc(void)
// {
//     static uint32_t packet_count = 0;
//     uint16_t max_timeout;
//     uint8_t rf_fifo_data[32];
//     uint16_t delay;

//     for(uint8_t i = 0; i < 32; i++) 
//     {
//         rf_fifo_data[i] = i;
//     }

//     __HAL_RF_CMD_FLUSH_TXFIFO();
//     __HAL_RF_CMD_FLUSH_RXFIFO();
//     __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_TX_DS_MASK | IRQ_MAX_RT_MASK|IRQ_RX_DR_MASK);
//     HAL_RF_SetTxMode(&hrf);
//     hrf.TxState = TX_IDLE;
        
//     uart_printf("interrupt_status_init: %d\n", __HAL_RF_GET_IRQ_Status());        
//     while(1){
//         // 写入数据并发送
//         // if(hrf.TxState==TX_IDLE ){
//         //     packet_count=0;
//         //     hrf.TxState = TX_BUSY_Tramsmit;
//         //     RF_Write_fifo(rf_fifo_data, 32);
//         // }

//         HAL_RF_Transmit_IT(&hrf, rf_fifo_data, 32);

//         packet_count++;
//         packt_count_intc ++;
//         delay++;
//         //if(delay % 400 == 0) {
//         uart_printf("while1 counts rxdr: %d txds: %d maxrt: %d int_status\n", isq_count_rxdr, isq_count_txds, isq_count_maxrt,__HAL_RF_GET_IRQ_Status());
//         //uart_printf("tx state: %d,timeout_cnt: %d,sys_time: %d\n", hrf.TxState, hrf.TimeManager.Tx_Timeout_cnt, SysTick_Value_ms);
//         //打印当前时间，超时时间设定，开始时间，超时标志位和次数
//         // uart_printf("Tx_start_time: %d, Tx_TimeOut: %d, Tx_TimeOut_flag: %d,cur_time: %d,timeout_cnt: %d\n",
//         //             hrf.TimeManager.Tx_start_time, 
//         //             hrf.TimeManager.Tx_TimeOut, 
//         //             hrf.TimeManager.Tx_TimeOut_flag, 
//         //             SysTick_Value_ms, 
//         //             hrf.TimeManager.Tx_Timeout_cnt);
        
//         //uart_printf("interrupt_status: %d\n", __HAL_RF_GET_IRQ_Status());
//         //}
//         Delay_us(1000);
//         SysTick_Value_ms++;
//     }
// }

// /* 轮询方式接收 */
// void rf_simple_receive()
// {
//     HAL_RF_SetRxMode(&hrf);
//     uint8_t rec[32];
//     uint16_t count_packet; 
//     uint16_t count_printf; 
//     uart_printf("in rf_simple_receive\n");
//     while(1)
//     {
//         uint8_t irq_status = __HAL_RF_GET_IRQ_Status();
//         if (irq_status & IRQ_RX_DR_MASK)
//         {
//             RF_Read_fifo(rec,32);
//             __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK);
//             count_packet++;
//            // uart_printf("recieve %ld packet: ", count_packet);
//             // 按照十进制打印接收到的数据
//             //for(uint8_t i = 0; i < 8; i++)
//             {
//                 uart_printf("%d ", rec[0]);
//             }
//             uart_printf("\n");
//         }
//         else
//         {
//             count_printf++;
//             if(count_printf % 10000 == 0)
//             {
//                 uart_printf("%d\n",irq_status & RX_P_NO );
//                 uart_printf("not received\n");
//             }
//         }
//     }
// }

// /* 中断方式接收 */
// void rf_intc_receive() {
//     HAL_RF_SetRxMode(&hrf);
//     uint16_t count_packet; 
//     uart_printf("in rf_simple_receive\n");
//     while(1){
//         if(hrf.RxBuff_valid==1){
//             uart_printf("counts rxdr: %d\n", isq_count_rxdr);
//             hrf.RxBuff_valid=0;
//             count_packet++;
//             uart_printf("recieve %d packet, length = %d ,packet=", count_packet, __HAL_RF_GET_RX_RPL_WIDTH());
//             for(uint8_t i = 0; i < 32; i++){
//                 uart_printf("%d ", hrf.RxBuff[i]);
//             }
//             uart_printf("\r\n");
//         }
    
//     }
// }
