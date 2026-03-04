#include "app_sleep.h"
#include "icu.h"
#include "aon_rtc.h"
#include "BK3633_RegList.h"
#include "user_config.h"
#include "timer_handler.h"



// //调用icu中的休眠函数，进入深度睡眠，并设置aon rtc定时唤醒
// void app_enter_deep_sleep_with_wakeup_by_rtc(uint32_t sleep_ms)
// {
//     //设置aon rtc唤醒时间
//     //复位rtc计数器
//     setf_rtc_aon_Reg0x0_rtc_cnt_reset;
//     //RTC是32k低速域，加一点回读/延时，确保硬件看到了复位信号
//     delay_ms(1);
//     clrf_rtc_aon_Reg0x0_rtc_cnt_reset;

//     addrtc_aon_Reg0x2 = sleep_ms * 32; //设置唤醒时间，单位为ms，rtc计数器为32kHz
//     addrtc_aon_Reg0x1 = sleep_ms * 32; //设置最大比较值
    
//     setf_rtc_aon_Reg0x0_rtc_aon_int  ;  //清除aon int标志位
//     setf_rtc_aon_Reg0x0_rtc_tick_int;   //清除tick中断标志位
//     setf_rtc_aon_Reg0x0_rtc_tick_int_en;
//     setf_rtc_aon_Reg0x0_rtc_aon_int_en; //使能aon int
//     //clrf_rtc_aon_Reg0x0_rtc_aon_int_en;  //失能aon int中断
//     //clrf_rtc_aon_Reg0x0_rtc_tick_int_en; //失能tick中断
//     uint32_t rtc_tick_val = addrtc_aon_Reg0x3;
//    while(1)
//         //uart_printf("sleep:%d,rtc_tick:%d\r\n", addrtc_aon_Reg0x2, addrtc_aon_Reg0x3);
//         uart_printf("sleep:%d,rtc_tick:%d\r\n", addrtc_aon_Reg0x1, addrtc_aon_Reg0x3);



//     cpu_24_reduce_voltage_sleep(); //进入低电压睡眠

//     // 代码执行到这里，说明已经被中断唤醒，且 ISR 已经执行完毕

//     // 立即关闭 Tick 中断
//     // 如果不关，系统醒着的时候每隔 10ms 还会被中断打断一次。
//     clrf_rtc_aon_Reg0x0_rtc_tick_int_en;

//     uart_printf("wake up from deep sleep\r\n");

// }

//调用icu中的休眠函数，进入深度睡眠，并设置aon定时唤醒
void app_enter_sleep_with_wakeup_by_timer(uint32_t sleep_ms)
{
    //设置定时器1_1唤醒时间
    uint32_t start = Get_SysTick_ms();
    uart_printf("deep sleep:%d\r\n", start);
    Timer1_Start_setload_value(1, sleep_ms *2); //timer1_1做唤醒源，cnt单位为0.5ms
    
    cpu_24_reduce_voltage_sleep(); //进入低电压睡眠

    // 代码执行到这里，说明已经被中断唤醒，且 ISR 已经执行完毕
    uart_printf("wake up from deep sleep:%d,sleep for %d ms\r\n", Get_SysTick_ms(), Get_SysTick_ms()-start);

}
