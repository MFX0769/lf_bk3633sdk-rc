/*
 * lcd_kmg.c
 * LCD 上层绘制/UI 层：
 * - 文本/数字绘制与图片显示封装
 * - Kamingo UI 组件（速度、电量、骑行模式、总里程）
 * - 绘制节奏控制（分步延时）
 */
#include "lcd_kmg.h"
#include "lcdfont_controller.h"
#include <string.h>
#include "lcd_init.h"
#include "pic.h"
#include "one_50x50_lcd.h"
#include "user_config.h"
#include "oled_config.h"
#include "datatypes.h"

#define KMS_UNIT       "km "
#define SPEED_UNIT     "km/h"
extern void Delay_ms(int num);
extern void Delay_us(int num);

/**
 * @brief UI 绘制的步进延时（毫秒）
 * @note 默认值保持与旧实现相近的节奏
 */
static uint16_t s_lcd_ui_step_delay_ms = 1;

/**
 * @brief 设置每个 UI 绘制步骤之间的延时
 * @param ms 延时毫秒
 */
void lcd_ui_set_step_delay_ms(uint16_t ms)
{
    s_lcd_ui_step_delay_ms = ms;
}

/**
 * @brief 获取当前 UI 步进延时
 * @return 延时毫秒
 */
uint16_t lcd_ui_get_step_delay_ms(void)
{
    return s_lcd_ui_step_delay_ms;
}

/**
 * @brief 根据配置执行一次步进延时
 */
static void lcd_ui_step_delay(void)
{
    if (s_lcd_ui_step_delay_ms)
        Delay_ms((int)s_lcd_ui_step_delay_ms);
}

// 字节位反转工具（bit7<->bit0），用于特定像素/字库格式
/**
 * @brief 字节位反转（bit7<->bit0）
 * @param b 输入字节
 * @return 反转后的字节
 */
uint8_t reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}


// 简易整数转字符串（十进制）
/**
 * @brief 整数转字符串（十进制）
 * @param num 输入整数
 * @param str 输出字符串缓冲区
 */
void int_to_str(int num, char *str) {
    int i = 0;
    if (num == 0) {
        str[i++] = '0';
    } else {
        int is_negative = 0;
        if (num < 0) {
            is_negative = 1;
            num = -num;
        }
        while (num > 0) {
            str[i++] = (num % 10) + '0';
            num /= 10;
        }
        if (is_negative) {
            str[i++] = '-';
        }
        // 反转字符串
        for (int j = 0; j < i / 2; j++) {
            char temp = str[j];
            str[j] = str[i - j - 1];
            str[i - j - 1] = temp;
        }
    }
    str[i] = '\0';
}


// 拆分两位数字（用于数码管样式显示）
/**
 * @brief 拆分两位数字（个位/十位）
 * @param num 输入 0~99
 * @param arr 输出数组 [个位, 十位]
 */
static void split_digits_2(uint8_t num, int8_t arr[2]) {
    if (num > 99) {
        // 超出范围，按需处理。这里默认清零。
        arr[0] = 0;
        arr[1] = 0;
        return;
    }

    arr[0] = num % 10;   // 个位
    arr[1] = num / 10;   // 十位
}


// 输入一个 0~999 的数，将其各位拆分后放入 arr[3]
// 拆分三位数字（用于数码管样式显示）
/**
 * @brief 拆分三位数字（个位/十位/百位）
 * @param num 输入 0~999
 * @param arr 输出数组 [个位, 十位, 百位]
 */
static void split_digits_3(uint16_t num, int8_t arr[3]) {
    if (num > 999) {
        // 超出范围，按需处理。这里默认清零。
        arr[0] = 0;
        arr[1] = 0;
        arr[2] = 0;
        return;
    }

    arr[0] = num % 10;         // 个位
    arr[1] = (num / 10) % 10;  // 十位
    arr[2] = num / 100;        // 百位
}



