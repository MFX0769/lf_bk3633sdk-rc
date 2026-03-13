/**
 ****************************************************************************************
 * @file pinmap_v1.h
 * @brief Pin mapping definition for REMOTE-CONTROL hardware v1.0
 * @note Based on BK3633 chip, refer to REMOTE-CONTROL0303.pdf schematic
 ****************************************************************************************
 */
#ifndef _PINMAP_V1_H_
#define _PINMAP_V1_H_

#include "drv_gpio.h"

/* ============================================================================
 * 调试串口模块 (Debug UART Module)
 * 协议：UART0，波特率115200
 * ============================================================================ */
#define UART0_TX        Port_Pin(0, 0)  // P00 - 串口0发送
#define UART0_RX        Port_Pin(0, 1)  // P01 - 串口0接收

/* ============================================================================
 * 按键输入模块 (Keys Input Module)
 * 配置：GPIO输入模式，内部下拉，按下时读到高电平
 * ============================================================================ */
#define KEY1            Port_Pin(1, 6)  // P16 - 按键按下拉高
#define KEY2            Port_Pin(0, 2)  // P02 - 按键按下拉高
#define KEY_SYS_PWR     Port_Pin(3, 7)  // P37 - 系统电源按键(按键按下拉高)
#define SYS_PWR_EN      Port_Pin(3, 3)  // P33 - 系统电源使能信号(拉高系统上电/拉低系统下电)

/* ============================================================================
 * LCD显示屏模块 (LCD Display Module)
 * ============================================================================ */
/* 新板子 */
#define LCD_SPI_SCK     Port_Pin(0, 4)  // P04 - SPI时钟
#define LCD_SPI_MOSI    Port_Pin(0, 5)  // P05 - SPI主机输出
#define LCD_SPI_CS      Port_Pin(0, 6)  // P06 - SPI片选(NSS)
#define LCD_DCX         Port_Pin(0, 7)  // P07 - 数据/命令选择
#define LCD_RST         Port_Pin(0, 3)  // P03 - LCD复位引脚

//旧板子
// #define LCD_SPI_SCK     Port_Pin(0, 4)  // P04 - SPI时钟
// #define LCD_SPI_MOSI    Port_Pin(0, 5)  // P05 - SPI主机输出
// #define LCD_SPI_CS      Port_Pin(0, 7)  // P07 - SPI片选(NSS)
// #define LCD_DCX         Port_Pin(1, 7)  // P17 - 数据/命令选择
// #define LCD_RST         Port_Pin(0, 3)  // P03 - LCD复位引脚
// #define LCD_KEY1_GPIO_PIN Port_Pin(1, 6)  // P16 - LCD按键1

/* ============================================================================
 * 霍尔传感器模块 (Hall Sensor Module)
 * ============================================================================ */
#define HALL_PWR_EN       Port_Pin(1, 0)  // P10
#define HALL_ADC_OUT      Port_Pin(3, 1)  // P31 (ADC Channel 1)

/* ============================================================================
 * 电池电压检测模块 (Battery Voltage Monitor Module)
 * 配置：ADC输入，通过分压电阻检测电池电压
 * ============================================================================ */
#define ADC_VBAT        Port_Pin(3, 2)  // P32 - 电池电压检测(ADC Channel 2)


/* ============================================================================
 * 硬件版本信息
 * ============================================================================ */
#define HARDWARE_VERSION_MAJOR  1
#define HARDWARE_VERSION_MINOR  0
#define HARDWARE_VERSION_STRING "v1.0"
#define HARDWARE_PCB_NAME       "REMOTE-CONTROL0303"

#endif // _PINMAP_V1_H_
