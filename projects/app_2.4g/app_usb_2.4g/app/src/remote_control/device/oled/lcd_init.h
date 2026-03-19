#ifndef _LCD_INIT_H_
#define _LCD_INIT_H_

#include "spi.h"
#include "drv_gpio.h"
#include "pinmap_v1.h"


/* 定义液晶分辨率 */
#define USE_HORIZONTAL 0// 设置竖屏显示 0 正向显示  1旋转180度显示

#define LCD_W 120
#define LCD_H 240


/* 定义管脚端口 旧板子*/
// #define LCD_RES_GPIO_PIN  Port_Pin(0, 3)
// #define LCD_SCK_GPIO_PIN  Port_Pin(0, 4)
// #define LCD_MOSI_GPIO_PIN Port_Pin(0, 5)
// #define LCD_CS_GPIO_PIN   Port_Pin(0, 7)
// #define LCD_DC_GPIO_PIN   Port_Pin(1, 7)
// #define LCD_KEY1_GPIO_PIN Port_Pin(1, 6)


#define LCD_RES_GPIO_PIN  LCD_RST
#define LCD_SCK_GPIO_PIN  LCD_SPI_SCK
#define LCD_MOSI_GPIO_PIN LCD_SPI_MOSI
#define LCD_CS_GPIO_PIN   LCD_SPI_CS
#define LCD_DC_GPIO_PIN   LCD_DCX
#define LCD_KEY1_GPIO_PIN Port_Pin(1, 6)


/* 定义端口电平状态 */
#define LCD_SCK_Clr() gpio_set(LCD_SCK_GPIO_PIN, 0)
#define LCD_SCK_Set() gpio_set(LCD_SCK_GPIO_PIN, 1)

#define LCD_MOSI_Clr() gpio_set(LCD_MOSI_GPIO_PIN, 0)
#define LCD_MOSI_Set() gpio_set(LCD_MOSI_GPIO_PIN, 1)

#define LCD_RES_Clr() gpio_set(LCD_RES_GPIO_PIN, 0)
#define LCD_RES_Set() gpio_set(LCD_RES_GPIO_PIN, 1)

#define LCD_DC_Clr() gpio_set(LCD_DC_GPIO_PIN, 0)
#define LCD_DC_Set() gpio_set(LCD_DC_GPIO_PIN, 1)

#define LCD_CS_Clr() gpio_set(LCD_CS_GPIO_PIN, 0)
#define LCD_CS_Set() gpio_set(LCD_CS_GPIO_PIN, 1)

#define LCD_KEY1_Clr() gpio_set(LCD_KEY1_GPIO_PIN, 0)
#define LCD_KEY1_Set() gpio_set(LCD_KEY1_GPIO_PIN, 1)

/* 函数说明 */
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);          // 设置坐标函数
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color); // 填充函数
void LCD_Init(void);        
void LCD_WR_Bus(uint8_t dat);                                                       // 设置LCD初始化
void LCD_WR_Bus(uint8_t dat);
void LCD_WR_REG(uint8_t reg);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void OLED_Init(void);
void LCD_WR_Bus_16(uint16_t dat);
void display_image(uint8_t* dat);
void LCD_WR_Bus_16_Part(uint8_t* dat);
// void LCD_WR_DATA_Part(uint16_t dat);
void LCD_WR_DATA_Part(uint8_t* dat, uint16_t size);
void LCD_WR_Bus_dma(uint8_t* dat, uint16_t size);
void LCD_WR_DATA_dma(uint8_t* dat, uint16_t size);
void LCD_WR_Bus_batch(uint8_t* dat, uint16_t size);



/* 定义画笔颜色 */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
 #define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40      // 棕色
#define BRRED 0XFC07      // 棕红色
#define GRAY 0X8430       // 灰色
#define DARKBLUE 0X01CF   // 深蓝色
#define LIGHTBLUE 0X7D7C  // 浅蓝色
#define GRAYBLUE 0X5458   // 灰蓝色
#define LIGHTGREEN 0X841F // 浅绿色
#define LGRAY 0XC618      // 浅灰色(PANNEL),窗体背景色
#define LBBLUE 0X2B12     // 浅棕蓝色(选择条目的反色)
#define LGRAYBLUE 0XA651  // 浅灰蓝色(中间层颜色)
#define MBLUE  0x72FF
#define MYELLOW 0xf5e4

#endif
