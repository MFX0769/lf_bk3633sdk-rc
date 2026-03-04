/**
 ****************************************************************************************
 *
 * @file uart.c
 *
 * @brief UART0 Driver (Refactored)
 *
 ****************************************************************************************
 */
#include <stddef.h>     
#include <stdarg.h>
#include <stdint.h>        
#include <string.h>        
#include <stdio.h>
#include "my_drv_uart.h"       
#include "drv_gpio.h"
#include "user_config.h"
#include "my_queue.h"
#include "icu.h"

#include "rwip.h" 
// #include "gpio.h"
#include "drv_gpio.h"
#include "user_config.h"
#include "dma.h"



// 缓冲区定义
uint8_t uart0_rxQueue_buffer[UART_FIFO_MAX_COUNT]; 
uint8_t uart0_txQueue_buffer[UART_FIFO_MAX_COUNT]; 

// 队列控制块
my_queue_t uart0_rxQueue; 
my_queue_t uart0_txQueue; 

// 临时缓冲
uint8_t uart0_rx_buf[UART_FIFO_MAX_COUNT];

//==============================UART INITIAL===================
void uart_init(uint32_t baudrate)
{
    uint32_t uart_clk_div;
    
    SET_UART_POWER_UP;   // Open Peripheral Clock

    // Baudrate Calculation
    uart_clk_div = 16000000/baudrate - 1;
    
    // GPIO Config for UART0 (P0.0=TX, P0.1=RX)
    gpio_config(Port_Pin(0,0), GPIO_SC_FUN, GPIO_PULL_HIGH);
    gpio_config(Port_Pin(0,1), GPIO_SC_FUN, GPIO_PULL_HIGH);
    
    // UART0 Control Register Configuration
    UART_REG0X0 = (uart_clk_div << POS_UART_REG0X0_CLK_DIVID) |
                  (0x0          << POS_UART_REG0X0_STOP_LEN ) |
                  (0x0          << POS_UART_REG0X0_PAR_MODE ) |
                  (0x0          << POS_UART_REG0X0_PAR_EN   ) |
                  (0x3          << POS_UART_REG0X0_LEN      ) | // 8-bit data
                  (0x0          << POS_UART_REG0X0_IRDA     ) |
                  (0x1          << POS_UART_REG0X0_RX_ENABLE) |
                  (0x1          << POS_UART_REG0X0_TX_ENABLE) ;
    
    // FIFO Threshold Config: RX_TH=32, TX_TH=32
    UART_REG0X1 = 0x00002020; 
    
    UART_REG0X4 = 0x42;  // Interrupt Enable: RX_FIFO_NEED_READ | UART_RX_STOP_END
    UART_REG0X6 = 0x0;
    UART_REG0X7 = 0x0;

    // Enable UART0 Interrupt in ICU
    SYS_REG0X10_INT_EN |= (0x01 << POS_SYS_REG0X10_INT_EN_UART);

    // Initialize Queues
    queue_init(&uart0_rxQueue, uart0_rxQueue_buffer, UART_FIFO_MAX_COUNT, UART_FIFO_MAX_COUNT, sizeof(uint8_t));
    queue_init(&uart0_txQueue, uart0_txQueue_buffer, UART_FIFO_MAX_COUNT, UART_FIFO_MAX_COUNT, sizeof(uint8_t));
}

//==============================SEND FUNCTIONS===================
static void uart_send_byte(uint8_t data)
{
    // Wait for FIFO Write Ready
    while( 0 == (UART_REG0X2 & (1 << POS_UART_REG0X2_FIFO_WR_READY)) );
    UART_REG0X3 = data;
}

void uart_send(void *buff, uint16_t len)
{
    uint8_t *tmpbuf = (uint8_t *)buff;
    while (len--)
        uart_send_byte(*tmpbuf++);
}

int uart0_printf(const char *fmt,...)
{
    va_list ap;
    int n;
    char send_buf[128];
    va_start(ap, fmt);
    n = vsprintf(send_buf, fmt, ap);
    va_end(ap);
    uart_send(send_buf, n);
    // Optional: Wait for TX FIFO Empty if blocking behavior is desired
    // while(0 == (UART_REG0X2 & (1 << POS_UART_REG0X2_TX_FIFO_EMPTY)));
    return n;
}

//==============================INTERRUPT HANDLER===================
void uart_isr(void)
{
    uint32_t irq_status = UART_REG0X5;
    uint8_t rx_byte;
    uint8_t bytes_to_read;

    //处理阈值中断 (RX FIFO >= Threshold)
    if (irq_status & (1 << POS_UART_REG0X5_RX_FIFO_NEED_READ))
    {
        // 阈值设为32，所以固定读32字节（或者读到空）
        // 为了安全，建议检查 FIFO_RD_READY
        bytes_to_read = 32; 
        
        for (uint8_t i = 0; i < bytes_to_read; i++)
        {
            if (!(UART_REG0X2 & (1 << POS_UART_REG0X2_FIFO_RD_READY))) {
                break; // FIFO Empty protection
            }
            
            rx_byte = (uint8_t)(UART_REG0X3 >> 8);
            queue_push_overwrite(&uart0_rxQueue, &rx_byte);
        }
        // 阈值中断通常在读取足够数据后自动清除，或通过写状态寄存器清除
    }
    
    //处理空闲中断 (RX Stop End / Timeout)
    if (irq_status & (1 << POS_UART_REG0X5_UART_RX_STOP_END))
    {

        // 读取 FIFO 中所有剩余数据
        while (UART_REG0X2 & (1 << POS_UART_REG0X2_FIFO_RD_READY))
        {
            rx_byte = (uint8_t)(UART_REG0X3 >> 8);
            queue_push_overwrite(&uart0_rxQueue, &rx_byte);
        }

        // 必须手动清除空闲中断标志
        UART_REG0X5 |= (1 << POS_UART_REG0X5_UART_RX_STOP_END);
    }
    
    // UART_REG0X5 = irq_status; 
}


#define UART1_DMA_W_CHANNEL 1
#define UART1_DMA_R_CHANNEL 2

#define UART_FIFO_WR_ADDR   0x0080630C
void uart1_dma_isr_callback(uint8_t channel)
{
    if(channel==UART1_DMA_W_CHANNEL)//dma write isr
    {
         uart_printf("%s write \r\n", __func__);

    }else if(channel==UART1_DMA_R_CHANNEL) //dma read isr
    {
         uart_printf("%s read \r\n", __func__);
    }
}

void uart1_dma_write(uint8_t *buffer,uint16_t buffer_len)
{ 
    uart_printf("%s \r\n", __func__);

    dma_config_write(UART1_DMA_W_CHANNEL,UART1_REQ,(uint32_t)buffer,UART_FIFO_WR_ADDR,buffer_len,DMA_DW_8B);        
}

void uart1_dma_read(uint8_t *buffer,uint16_t buffer_len)
{
    uart_printf("%s \r\n", __func__);

    dma_config_read(UART1_DMA_R_CHANNEL,UART1_REQ,UART_FIFO_WR_ADDR,(uint32_t)buffer,buffer_len,DMA_DW_8B);
}


