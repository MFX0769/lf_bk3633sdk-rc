#include "app_sleep.h"
#include "icu.h"
#include "aon_rtc.h"
#include "BK3633_RegList.h"
#include "user_config.h"
#include "timer_handler.h"

//调用icu中的休眠函数，进入深度睡眠，并设置aon定时唤醒
/* 
*/
void app_enter_sleep_with_wakeup_by_timer(uint32_t sleep_ms,uint8_t sleep_flag)
{
    if(!sleep_flag)
        return ;

    //设置定时器1_1唤醒时间
    uint32_t start = Get_SysTick_ms();
    //uart_printf("deep sleep:%d\r\n", start);
    Timer1_Start_setload_value(1, sleep_ms *2); //timer1_1做唤醒源，cnt单位为0.5ms
    
    cpu_24_reduce_voltage_sleep(); //进入低电压睡眠

    // 代码执行到这里，说明已经被中断唤醒，且 ISR 已经执行完毕
    //uart_printf("wake up from deep sleep:%d,sleep for %d ms\r\n", Get_SysTick_ms(), Get_SysTick_ms()-start);

}
