/*
 * timer_handler.c
 * 调用driver_timer.c中的定时器相关接口，完成handler的注册与初始化
 *  Created on: 
*/
#include "timer_handler.h"
#include "user_config.h"
#include "drv_gpio.h"
#include "BK3633_RegList.h"

/* 使用timer1_0作为系统ms级时基 */
static volatile uint32_t Systick_ms=0;
static volatile uint16_t overflow_cnt =0;

static void Inc_Systick(void)
{
    //溢出中断更新
    overflow_cnt++;
    //翻转电平测试
    //gpio_toggle(Port_Pin(0,7));
    //uart_printf("systick_irq:%d\r\n",Systick_ms);
}
static void timer_sys_timebase_init(void)
{
    Timer_Initial(1,0,16-1); // T1,32K,最大分频16, cnt单位为1/2=0.5ms
    //1ms一次的定时中断
    //Timer1_Start_setload_value(0,2);
    //4294967295*0.5ms=2147483647.5ms=2147483.6475s=596.5232 hours
    //最大值溢出时间约为596小时
    Timer1_Start_setload_value(0,0xffffffff);
    
    //Timer1_Start(0,1000); // 1ms
    timer_cb_register(1,0,Inc_Systick);
    //中断使能  需要去intc.c中手动使能timer1中断
}

uint32_t read_timer1_0_cnt(void)
{
    // 发起读取请求
    // 目标: 读取 Timer1_0 (Index=0)
    // 操作: 设置 Bit[3:2]=0 (Index 0), 设置 Bit[0]=1 (Start Read)
    addTIMER1_Reg0x4 = 1;

    //回读一次状态寄存器。
    // 这行代码的作用是强迫 CPU 等待总线写操作真正完成（Flush Write Buffer）
    volatile uint32_t dummy = addTIMER1_Reg0x4;
    while(addTIMER1_Reg0x4 & 1); //等待硬件把 addTIMER1_Reg0x4的第0位:timerl_cnt_read 清零

    return addTIMER1_Reg0x5;
}

uint32_t Get_SysTick_ms(void)
{
    uint32_t overflow_cnt_tmp;
    uint32_t cnt_val_tmp;
    
    //先读一次溢出计数器和cnt
    overflow_cnt_tmp = overflow_cnt;       
    cnt_val_tmp = read_timer1_0_cnt(); 
    //uart_printf("cnt=%d,ovf=%d\r\n", cnt_val_tmp, overflow_cnt_tmp);
    
    //再读一次溢出计数器 
    if (overflow_cnt != overflow_cnt_tmp)
    {
        // 如果不等，说明刚才那几微秒内发生了溢出,重新读取cnt和溢出计数器
        overflow_cnt_tmp = overflow_cnt;
        cnt_val_tmp = read_timer1_0_cnt(); 
    }
    
    // 下溢出时间公式：Total = Ovf * Period + (Period - Current)
    //uint32_t total_ticks = (overflow_cnt_tmp * 0xFFFFFFFF) + (0xFFFFFFFF - cnt_val_tmp);
    //上溢出时间公式
    uint32_t total_ticks = (overflow_cnt_tmp * 0xFFFFFFFF) + cnt_val_tmp;    
    // 转换时间1tick = 0.5ms
    Systick_ms = total_ticks * 0.5;
   // uart_printf("return=%d\r\n", Systick_ms);
    return Systick_ms;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = Get_SysTick_ms();
    while((Get_SysTick_ms() - start) < ms);
}
     
/* timer1_1做唤醒源 */
static void timer1_1_cb(void)
{
    //关闭timer1_1
    clrf_TIMER1_Reg0x3_timer1_en;
    //uart_printf("timer1_1_cb\r\n");
}
static void timer1_1_init(void)
{
    timer_cb_register(1,1,timer1_1_cb);
}

void Timer_Handler_Init(void)
{   
    /* timer1_n使用了32k时钟，是aon_timer,低功耗模式下也在跑 */
    timer_sys_timebase_init();
    timer1_1_init();
}