/**
 * @brief       显示单个字符
 * @param       x:字符显示位置列起始坐标
 * @param       y:字符显示位置行起始坐标
 * @param       num:显示字符的ASCII码
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
// 绘制单字符（常规坐标系，使用点阵字库）
/**
 * @brief 绘制单字符（常规坐标系）
 * @param x 起始列
 * @param y 起始行
 * @param num 字符 ASCII 码
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t temp, sizex, t, m = 0;
    uint16_t i, TypefaceNum; // 一个字符所占字节大小
    sizex = sizey / 2;
    TypefaceNum = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
    num = num - ' ';                                     // 得到偏移后的值
    LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1); // 设置显示窗口
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 16)
            ;// temp = ascii_1608[num][i]; // 调用8x16字体
        else 
        if (sizey == 24)
            ;// temp = ascii_2412[num][i]; // 调用12x24字体
        else if (sizey == 32)
           ; // temp = ascii_3216[num][i]; // 调用16x32字体
        else
            return;
        for (t = 0; t < 8; t++)
        {
            if (temp & (0x01 << t))
                // LCD_WR_DATA(reverse_bits(fc));
                LCD_WR_DATA((fc));
            else
                LCD_WR_DATA((bc));
            m++;
            if (m % sizex == 0)
            {
                m = 0;
                break;
            }
        }
    }
}

/**
 * @brief       显示单个字符 
 * @param       x:字符显示位置列起始坐标
 * @param       y:字符显示位置行起始坐标
 * @param       num:显示字符的ASCII码
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
// 绘制单字符（横屏坐标系，y 轴向上）
/**
 * @brief 绘制单字符（横屏坐标系，y 轴向上）
 * @param x 起始列
 * @param y 起始行
 * @param ch 字符 ASCII 码
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowChar_Hor(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    ch = ch - ' ';
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;

    // 设置绘图区域为字符显示矩形
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长

    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 32)
        {
            temp = ascii_32[ch][i]; // 示例：12x24字体
        }
        else
        {
            return;
        }

        for (j = 0; j < 8; j++)
        {
           
                 
            if (temp & (1 << j))
            {
                // uart_printf("*");
                LCD_WR_DATA(fc);
            }
            else
            {
                // uart_printf(" ");
                LCD_WR_DATA(bc);
            }
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                break; // 每列sizex个像素（对应一行）
            }
        }
    }
}


/**
 * @brief       显示单个字符
 * @param       x:字符显示位置列起始坐标
 * @param       y:字符显示位置行起始坐标
 * @param       num:显示字符的ASCII码
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
/**
 * @brief 横屏下绘制空白字符（用于清除区域）
 * @param x 起始列
 * @param y 起始行
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_Show_Space_Hor(uint16_t x, uint16_t y, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;

    // 设置绘图区域为字符显示矩形
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长

    for (i = 0; i < TypefaceNum; i++)
    {
        for (j = 0; j < 8; j++)
        {

            
            LCD_WR_DATA(bc);
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                break; // 每列sizex个像素（对应一行）
            }
        }
    }
}




// 绘制数字字符（横屏坐标系，数码字体）
/**
 * @brief 绘制数字字符（横屏数码字体）
 * @param x 起始列
 * @param y 起始行
 * @param ch 数字字符（'0'~'9'）
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowChar_Digital_Hor(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    ch = ch - '0';
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;

    // 设置绘图区域为字符显示矩形
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长

    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 32)
        {
            temp = digital_32_hor[ch][i]; // 示例：12x24字体
        }
        else if (sizey == 36)
        {
            temp = digital_36_hor[ch][i]; // 示例：12x24字体
        }
        else if (sizey == 56)
        {
            temp = digital_56_hor[ch][i];
        }
        else
        {
            return;
        }

        for (j = 0; j < 8; j++)
        {
           
                 
            if (temp & (1 << j))
            {
                // uart_printf("*");
                LCD_WR_DATA(fc);
            }
            else
            {
                // uart_printf(" ");
                LCD_WR_DATA(bc);
            }
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                break; // 每列sizex个像素（对应一行）
            }
        }
    }
}


/**
 * @brief       显示单个字符
 * @param       x:字符显示位置列起始坐标
 * @param       y:字符显示位置行起始坐标
 * @param       num:显示字符的ASCII码
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
/**
 * @brief 横屏下绘制空白字符（DMA/批量方式）
 * @param x 起始列
 * @param y 起始行
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_Show_Space_Hor_dma(uint16_t x, uint16_t y, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;
#if !USE_DMA
    uint8_t dma_buffer[SPI_BATCH_N];
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长
#else
    uint32_t data_size = TypefaceNum * 8;
    uint8_t dma_buffer[data_size];
#endif
    // 设置绘图区域为字符显示矩形
    uint16_t dma_index = 0;
    for (i = 0; i < TypefaceNum; i++)
    {
        for (j = 0; j < 8; j++)
        {

            dma_buffer[dma_index ++] = bc >> 8;
            dma_buffer[dma_index ++] = bc & 0xFF;
            if (dma_index >= SPI_BATCH_N) {
                // LCD_DC_Set();
                LCD_WR_Bus_batch(&(dma_buffer[0]), dma_index);
                // LCD_DC_Set();
                dma_index = 0;
            }
            // LCD_WR_DATA(bc);
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                break; // 每列sizex个像素（对应一行）
            }
        }
    }


    
    
    // LCD_WR_DATA_dma(&(dma_buffer[0]), dma_index);

    
#if USE_DMA
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长
    LCD_TX_Begin(LCD_TX_DATA);
    LCD_TX_Write_DMA_Async(&(dma_buffer[0]), dma_index, NULL);
    LCD_TX_End();      // CS deassert happens in the DMA/SPI completion ISR
    LCD_TX_WaitDone(); // prevents dma_buffer reuse races
#else
    // uint16_t batch_n = 32;
    // uint16_t batch_size = dma_index / SPI_BATCH_N;
    // for (i = 0; i < SPI_BATCH_N; i++)
    // {
    //     LCD_WR_Bus_batch(&(dma_buffer[0]) + i * batch_size, batch_size);
    //     dma_index -= batch_size;
    // }
    // if (dma_index > 0)
    // {
    //     LCD_WR_Bus_batch(&(dma_buffer[0]) + i * batch_size, dma_index);
    // }

    LCD_DC_Set();
    if (dma_index > 0)
    {
        LCD_WR_Bus_batch(&(dma_buffer[0]), dma_index);
    }
    LCD_DC_Set();
#endif
  
}

/**
 * @brief 绘制数字字符（横屏数码字体，DMA/批量方式）
 * @param x 起始列
 * @param y 起始行
 * @param ch 数字字符（'0'~'9'）
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowChar_Digital_Hor_dma(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    ch = ch - '0';
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;

    uint32_t dma_index = 0;

    // 设置绘图区域为字符显示矩形
#if !USE_DMA
    uint8_t dma_buffer[SPI_BATCH_N];
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长
#else
    uint32_t data_size = TypefaceNum * 8;
    uint8_t dma_buffer[data_size];
#endif

    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 32)
        {
            temp = digital_32_hor[ch][i]; // 示例：12x24字体
        }
        else if (sizey == 36)
        {
            temp = digital_36_hor[ch][i]; // 示例：12x24字体
        }
        else if (sizey == 56)
        {
            temp = digital_56_hor[ch][i];
        }
        else
        {
            return;
        }

        for (j = 0; j < 8; j++)
        {
            if (temp & (1 << j))
            {
                // uart_printf("*");
                // LCD_WR_DATA(fc);

                dma_buffer[dma_index++] = fc >> 8;
                dma_buffer[dma_index++] = fc & 0xFF;
            }
            else
            {
                // uart_printf(" ");
                // LCD_WR_DATA(bc);

                dma_buffer[dma_index++] = bc >> 8;
                dma_buffer[dma_index++] = bc & 0xFF;
            }

            if (dma_index >= SPI_BATCH_N)
            {
                // LCD_DC_Set();
                LCD_WR_Bus_batch(&(dma_buffer[0]), dma_index);
                // LCD_DC_Set();
                dma_index = 0;
            }
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                k = 0;
                break; // 每列sizex个像素（对应一行）
            }
        }
    }

    // uart_printf("l>%d\r\n", dma_index);
    // LCD_WR_DATA_dma(&(dma_buffer[0]), dma_index);
#if USE_DMA
    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长
    LCD_TX_Begin(LCD_TX_DATA);
    LCD_TX_Write_DMA_Async(&(dma_buffer[0]), dma_index, NULL);
    LCD_TX_End();      // CS deassert happens in the DMA/SPI completion ISR
    LCD_TX_WaitDone(); // prevents dma_buffer reuse races
#else
    LCD_DC_Set();
    // uint16_t batch_n = 64;
    // uint16_t batch_size = dma_index / SPI_BATCH_N;
    // for (i = 0; i < SPI_BATCH_N; i++)
    // {
    //     LCD_WR_Bus_batch(&(dma_buffer[0]) + i * batch_size, batch_size);
    //     dma_index -= batch_size;
    // }
    // if (dma_index > 0)
    // {
    //     LCD_WR_Bus_batch(&(dma_buffer[0]) + i * batch_size, dma_index);
    // }

    if (dma_index > 0)
    {
        LCD_WR_Bus_batch(&(dma_buffer[0]), dma_index);
    }
    LCD_DC_Set();
#endif

    
    // uart_printf("len>%d\r\n", dma_index);
    // spi_dma_write(&(dma_buffer[0]), dma_index, NULL);
    // LCD_DC_Set();
}


/**
 * @brief       显示单个字符 
 * @param       x:字符显示位置列起始坐标
 * @param       y:字符显示位置行起始坐标
 * @param       num:显示字符的ASCII码
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
/**
 * @brief 绘制字符（横屏，DMA/批量方式）
 * @param x 起始列
 * @param y 起始行
 * @param ch 字符 ASCII 码
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowChar_Hor_dma(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t j,temp;
    uint8_t sizex = sizey / 2;
    uint16_t i, k = 0;
    uint16_t TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    ch = ch - ' ';
     // 起始地址向下对齐为4的倍数
    x = x & ~0x03;

    uint32_t data_size = TypefaceNum * 8;
    uint8_t dma_buffer[data_size];
    uint16_t dma_index = 0;

    // 设置绘图区域为字符显示矩形
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 32)
        {
            temp = ascii_32[ch][i]; // 示例：12x24字体
        }
        else
        {
            return;
        }

        for (j = 0; j < 8; j++)
        {

            if (temp & (1 << j))
            {
                // uart_printf("*");
                // LCD_WR_DATA(fc);
                dma_buffer[dma_index++] = fc >> 8;
                dma_buffer[dma_index++] = fc & 0xFF;
            }
            else
            {
                // uart_printf(" ");
                // LCD_WR_DATA(bc);
                dma_buffer[dma_index++] = bc >> 8;
                dma_buffer[dma_index++] = bc & 0xFF;
            }
            k++;
            if ((k % sizey) == 0){
                // uart_printf("\r\n");
                k = 0;
                break; // 每列sizex个像素（对应一行）
            }
        }
    }

    LCD_Address_Set(x, y - sizey + 1, x + sizey - 1, y); // 注意：坐标系统是横屏的，y向上增长
    // LCD_WR_DATA_dma(&dma_buffer, dma_index);
    LCD_TX_Begin(LCD_TX_DATA);
    LCD_TX_Write_DMA_Async(&(dma_buffer[0]), dma_index, NULL);
    LCD_TX_End();      // CS deassert happens in the DMA/SPI completion ISR
    LCD_TX_WaitDone(); // prevents dma_buffer reuse races
}

// void OLED_ShowChar_0(u8 x, u8 y, u8 chr, u8 Char_Size)
// {
//     unsigned char c = 0, i = 0, disp_dat[16] = {0};
//     c = chr - ' '; // 得到偏移后的值
//     if (x > Max_Column - 1)
//     {
//         x = 0;
//         y = y + 2;
//     }
//     if (Char_Size == 16)
//     {
//         OLED_Set_Pos(x, y);
//         for (uint8_t id1 = 0; id1 < 8; id1++)
//         { // 8x16
//             for (uint8_t id2 = 0; id2 < 16; id2++)
//             {
//                 disp_dat[15 - id2] |= ((F8X16s[(c) * 16 + id1 + (id2 > 7 ? 8 : 0)] & (1 << (id2 - (id2 > 7 ? 8 : 0)))) >> (id2 - (id2 > 7 ? 8 : 0))) << id1;
//             }
//         }

//         for (i = 0; i < 16; i++)
//             OLED_WR_Byte(disp_dat[i], OLED_DATA);
//     }
//     else
//     {
//         OLED_Set_Pos(x, y);
//         for (uint8_t id1 = 0; id1 < 6; id1++)
//         { // 6x8/8x8
//             for (uint8_t id2 = 0; id2 < 8; id2++)
//             {
//                 disp_dat[7 - id2] |= ((F6x8s[c][id1] & (1 << id2)) >> id2) << id1;
//             }
//         }
//         for (i = 0; i < 8; i++)
//             OLED_WR_Byte(disp_dat[i], OLED_DATA);
//     }
// }

/**
 * @brief       显示字符串
 * @param       x:字符串显示位置列起始坐标
 * @param       y:字符串显示位置行起始坐标
 * @param       *s:显示的字符串内容
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
// 绘制字符串（常规坐标系）
/**
 * @brief 绘制字符串（常规坐标系）
 * @param x 起始列
 * @param y 起始行
 * @param s 字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowString(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint16_t sizey)
{
    while ((*s <= '~') && (*s >= ' ')) // 判断是不是非法字符
    {
        if (x > (LCD_W - 1) || y > (LCD_H - 1))
            return;
        LCD_ShowChar(x, y, *s, fc, bc, sizey);
        x += sizey / 2;
        s++;
    }
}


/**
 * @brief       横屏显示字符串
 * @param       x:字符串显示位置列起始坐标
 * @param       y:字符串显示位置行起始坐标
 * @param       *s:显示的字符串内容
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
// 绘制字符串（横屏坐标系）
/**
 * @brief 绘制字符串（横屏坐标系）
 * @param x 起始列
 * @param y 起始行
 * @param s 字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void LCD_ShowString_Hor(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint16_t sizey)
{
    while ((*s <= '~') && (*s >= ' ')) // 判断是不是非法字符
    {
        if (y > (LCD_W - 1) || x > (LCD_H - 1))
            return;
        LCD_ShowChar(x, y, *s, fc, bc, sizey);
        y += sizey;
        s++;
    }
}

/**
 * @brief       幂运算(内部调用)
 * @param       m:底数
 * @param       n:指数
 * @retval      result:m的n次幂
 */
uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
    {
        result *= m;
    }
    return result;
}

/**
 * @brief       显示数字
 * @param       x:数字显示位置列起始坐标
 * @param       y:数字显示位置行起始坐标
 * @param       num:显示的数字(0~4294967295)
 * @param       len:显示数字的位数
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp, enshow = 0;
    uint8_t sizex = sizey / 2;
    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        LCD_ShowChar(x + t * sizex, y, temp + '0', fc, bc, sizey);
    }
}

/**
 * @brief       显示数字
 * @param       x:数字显示位置列起始坐标
 * @param       y:数字显示位置行起始坐标
 * @param       num:显示的数字(0~4294967295)
 * @param       len:显示数字的位数
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowNum_Hor(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp, enshow = 0;
    uint8_t sizex = sizey / 2;
    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_Show_Space_Hor(x, y - t * sizex, bc, sizey);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        LCD_ShowChar_Digital_Hor(x, y - t * sizex, temp + '0', fc, bc, sizey);
    }
}

/**
 * @brief       显示数字
 * @param       x:数字显示位置列起始坐标
 * @param       y:数字显示位置行起始坐标
 * @param       num:显示的数字(0~4294967295)
 * @param       len:显示数字的位数
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowNum_Hor_dma(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t t, temp, enshow = 0;
    uint8_t sizex = sizey / 2;
    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_Show_Space_Hor_dma(x, y - t * sizex * 3 / 4, bc, sizey);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        LCD_ShowChar_Digital_Hor_dma(x, y - t * sizex * 3 / 4, temp + '0', fc, bc, sizey);
    }
}

/**
 * @brief       显示浮点数
 * @param       x:数字显示位置列起始坐标
 * @param       y:数字显示位置行起始坐标
 * @param       num:显示的浮点数
 * @param       pre:显示浮点数精度
 * @param       len:显示浮点数的位数(不包含小数点)
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t pre, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint32_t i, temp, num1;
    uint8_t sizex = sizey / 2;
    num1 = num * mypow(10, pre);
    for (i = 0; i < len; i++)
    {
        temp = (num1 / mypow(10, len - i - 1)) % 10;
        if (i == (len - pre))
        {
            LCD_ShowChar(x + (len - pre) * sizex, y, '.', fc, bc, sizey);
            i++;
            len += 1;
        }
        LCD_ShowChar(x + i * sizex, y, temp + '0', fc, bc, sizey);
    }
}


#if 0

/**
 * @brief       显示12x12汉字
 * @param       x:汉字显示位置列起始坐标
 * @param       y:汉字显示位置行起始坐标
 * @param       *s:显示中文字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void ;//LCD_ShowChinese12x12(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // 汉字数目
    uint16_t TypefaceNum; // 一个字符所占字节大小

    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont12) / sizeof(typFONT_GB12); // 统计汉字数目
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (tfont12[k].Msk[i] & (0x01 << j))
                    {
                        LCD_WR_DATA(fc);
                    }
                    else
                    {
                        LCD_WR_DATA(bc);
                    }
                    m++;
                    if (m % sizey == 0)
                    {
                        m = 0;
                        break;
                    }
                }
            }
        }
        continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
    }
}

/**
 * @brief       显示16x16汉字
 * @param       x:汉字显示位置列起始坐标
 * @param       y:汉字显示位置行起始坐标
 * @param       *s:显示中文字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void ;//LCD_ShowChinese16x16(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // 汉字数目
    uint16_t TypefaceNum; // 一个字符所占字节大小

    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont16) / sizeof(typFONT_GB16); // 统计汉字数目
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (tfont16[k].Msk[i] & (0x01 << j))
                    {
                        LCD_WR_DATA(fc);
                    }
                    else
                    {
                        LCD_WR_DATA(bc);
                    }
                    m++;
                    if (m % sizey == 0)
                    {
                        m = 0;
                        break;
                    }
                }
            }
        }
        continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
    }
}

/**
 * @brief       显示24x24汉字
 * @param       x:汉字显示位置列起始坐标
 * @param       y:汉字显示位置行起始坐标
 * @param       *s:显示中文字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void ;//LCD_ShowChinese24x24(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // 汉字数目
    uint16_t TypefaceNum; // 一个字符所占字节大小

    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont24) / sizeof(typFONT_GB24); // 统计汉字数目
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {

                    if (tfont24[k].Msk[i] & (0x01 << j))
                    {
                        LCD_WR_DATA(fc);
                    }
                    else
                    {
                        LCD_WR_DATA(bc);
                    }
                    m++;
                    if (m % sizey == 0)
                    {
                        m = 0;
                        break;
                    }
                }
            }
        }
        continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
    }
}

/**
 * @brief       显示32x32汉字
 * @param       x:汉字显示位置列起始坐标
 * @param       y:汉字显示位置行起始坐标
 * @param       *s:显示中文字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void ;//LCD_ShowChinese32x32(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // 汉字数目
    uint16_t TypefaceNum; // 一个字符所占字节大小

    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont32) / sizeof(typFONT_GB32); // 统计汉字数目
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (tfont32[k].Msk[i] & (0x01 << j))
                    {
                        LCD_WR_DATA(fc);
                    }
                    else
                    {
                        LCD_WR_DATA(bc);
                    }
                    m++;
                    if (m % sizey == 0)
                    {
                        m = 0;
                        break;
                    }
                }
            }
        }
        continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
    }
}

/**
 * @brief       显示汉字串
 * @param       x:汉字显示位置列起始坐标
 * @param       y:汉字显示位置行起始坐标
 * @param       *s:显示中文字符
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void ;//LCD_ShowChinese(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    while (*s != 0)
    {
        if (sizey == 12)
            ;//LCD_ShowChinese12x12(x, y, s, fc, bc, sizey);
        else if (sizey == 16)
            ;//LCD_ShowChinese16x16(x, y, s, fc, bc, sizey);
        else if (sizey == 24)
            ;//LCD_ShowChinese24x24(x, y, s, fc, bc, sizey);
        else if (sizey == 32)
            ;//LCD_ShowChinese32x32(x, y, s, fc, bc, sizey);
        else
            return;
        s += 2;
        x += sizey;
    }
}
#endif


/**
 * @brief       中英字符混显
 * @param       x:显示位置列起始坐标
 * @param       y:显示位置行起始坐标
 * @param       *s:显示字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowStr(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint16_t x0 = x;
    uint8_t bHz = 0; // 字符或者中文
    while (*s != 0)  // 数据未结束
    {
        if (!bHz) // 英文
        {
            if (x > (LCD_W - sizey / 2) || y > (LCD_H - sizey))
            {
                return;
            }
            if (*s > 0x80)
            {
                bHz = 1; // 中文
            }
            else // 字符
            {
                if (*s == 0x0D) // 换行符号
                {
                    y += sizey;
                    x = x0;
                    s++;
                }
                else
                {
                    LCD_ShowChar(x, y, *s, fc, bc, sizey);
                    x += sizey / 2; // 字符,为全字的一半
                }
                s++;
            }
        }
        else // 中文
        {
            if (x > (LCD_W - sizey) || y > (LCD_H - sizey))
            {
                return;
            }
            bHz = 0;
            if (sizey == 12)
                ;// LCD_ShowChinese12x12(x, y, s, fc, bc, sizey);
            else if (sizey == 16)
                ;// LCD_ShowChinese16x16(x, y, s, fc, bc, sizey);
            else if (sizey == 24)
                ;// LCD_ShowChinese24x24(x, y, s, fc, bc, sizey);
            else
                ;// LCD_ShowChinese32x32(x, y, s, fc, bc, sizey);
            s += 2;
            x += sizey;
        }
    }
}

/**
 * @brief       中英字符混显
 * @param       x:显示位置列起始坐标
 * @param       y:显示位置行起始坐标
 * @param       *s:显示字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowStr_Hor(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{

    uint16_t x0 = x;
    uint8_t bHz = 0;
    uint8_t sizex = sizey / 2;

    while (*s != 0)
    {
        if (!bHz)
        {
            if (x > (240 - sizex) || y < (sizey - 1))  // 注意横屏坐标方向，y是向上变小
                return;

            if (*s > 0x80)
            {
                bHz = 1;  // 中文
            }
            else
            {
                if (*s == 0x0D) // 换行
                {
                    y -= sizey;
                    x = x0;
                    s++;
                }
                else
                {
                    // uart_printf("%c\r\n", *s);
                    // uart_printf("\r\n", *s);
                    // uart_printf("\r\n", *s);
                    LCD_ShowChar_Hor(x, y, *s, fc, bc, sizey);
                    // x += sizey;  // 注意横屏是横向排字
                    y -= sizex;
                    s++;
                }
            }
        }
        else
        {
            if (x > (240 - sizey) || y < (sizey - 1))
                return;

            bHz = 0;
            // 中文显示函数根据 sizey 选择
            if (sizey == 24)
                ; // LCD_ShowChinese24x24(x, y, s, fc, bc);
            s += 2;
            x += sizey;
        }
    }
}

/**
 * @brief       中英字符混显
 * @param       x:显示位置列起始坐标
 * @param       y:显示位置行起始坐标
 * @param       *s:显示字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_ShowStr_Hor_dma(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{

    uint16_t x0 = x;
    uint8_t bHz = 0;
    uint8_t sizex = sizey / 2;

    while (*s != 0)
    {
        if (!bHz)
        {
            if (x > (240 - sizex) || y < (sizey - 1))  // 注意横屏坐标方向，y是向上变小
                return;

            if (*s > 0x80)
            {
                bHz = 1;  // 中文
            }
            else
            {
                if (*s == 0x0D) // 换行
                {
                    y -= sizey;
                    x = x0;
                    s++;
                }
                else
                {
                    // uart_printf("%c\r\n", *s);
                    // uart_printf("\r\n", *s);
                    // uart_printf("\r\n", *s);
                    LCD_ShowChar_Hor_dma(x, y, *s, fc, bc, sizey);
                    // x += sizey;  // 注意横屏是横向排字
                    y -= sizex;
                    s++;
                }
            }
        }
        else
        {
            if (x > (240 - sizey) || y < (sizey - 1))
                return;

            bHz = 0;
            // 中文显示函数根据 sizey 选择
            if (sizey == 24)
                ; // LCD_ShowChinese24x24(x, y, s, fc, bc);
            s += 2;
            x += sizey;
        }
    }
}

void LCD_ShowStr_Digital_Hor(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint16_t x0 = x;
    uint8_t bHz = 0;
    uint8_t sizex = sizey / 2;

    while (*s != 0)
    {
        if (!bHz)
        {
            if (x > (240 - sizex) || y < (sizey - 1)) // 注意横屏坐标方向，y是向上变小
                return;

            if (*s > 0x80)
            {
                bHz = 1; // 中文
            }
            else
            {
                if (*s == 0x0D) // 换行
                {
                    y -= sizey;
                    x = x0;
                    s++;
                }
                else
                {
                    LCD_ShowChar_Digital_Hor(x, y, *s, fc, bc, sizey);
                    y -= sizex;
                    s++;
                }
            }
        }
        else
        {
            uart_printf("Not_Digital\r\n");
        }
    }
}

/**
 * @brief       字符居中显示
 * @param       x:此输入参数无效
 * @param       y:显示位置行起始坐标
 * @param       *s:显示字符起始地址
 * @param       fc:字符颜色
 * @param       bc:字符背景颜色
 * @param       sizey:字符大小
 * @retval      无
 */
