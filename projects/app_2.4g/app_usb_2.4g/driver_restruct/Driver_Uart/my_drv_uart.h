/**
 ****************************************************************************************
 *
 * @file my_drv_uart.h
 *
 * @brief UART0 Driver
 *
 ****************************************************************************************
 */

#ifndef _MY_DRV_UART_H_
#define _MY_DRV_UART_H_

#include <stdbool.h>
#include <stdint.h>
#include "user_config.h"
#include "my_queue.h" 

#define BASEADDR_UART                                      0x00806300

// UART0 Register Definitions
#define UART_REG0X0                                        *((volatile unsigned long *) (BASEADDR_UART+0x0*4))
#define POS_UART_REG0X0_TX_ENABLE                          0
#define POS_UART_REG0X0_RX_ENABLE                          1
#define POS_UART_REG0X0_IRDA                               2
#define POS_UART_REG0X0_LEN                                3
#define POS_UART_REG0X0_PAR_EN                             5
#define POS_UART_REG0X0_PAR_MODE                           6
#define POS_UART_REG0X0_STOP_LEN                           7
#define POS_UART_REG0X0_CLK_DIVID                          8

#define UART_REG0X1                                         *((volatile unsigned long *) (BASEADDR_UART+0x1*4))
#define POS_UART_REG0X1_TX_FIFO_THRESHOLD                   0
#define POS_UART_REG0X1_RX_FIFO_THRESHOLD                   8
#define POS_UART_REG0X1_RX_STOP_DETECT_TIME                 16

#define UART_REG0X2                                         *((volatile unsigned long *) (BASEADDR_UART+0x2*4))
#define POS_UART_REG0X2_TX_FIFO_COUNT                       0
#define POS_UART_REG0X2_RX_FIFO_COUNT                       8
#define POS_UART_REG0X2_TX_FIFO_FULL                        16
#define POS_UART_REG0X2_TX_FIFO_EMPTY                       17
#define POS_UART_REG0X2_RX_FIFO_FULL                        18
#define POS_UART_REG0X2_RX_FIFO_EMPTY                       19
#define POS_UART_REG0X2_FIFO_WR_READY                       20
#define POS_UART_REG0X2_FIFO_RD_READY                       21

#define UART_REG0X3                                         *((volatile unsigned long *) (BASEADDR_UART+0x3*4))
#define POS_UART_REG0X3_UART_TX_FIFO_DIN                    0
#define POS_UART_REG0X3_UART_RX_FIFO_DOUT                   8

#define UART_REG0X4                                         *((volatile unsigned long *) (BASEADDR_UART+0x4*4))
#define POS_UART_REG0X4_TX_FIFO_NEED_WRITE_MASK             0
#define POS_UART_REG0X4_RX_FIFO_NEED_READ_MASK              1
#define POS_UART_REG0X4_RX_FIFO_OVER_FLOW_MASK              2
#define POS_UART_REG0X4_UART_RX_PARITY_ERR_MASK             3
#define POS_UART_REG0X4_UART_RX_STOP_ERR_MASK               4
#define POS_UART_REG0X4_UART_TX_STOP_END_MASK               5
#define POS_UART_REG0X4_UART_RX_STOP_END_MASK               6
#define POS_UART_REG0X4_UART_RXD_WAKEUP_MASK                7

#define UART_REG0X5                                         *((volatile unsigned long *) (BASEADDR_UART+0x5*4))
#define POS_UART_REG0X5_TX_FIFO_NEED_WRITE                  0
#define POS_UART_REG0X5_RX_FIFO_NEED_READ                   1
#define POS_UART_REG0X5_RX_FIFO_OVER_FLOW                   2
#define POS_UART_REG0X5_UART_RX_PARITY_ERROR                3
#define POS_UART_REG0X5_UART_RX_STOP_ERROR                  4
#define POS_UART_REG0X5_UART_TX_STOP_END                    5
#define POS_UART_REG0X5_UART_RX_STOP_END                    6
#define POS_UART_REG0X5_UART_RXD_WAKEUP                     7

#define UART_REG0X6                                         *((volatile unsigned long *) (BASEADDR_UART+0x6*4))
#define POS_UART_REG0X6_FLOW_CTL_LOW_CNT                    0
#define POS_UART_REG0X6_FLOW_CTL_HIGH_CNT                   8
#define POS_UART_REG0X6_FLOW_CONTROL_ENA                    16
#define POS_UART_REG0X6_RTS_POLARITY_SEL                    17
#define POS_UART_REG0X6_CTS_POLARITY_SEL                    18

#define UART_REG0X7                                         *((volatile unsigned long *) (BASEADDR_UART+0x7*4))
#define POS_UART_REG0X7_UART_WAKE_COUNT                     0
#define POS_UART_REG0X7_UART_TXD_WAIT_CNT                   10
#define POS_UART_REG0X7_UART_RXD_WAKE_EN                    20
#define POS_UART_REG0X7_UART_TXD_WAKE_EN                    21
#define POS_UART_REG0X7_RXD_NEGEDGE_WAKE_EN                 22

// UART0 Buffer Size
#define UART_FIFO_MAX_COUNT  256

// Export Queues
extern my_queue_t uart0_rxQueue; // UART0接收队列
extern my_queue_t uart0_txQueue; // UART0发送队列

// Function Prototypes
void uart_init(uint32_t baudrate);
void uart_isr(void);
void uart_send(void *buff, uint16_t len);
int  uart0_printf(const char *fmt,...);

#endif /* _MY_DRV_UART_H_ */