#include "app_sleep.h"
#include "icu.h"
#include "aon_rtc.h"
#include "BK3633_RegList.h"
#include "user_config.h"
#include "timer_handler.h"
#include "hal_drv_rf.h"


//调用icu中的休眠函数，进入深度睡眠，并设置aon定时唤醒
/* 
*/
void app_enter_sleep_with_wakeup_by_timer(uint32_t sleep_ms,uint8_t sleep_flag)
{
    if(!sleep_flag)
        return ;

    //设置定时器1_1唤醒时间
    //uart_printf("deep sleep:%d\r\n", start);
    Timer1_Start_setload_value(1, sleep_ms *2); //timer1_1做唤醒源，cnt单位为0.5ms

    // 完全关闭RF模块
    __HAL_RF_PowerDown();
    __HAL_RF_CHIP_DIS();

    cpu_24_reduce_voltage_sleep(); //进入低电压睡眠

    __HAL_RF_PowerUp();
    __HAL_RF_CHIP_EN();

    // 唤醒后停止Timer1，避免持续功耗

    // 唤醒后不立即上电RF，由调度器在需要通信时再上电
    // __HAL_RF_PowerUp(); // 删除此行，避免不必要的RF功耗


    // 代码执行到这里，说明已经被中断唤醒，且 ISR 已经执行完毕
    //uart_printf("wake up from deep sleep:%d,sleep for %d ms\r\n", Get_SysTick_ms(), Get_SysTick_ms()-start);

}