void LCD_StrCenter(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint16_t len = strlen((const char *)s);
    uint16_t x1 = (LCD_W - len * 8) / 2;
    LCD_ShowStr_Hor(x1, y, s, fc, bc, sizey);
}



/**
 * @brief       图片显示函数
 * @param       x:图片显示位置列起始坐标
 * @param       y:图片显示位置行起始坐标
 * @param       width:图片宽度
 * @param       height:图片高度
 * @param       pic:图片取模数组
 * @retval      无
 */
// 显示图片（阻塞式批量写入）
/**
 * @brief 显示图片（阻塞式分段写入）
 * @param x 起始列
 * @param y 起始行
 * @param width 宽度
 * @param height 高度
 * @param pic 图片数据（RGB565）
 */
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[])
{
    uint8_t ssd_buffer[DMA_SIZE];
    uint8_t picH, picL;
    uint16_t i, j;
    uint16_t xe, ye;
    uint32_t k = 0;

    uint16_t sd_index = 0;
    
    // x = x & ~0x03;
    // xe = x + width - 1;
    // xe = (xe + 3) & ~0x03;
    LCD_Address_Set(x, y, x + width - 1, y + height - 1);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            // picH = reverse_bits(pic[k * 2]);
            // picL = reverse_bits(pic[k * 2 + 1]);
            // picH = (pic[k * 2]);
            // picL = (pic[k * 2 + 1]);

            ssd_buffer[sd_index++] = (pic[k * 2]);
            ssd_buffer[sd_index++] = (pic[k * 2 + 1]);

            if (sd_index >= DMA_SIZE)
            {
                LCD_WR_DATA_Part(&ssd_buffer, DMA_SIZE);
                sd_index = 0;
            }
            k++;
        }
    }
}

