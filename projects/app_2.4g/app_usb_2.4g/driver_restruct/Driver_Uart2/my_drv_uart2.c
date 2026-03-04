#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "my_drv_uart2.h"
#include "user_config.h"
#include "icu.h"
#include "BK3633_RegList.h"
#include "my_queue.h"
#include "drv_gpio.h"




uint8_t uart_rxQueue_buffer[UART2_FIFO_MAX_COUNT]; //UART2接收队列缓冲区
uint8_t uart_txQueue_buffer[UART2_FIFO_MAX_COUNT]; //UART2发送队列缓冲区
my_queue_t uart2_rxQueue; //UART2接收队列
my_queue_t uart2_txQueue; //UART2发送队列

volatile uint8_t  uart2_rx_done = 0;
volatile uint32_t uart2_rx_index = 0;

uint8_t uart2_rx_buf[UART2_FIFO_MAX_COUNT];
uint8_t uart2_tx_buf[UART2_FIFO_MAX_COUNT];


//==============================UART_INITIAL===================
void uart2_init(uint32_t baudrate)
{
    uint32_t    uart_clk_div;
    
    SET_UART2_POWER_UP ;   
    uart_clk_div = 16000000/baudrate -1 ;
    
    // gpio_config(0x16,SC_FUN,PULL_HIGH);
    // gpio_config(0x17,SC_FUN,PULL_HIGH);
    gpio_config(Port_Pin(1,6),GPIO_SC_FUN,GPIO_PULL_HIGH);
    gpio_config(Port_Pin(1,7),GPIO_SC_FUN,GPIO_PULL_HIGH);
    
    UART2_REG0X0 =  (uart_clk_div << POS_UART2_REG0X0_CLK_DIVID) |
                    (0x0          << POS_UART2_REG0X0_STOP_LEN ) |
                    (0x0          << POS_UART2_REG0X0_PAR_MODE ) |
                    (0x0          << POS_UART2_REG0X0_PAR_EN   ) |
                    (0x3          << POS_UART2_REG0X0_LEN      ) |
                    (0x0          << POS_UART2_REG0X0_IRDA     ) |
                    (0x1          << POS_UART2_REG0X0_RX_ENABLE) |
                    (0x1          << POS_UART2_REG0X0_TX_ENABLE) ;
    
    // 修改这里：设置32字节接收阈值，32位超时时间
    UART2_REG0X1 = 0x00002020;  // RX_TH=32, TX_TH=32, STOP_DET=0
    
    UART2_REG0X4 = 0x42;  // 中断使能配置
    UART2_REG0X6 = 0x0;   // 流控配置
    UART2_REG0X7 = 0x0;   // 唤醒配置

    SYS_REG0X10_INT_EN |= (0x01 << POS_SYS_REG0X10_INT_EN_UART2);

    uart2_rx_done = 0;
    uart2_rx_index = 0;

    queue_init(&uart2_rxQueue, uart_rxQueue_buffer, UART2_FIFO_MAX_COUNT,UART2_FIFO_MAX_COUNT, sizeof(uint8_t));
    queue_init(&uart2_txQueue, uart_txQueue_buffer, UART2_FIFO_MAX_COUNT, UART2_FIFO_MAX_COUNT, sizeof(uint8_t));

}

static void uart2_send_byte(unsigned char data)
{
    while( 0==(UART2_REG0X2&(1<<POS_UART2_REG0X2_FIFO_WR_READY)) );
    UART2_REG0X3 = data;
}

void uart2_send(void *buff, int len)
{
    uint8_t *tmpbuf = (uint8_t *)buff;
    while (len--)
        uart2_send_byte(*tmpbuf++);
}

void clear_uart2_buffer(void)
{
    uart2_rx_index = 0;
    uart2_rx_done = 0;
    memset(uart2_rx_buf, 0, sizeof(uart2_rx_buf)); /**< Clear the RX buffer */
    memset(uart2_tx_buf, 0, sizeof(uart2_tx_buf)); /**< Clear the TX buffer */
}

 
int uart2_printf(const char *fmt,...)
{
#if (UART_PRINTF_ENABLE)    
    va_list ap;
    int n;
    char send_buf[128];
    va_start(ap, fmt);
    n=vsprintf(send_buf, fmt, ap);
    va_end(ap);
    uart2_send(send_buf, n);
    return n;
#else
    return 0;
#endif
}   

