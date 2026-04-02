#ifndef _LCD_KMG_H_
#define _LCD_KMG_H_

#include "lcd_init.h"

#define TOTAL_KMS_DIGITAL_SIZE     36
#define TOTAL_KMS_UNIT_SIZE        32

#define BATTERY_DIGITAL_SIZE       32

#define SPEED_KMH_DIGITAL_SIZE     56
#define SPEED_KMH_UNIT_SIZE        32

#define RIDE_TYPE_PIC_SIZE         48

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey);                             // ��ʾ�ַ�����
void LCD_ShowChar_Hor(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowString(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint16_t sizey);                           // ��ʾ�ַ���
uint32_t mypow(uint8_t m, uint8_t n);                                                                                                      // �������ڲ�����
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);                // ��ʾ����
void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t pre, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey); // ��ʾ������
void LCD_ShowChinese12x12(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                      // ��ʾ12x12�ַ���
void LCD_ShowChinese16x16(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                      // ��ʾ16x16�ַ���
void LCD_ShowChinese24x24(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                      // ��ʾ24x24�ַ���
void LCD_ShowChinese32x32(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                      // ��ʾ32x32�ַ���
void LCD_ShowChinese(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                           // ��ʾ�����ַ���
void LCD_ShowStr(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                               // ��Ӣ����
void LCD_ShowStr_Hor(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                               // ��Ӣ����
void LCD_StrCenter(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);                             // ������ʾ
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[]);       
void LCD_Show_Space_Hor(uint16_t x, uint16_t y, uint16_t bc, uint8_t sizey);                                 // ��ʾͼƬ
void test_lcd(void);
void test_lcd2(void);

/* UI模式定义 */
#define UI_MODE_NORMAL    0
#define UI_MODE_PAIRING   1
#define UI_MODE_PAIR_OK   2
#define UI_MODE_PAIR_FAIL 3

/* 动力电池连接状态 */
#define BAT_CONN_UNPAIRED  0
#define BAT_CONN_OFFLINE   1
#define BAT_CONN_ONLINE    2

void update_ui_test(uint8_t hall, uint8_t bat_soc,
                    uint8_t ui_mode, uint8_t bat_paired, const uint8_t *bat_addr,
                    uint8_t bat_conn);
void update_ui(int tatal_kms, uint8_t battery_capacity, uint16_t speed);
// void Kamingo_Show_Battery(uint16_t x, uint16_t y, uint8_t battery_capacity, uint16_t fc, uint16_t bc, uint8_t sizey) ;
void Kamingo_Show_Battery(uint16_t x, uint16_t y, uint8_t battery_capacity, uint16_t fc, uint16_t bc, uint8_t sizey, bool is_controller_battery);
void Kamingo_Show_Total_Kms(uint16_t x, uint16_t y, uint16_t total_kms, uint16_t fc, uint16_t bc, uint8_t sizey, bool reset);
void Kamingo_Show_Speed_Kmh(uint16_t x, uint16_t y, uint16_t speed, uint16_t fc, uint16_t bc, uint8_t sizey, bool reset);
void LCD_ShowPicture_dma(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[], const uint16_t data_size);
void LCD_Show_Space_Hor_dma(uint16_t x, uint16_t y, uint16_t bc, uint8_t sizey);
void LCD_ShowChar_Digital_Hor_dma(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowNum_Hor_dma(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowStr_Hor_dma(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowChar_Hor_dma(uint16_t x, uint16_t y, uint8_t ch, uint16_t fc, uint16_t bc, uint8_t sizey);
void LCD_ShowPicture_batch(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[], const uint16_t data_size);

#endif