/**
 * @brief       图片显示函数
 * @param       x:图片显示位置列起始坐标
 * @param       y:图片显示位置行起始坐标
 * @param       width:图片宽度
 * @param       height:图片高度
 * @param       pic:图片取模数组
 * @retval      无
 */
// 显示图片（DMA 分段写入，data_size 为单次发送大小）
/**
 * @brief 显示图片（DMA 分段写入）
 * @param x 起始列
 * @param y 起始行
 * @param width 宽度
 * @param height 高度
 * @param pic 图片数据（RGB565）
 * @param data_size 单次发送大小（字节）
 */
void LCD_ShowPicture_dma(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[], const uint16_t data_size)
{
    uint8_t picH, picL;
    uint16_t i, j;
    uint16_t xe, ye;
    uint32_t k = 0;
    // x = x & ~0x03;
    // xe = x + width - 1;
    // xe = (xe + 3) & ~0x03;
    // uint32_t data_size = sizeof(pic) / 4;
    // uart_printf("pl>%d\r\n", data_size);
    uint8_t dma_buffer[data_size];
    uint16_t sd_index = 0;
    LCD_Address_Set(x, y,  x + width - 1, y + height - 1);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            // picH = reverse_bits(pic[k * 2]);
            // picL = reverse_bits(pic[k * 2 + 1]);
            // picH = (pic[k * 2]);
            // picL = (pic[k * 2 + 1]);

            dma_buffer[sd_index++] = (pic[k * 2]);
            dma_buffer[sd_index++] = (pic[k * 2 + 1]);

            if (sd_index >= data_size)
            {
                LCD_WR_DATA_Part(&dma_buffer, sd_index);
                sd_index = 0;
            }
            k++;
        }
    }
    // display_image(&pic);
}