int uart2_printf_null(const char *fmt,...)
{
    return 0;

}
//==========================UART_INTERRUPT==========================
// void uart2_isr(void)
// {
//     unsigned long irq_status;
//     uint8_t rx_byte;

//     irq_status = UART2_REG0X5;

//     // 检查是否有 "FIFO需读" 中断 或 "接收超时" 中断
//     if (irq_status & ((1 << POS_UART2_REG0X5_RX_FIFO_NEED_READ) | (1 << POS_UART2_REG0X5_UART_RX_STOP_END)))
//     {    
//         // 只要硬件FIFO里有数据，就循环读取
//         while(UART2_REG0X2 & (1 << POS_UART2_REG0X2_FIFO_RD_READY))
//         {
//             // 读取数据 (高8位有效)
//             rx_byte = (uint8_t)(UART2_REG0X3 >> 8);
//             queue_push_overwrite(&uart2_rxQueue, &rx_byte);
//         }
//     }

//     // 清除中断标志
//     UART2_REG0X5 = irq_status;
// }


void uart2_isr(void)
{
    //uart_printf("uart2_isr\r\n");
    
    uint32_t irq_status = UART2_REG0X5;
    uint8_t rx_byte;
    uint8_t bytes_to_read;

    // 处理阈值中断（32字节触发）
    if (irq_status & (1 << POS_UART2_REG0X5_RX_FIFO_NEED_READ))
    {
        // 阈值中断触发，表示FIFO中有至少32字节数据
        // 但我们只读取32字节，保持分块处理
        bytes_to_read = 32;  // 每次阈值中断固定读取32字节
        
        for (uint8_t i = 0; i < bytes_to_read; i++)
        {
            if (!(UART2_REG0X2 & (1 << POS_UART2_REG0X2_FIFO_RD_READY))) {
                break;  // FIFO意外变空，退出循环
            }
            
            // 读取一个字节
            rx_byte = (uint8_t)(UART2_REG0X3 >> 8);
            // push到环形FIFO
            queue_push_overwrite(&uart2_rxQueue, &rx_byte);

        }
        // 对于阈值中断，读取数据后会自动清除，无需软件操作
        //uart_printf("32B\r\n");
    }
    
    //处理空闲中断
    if (irq_status & (1 << POS_UART2_REG0X5_UART_RX_STOP_END))
    {
        // 读取FIFO中剩余的所有数据
        // uint8_t bytes_in_fifo = 0;
        
        // //读取所有剩余数据
        while (UART2_REG0X2 & (1 << POS_UART2_REG0X2_FIFO_RD_READY))
        {
            rx_byte = (uint8_t)(UART2_REG0X3 >> 8);
            queue_push_overwrite(&uart2_rxQueue, &rx_byte);
        }

        //下面这种方式会丢包！！！！
        // uint8_t bytes_in_fifo = (UART2_REG0X2 >> 8) & 0xFF;
        // for (uint8_t i = 0; i < bytes_in_fifo; i++)
        // {
        //     // 检查FIFO是否还有数据可读
        //     if (UART2_REG0X2 & (1 << POS_UART2_REG0X2_FIFO_RD_READY)){
        //         rx_byte = (uint8_t)(UART2_REG0X3 >> 8);
        //         // push到环形FIFO
        //         queue_push_overwrite(&uart2_rxQueue, &rx_byte);
        //     }else{
        //         // FIFO提前空了，跳出循环
        //         break;
        //     }
        // }

        // 清除空闲中断标志（需要写1清零）  1 << POS_UART2_REG0X5_UART_RX_STOP_END是1左移6位
        UART2_REG0X5 |= (1 << POS_UART2_REG0X5_UART_RX_STOP_END);
        //uart_printf("IDLE\r\n");
    }
}