/**
 * @brief       图片显示函数
 * @param       x:图片显示位置列起始坐标
 * @param       y:图片显示位置行起始坐标
 * @param       width:图片宽度
 * @param       height:图片高度
 * @param       pic:图片取模数组
 * @retval      无
 */
// 显示图片（按固定批次发送，适用于大图）
/**
 * @brief 显示图片（固定批次发送，适用于大图）
 * @param x 起始列
 * @param y 起始行
 * @param width 宽度
 * @param height 高度
 * @param pic 图片数据（RGB565）
 * @param data_size 单次批次大小（字节）
 */
void LCD_ShowPicture_batch(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[], const uint16_t data_size)
{
    uint8_t picH, picL;
    uint16_t i, j;
    uint16_t xe, ye;
    uint32_t k = 0;
    // x = x & ~0x03;
    // xe = x + width - 1;
    // xe = (xe + 3) & ~0x03;
    // uint32_t data_size = sizeof(pic) / 4;
    // uart_printf("pl>%d\r\n", data_size);
    uint8_t dma_buffer[SPI_PIC_BATCH_N];
    uint16_t sd_index = 0;
    LCD_Address_Set(x, y,  x + width - 1, y + height - 1);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            // picH = reverse_bits(pic[k * 2]);
            // picL = reverse_bits(pic[k * 2 + 1]);
            // picH = (pic[k * 2]);
            // picL = (pic[k * 2 + 1]);

            dma_buffer[sd_index++] = (pic[k * 2]);
            dma_buffer[sd_index++] = (pic[k * 2 + 1]);

            if (sd_index >= SPI_PIC_BATCH_N)
            {
                // LCD_WR_DATA_Part(&dma_buffer, sd_index);
                LCD_DC_Set();
                LCD_WR_Bus_batch(&dma_buffer, sd_index);
                LCD_DC_Set();
                sd_index = 0;
            }
            k++;
        }
    }

    if (sd_index > 0)
    {
        LCD_DC_Set();
        LCD_WR_Bus_batch(&dma_buffer, sd_index);
        LCD_DC_Set();
        sd_index = 0;
    }

    // display_image(&pic);
}


/**
 * @brief 显示里程数字（数码字体，横屏）
 * @param x 起始列
 * @param y 起始行
 * @param s 数字字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void Kamingo_Show_Kms_digital(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint16_t sizey) {
     LCD_ShowStr_Digital_Hor(x, y, s, fc, bc, sizey);
}

/**
 * @brief 显示里程单位（km）
 * @param x 起始列
 * @param y 起始行
 * @param s 单位字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
static void Kamingo_Show_Kms_Unit(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey) {
    LCD_ShowStr_Hor(x, y, s, fc, bc, sizey);
}

/**
 * @brief 显示电量百分比字符串
 * @param x 起始列
 * @param y 起始行
 * @param s 数字字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
static void Kamingo_Show_Battery_capacity(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey) {
     LCD_ShowStr_Hor(x, y, s, fc, bc, sizey);
}

/**
 * @brief 显示电池图标
 * @param x 起始列
 * @param y 起始行
 * @param width 图标宽度
 * @param height 图标高度
 * @param gImage_ico 图标数据
 */
static void Kamingo_Show_Battery_icon(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t gImage_ico[]) {
    uint16_t data_size = sizeof(gImage_bty_1) / 20;
    // LCD_ShowPicture_batch(x, y, width, height, gImage_ico, data_size);
    LCD_ShowPicture_dma(x, y, width, height, gImage_ico, data_size);
}


/**
 * @brief 显示速度数字（数码字体，横屏）
 * @param x 起始列
 * @param y 起始行
 * @param s 数字字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void Kamingo_Show_Speed_digital(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey) {
    LCD_ShowStr_Digital_Hor(x, y, s, fc, bc, sizey);
}

/**
 * @brief 显示速度单位（km/h）
 * @param x 起始列
 * @param y 起始行
 * @param s 单位字符串
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 字符高度
 */
void Kamingo_Show_Speed_Unit(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey) {
     LCD_ShowStr_Hor(x, y, s, fc, bc, sizey);
}



/**
 * @brief 显示总里程（数码字体 + 单位）
 * @param x 起始列
 * @param y 起始行
 * @param total_kms 总里程数值
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 数字高度
 * @param reset true 时重置缓存，不绘制
 */
void Kamingo_Show_Total_Kms(uint16_t x, uint16_t y, uint16_t total_kms, uint16_t fc, uint16_t bc, uint8_t sizey, bool reset) {
    static uint16_t last_total_kms = -1;
    uint8_t sizex = sizey / 2;
    static bool unit_shown = false;
    static int8_t last_double_digit_2 = -1;
    static int8_t last_double_digit_3 = -1;
    static int8_t last_hundred_digit_3 = -1;

    if (reset) {
        last_total_kms = -1;
        last_double_digit_2 = -1;
        last_double_digit_3 = -1;
        last_hundred_digit_3 = -1;
        return;
    }
    // uart_printf("num>%d\r\n", total_kms);
    x = x + 2;
    if (!unit_shown)
    {
        Kamingo_Show_Kms_Unit(x, y - 3 * sizex + 3, KMS_UNIT, fc, bc, TOTAL_KMS_UNIT_SIZE);
        unit_shown = true;
    }
    if (last_total_kms != total_kms)
    // if (0)
    {
#if 0
        LCD_ShowNum_Hor_dma(x, y, total_kms, 3, fc, bc, sizey);
#else
        if (total_kms < 10)
        {
            // uart_printf("num>%d\r\n", total_kms);
            LCD_Show_Space_Hor_dma(x, y, bc, sizey);
            LCD_Show_Space_Hor_dma(x, y - sizex * 3 / 4, bc, sizey);
            // LCD_ShowNum_Hor_dma(x, y - sizex * 3 / 2, total_kms, 3, fc, bc, sizey);
            if (total_kms != 0) {
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, total_kms + '0', fc, bc, sizey);
            } else {
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, 0 + '0', fc, bc, sizey);
            }

            last_double_digit_2 = -1;
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;
        }
        else if (total_kms < 100)
        {
            int8_t digits[2];
            // static int8_t last_double_digit = 0;
            split_digits_2(total_kms, digits);

            if (last_double_digit_2 != digits[1])
            {
                LCD_Show_Space_Hor_dma(x, y, bc, sizey);
                // LCD_ShowNum_Hor_dma(x, y - sizex * 3 / 4, digits[1], 1, fc, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 4, digits[1] + '0', fc, bc, sizey);
            }   

            // LCD_Show_Space_Hor(x, y, bc, sizey);
            // LCD_ShowNum_Hor_dma(x, y - sizex * 3 / 2, digits[0], 1, fc, bc, sizey);
            LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, digits[0] + '0', fc, bc, sizey);

            last_double_digit_2 = digits[1];
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;

        }
        else if (total_kms < 1000)
        {
            int8_t digits[3];
            
            split_digits_3(total_kms, digits);

            if (last_hundred_digit_3 != digits[2])
            {
                // LCD_ShowNum_Hor_dma(x, y, digits[2], 1, fc, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x, y, digits[2] + '0', fc, bc, sizey);
            }

            if (last_double_digit_3 != digits[1])
            {
                // LCD_ShowNum_Hor_dma(x, y - sizex * 3 / 4, digits[1], 1, fc, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 4, digits[1] + '0', fc, bc, sizey);
            }

            // LCD_ShowNum_Hor_dma(x, y - sizex * 3 / 2, digits[0], 1, fc, bc, sizey);
            LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, digits[0] + '0', fc, bc, sizey);

            last_double_digit_3 = digits[1];
            last_hundred_digit_3 = digits[2];

            last_double_digit_2 = -1;

            // last_double_digit_3 = -1;
            // last_hundred_digit_3 = -1;
        }
#endif
        // uart_printf(">%s\r\n", kms_str);
        // Kamingo_Show_Kms_digital(x, y, kms_str, fc, bc, 32);

        // if (total_kms < 100)
        // uint8_t unit_sizey = TOTAL_KMS_UNIT_SIZE;
        // uint8_t unit_sizex = unit_sizey / 2;
        {
            // Kamingo_Show_Kms_Unit(x, y - 3*sizex + 3, KMS_UNIT, fc, bc, TOTAL_KMS_UNIT_SIZE);
        }
        // else
        // {
        //     Kamingo_Show_Kms_Unit(x + 8, y - 90, KMS_UNIT, fc, bc, 24);
        // }
    }
   
    
    last_total_kms = total_kms;
}

/**
 * @brief 显示电量百分比与电池图标
 * @param x 起始列
 * @param y 起始行
 * @param battery_capacity 电量百分比
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 数字高度
 * @param is_controller_battery 是否为控制器电池（预留）
 */
void Kamingo_Show_Battery(uint16_t x, uint16_t y, uint8_t battery_capacity, uint16_t fc, uint16_t bc, uint8_t sizey, bool is_controller_battery) {
    char bty_str[16];
    static uint8_t last_battery_capacity = -1;
    unsigned char* bty_ico;
    static unsigned char* last_bty_ico;

    static int8_t last_double_digit_2 = -1;
    static int8_t last_double_digit_3 = -1;
    static int8_t last_hundred_digit_3 = -1;
    if (last_battery_capacity != battery_capacity)
    {
        if (battery_capacity <= 25)
        {
            bty_ico = gImage_bty_1;
        }
        else if (battery_capacity <= 50)
        {
            bty_ico = gImage_bty_2;
        }
        else if (battery_capacity <= 75)
        {
            bty_ico = gImage_bty_3;
        }
        else
        {
            bty_ico = gImage_bty_4;
        }

        uint8_t y0 = 103;
        // uint8_t sizey = 36;
        uint8_t sizex = sizey / 2;

        if (battery_capacity < 10)
        {
            LCD_Show_Space_Hor_dma(x + 2, y, bc, sizey);
            LCD_Show_Space_Hor_dma(x + 2, y - sizex * 3 / 4, bc, sizey);
            LCD_ShowChar_Digital_Hor_dma(x + 2, y - sizex * 3 / 2, battery_capacity + '0', fc, bc, sizey);

            last_double_digit_2 = -1;
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;
        }
        else if (battery_capacity < 100)
        {
            int8_t digits[2];
            split_digits_2(battery_capacity, digits);
            if (last_double_digit_2 != digits[1])
            {
                LCD_Show_Space_Hor_dma(x, y, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x + 2, y - sizex * 3 / 4, digits[1] + '0', fc, bc, sizey);
            }
            LCD_ShowChar_Digital_Hor_dma(x + 2, y - sizex * 3 / 2, digits[0] + '0', fc, bc, sizey);

            last_double_digit_2 = digits[1];
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;
            // Kamingo_Show_Battery_capacity(x + 2, y0 + sizey / 2, bty_str, fc, bc, sizey);
        }
        else if (battery_capacity < 1000)
        {
            int8_t digits[3];
            LCD_ShowNum_Hor_dma(x, y, 100, 3, fc, bc, sizey);
            
            split_digits_3(100, digits);
            last_double_digit_3 = digits[1];
            last_hundred_digit_3 = digits[2];

            last_double_digit_2 = -1;
        }

        if (last_bty_ico != bty_ico)
        {
            Kamingo_Show_Battery_icon(x + 2, 12, 30, 62, (unsigned char *)bty_ico);
        }
    }

    last_battery_capacity = battery_capacity;
    last_bty_ico = bty_ico;
}


extern volatile control_model_state model_state;
extern volatile bool cruise_on;
/**
 * @brief 显示骑行模式图标
 * @param x 起始列
 * @param y 起始行
 * @param width 图标宽度
 * @param height 图标高度
 * @param ride_type 当前模式
 * @param reset true 时重置缓存，不绘制
 */
void Kamingo_Show_Ride_type(uint16_t x, uint16_t y, uint16_t width, uint16_t height, control_model_state ride_type, bool reset) {
     unsigned char* ride_ico;
     static control_model_state last_ride_type = CTL_MODEL_STOP;
     uint32_t data_size = sizeof(gImage_pas_bike_rot) / 2;
    if (reset) {
        last_ride_type = -1;
        return;
    }
     if (last_ride_type != ride_type)
     {
         switch (ride_type)
         {
            case CTL_MODEL_STOP:
            case CTL_MODEL_UNLOCK:
                /* code */
                ride_ico = gImage_pas_bike_rot;
                break;
            case CTL_MODEL_B1_LEVEL:
            case CTL_MODEL_B2_LEVEL:
                if (cruise_on)
                {
                    ride_ico = gImage_pas_cruise_rot;
                }
                else
                {
                    ride_ico = gImage_pas_ebike_rot;
                }
                /* code */
                break;

            default:
                ride_ico = gImage_pas_bike_rot;
                break;
         }
         
        //  uart_printf("pl>%d\r\n", data_size);
        //  LCD_ShowPicture_batch(x, y, width, height, ride_ico, data_size);
         LCD_ShowPicture_dma(x, y, width, height, ride_ico, data_size);
     }
     last_ride_type = ride_type;
}

/**
 * @brief 显示速度（km/h，数码字体）
 * @param x 起始列
 * @param y 起始行
 * @param speed 速度值
 * @param fc 前景色
 * @param bc 背景色
 * @param sizey 数字高度
 * @param reset true 时重置缓存，不绘制
 */
void Kamingo_Show_Speed_Kmh(uint16_t x, uint16_t y, uint16_t speed, uint16_t fc, uint16_t bc, uint8_t sizey, bool reset)
{
    // uint8_t sizey = 56;
    static uint16_t last_speed_kmh = -1;
    uint8_t sizex = sizey / 2;
    static bool unit_shown = false;
    static int8_t last_double_digit_2 = -1;
    static int8_t last_double_digit_3 = -1;
    static int8_t last_hundred_digit_3 = -1;

    if (reset) {
        last_speed_kmh = -1;
        last_double_digit_2 = -1;
        last_double_digit_3 = -1;
        last_hundred_digit_3 = -1;
        return;
    }

    if (!unit_shown)
    {
        Kamingo_Show_Speed_Unit(x + 17, y - 3 * sizex - 2, SPEED_UNIT, fc, bc, SPEED_KMH_UNIT_SIZE);
        unit_shown = true;
    }

    if (last_speed_kmh != speed)
    // if (0)
    {
        // int_to_str(speed, speed_str);
#if 0    
    if (speed < 10)
    {
        LCD_Show_Space_Hor(x, y, bc, sizey);
        LCD_Show_Space_Hor(x, y - sizex, bc, sizey);
        LCD_ShowNum_Hor(x, y - sizey, speed, 1, fc, bc, sizey);
    }
    else if (speed < 100)
    {
        LCD_Show_Space_Hor(x, y, bc, sizey);
        LCD_ShowNum_Hor(x, y - sizex, speed, 2, fc, bc, sizey);
    }
    else if (speed < 1000)
    {
        LCD_ShowNum_Hor(x, y, speed, 3, fc, bc, sizey);
    }
#else
        if (speed < 10)
        {
            // LCD_Show_Space_Hor_dma(x, y, bc, sizey);
            // LCD_Show_Space_Hor_dma(x, y - sizex * 3 / 4, bc, sizey);
            if (speed != 0) {
                LCD_ShowNum_Hor_dma(x, y, speed, 3, fc, bc, sizey);
            } else {
                LCD_Show_Space_Hor_dma(x, y, bc, sizey);
                LCD_Show_Space_Hor_dma(x, y - sizex * 3 / 4, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, 0 + '0', fc, bc, sizey);
            }

            last_double_digit_2 = -1;
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;
        }
        else if (speed < 100)
        {
            int8_t digits[2];
            split_digits_2(speed, digits);
            if (last_double_digit_2 != digits[1])
            {
                LCD_Show_Space_Hor_dma(x, y, bc, sizey);
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 4, digits[1] + '0', fc, bc, sizey);
            }
            LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, digits[0] + '0', fc, bc, sizey);

            last_double_digit_2 = digits[1];
            last_double_digit_3 = -1;
            last_hundred_digit_3 = -1;
        }
        else if (speed < 1000)
        {
            int8_t digits[3];
            split_digits_3(speed, digits);
            if (last_hundred_digit_3 != digits[2])
            {
                LCD_ShowChar_Digital_Hor_dma(x, y, digits[2] + '0', fc, bc, sizey);
            }

            if (last_double_digit_3 != digits[1])
            {
                LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 4, digits[1] + '0', fc, bc, sizey);
            }

            LCD_ShowChar_Digital_Hor_dma(x, y - sizex * 3 / 2, digits[0] + '0', fc, bc, sizey);

            last_double_digit_3 = digits[1];
            last_hundred_digit_3 = digits[2];

            last_double_digit_2 = -1;
        }
#endif
    }
    last_speed_kmh = speed;
    // Kamingo_Show_Speed_digital(x, y, speed_str, fc, bc, 56);
}

/**
 * @brief 测试用：切换骑行模式状态（含巡航标志）
 */
void modify_mode_state(void)
{
    if (model_state == CTL_MODEL_STOP || model_state == CTL_MODEL_UNLOCK)
    {
        model_state = CTL_MODEL_B1_LEVEL;
    }
    else if (model_state == CTL_MODEL_B1_LEVEL)
    {
        model_state = CTL_MODEL_STOP;
        if (cruise_on)
        {
            cruise_on = false;
        }
        else
        {
            cruise_on = true;
        }
    }
    else
    {
        model_state = CTL_MODEL_STOP;
    }
}

/**
 * @brief UI 刷新入口：按固定顺序刷新各组件
 * @param tatal_kms 总里程
 * @param battery_capacity 电量百分比
 * @param speed 速度
 */
void update_ui(int tatal_kms, uint8_t battery_capacity, uint16_t speed)
{
    // UI 组件依次刷新，中间插入小延时以平滑显示。
    delay_ms(30);
    Kamingo_Show_Total_Kms(6, 250, tatal_kms, WHITE, BLACK, TOTAL_KMS_DIGITAL_SIZE, false);
    delay_ms(30);
    Kamingo_Show_Battery(10, 135, battery_capacity, WHITE, BLACK, BATTERY_DIGITAL_SIZE, true);
    delay_ms(30);
    Kamingo_Show_Ride_type(60, 170, RIDE_TYPE_PIC_SIZE, RIDE_TYPE_PIC_SIZE, model_state, false);
    delay_ms(30);
    Kamingo_Show_Speed_Kmh(57, 180, speed, MYELLOW, BLACK, SPEED_KMH_DIGITAL_SIZE, false);
}

/**
 * @brief 测试用：循环随机数据刷新 UI
 */
void test_lcd2(void)
{
    static int16_t total_kms = 0;
    int16_t total_kms1 = rand() % 100;
    delay_ms(30);
    LCD_KEY1_Clr();
    delay_ms(30);
    modify_mode_state();
    Kamingo_Show_Ride_type(60, 170, RIDE_TYPE_PIC_SIZE, RIDE_TYPE_PIC_SIZE, model_state, false);
    delay_ms(30);
    Kamingo_Show_Battery(10, 135, total_kms1, WHITE, BLACK, BATTERY_DIGITAL_SIZE, true);
    delay_ms(30);
    Kamingo_Show_Total_Kms(6, 250, total_kms++, WHITE, BLACK, TOTAL_KMS_DIGITAL_SIZE, false);
    delay_ms(30);
    Kamingo_Show_Speed_Kmh(57, 180, total_kms1, MYELLOW, BLACK, SPEED_KMH_DIGITAL_SIZE, false);
    
    //LCD_KEY1_Set();

    if (total_kms >= 122) {
        total_kms = 0;
    }
}
