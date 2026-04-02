#include "app_sleep.h"
#include "icu.h"
#include "aon_rtc.h"
#include "BK3633_RegList.h"
#include "user_config.h"
#include "timer_handler.h"
#include "hal_drv_rf.h"
#include "intc.h"
#include "reg_intc.h"


//调用icu中的休眠函数，进入深度睡眠，并设置aon定时唤醒
/*
*/
void app_enter_sleep_with_wakeup_by_timer(uint32_t sleep_ms,uint8_t sleep_flag)
{
    if(!sleep_flag)
        return ;

    //设置定时器1_1唤醒时间
    Timer1_Start_setload_value(1, sleep_ms *2); //timer1_1做唤醒源，cnt单位为0.5ms

    // 清除RF所有中断标志（RF还在上电状态，可以访问寄存器）
    // 防止sleep期间残留RF中断在唤醒后ISR中访问已下电的RF寄存器
    // __HAL_RF_CLEAR_IRQ_FLAGS(IRQ_RX_DR_MASK | IRQ_TX_DS_MASK | IRQ_MAX_RT_MASK);
    // intc_status_clear(INT_BK24_bit);

    // 完全关闭RF模块
    __HAL_RF_PowerDown();
    __HAL_RF_CHIP_DIS();

    // 注意：不禁用BK24系统中断！Timer1无法唤醒CPU_PWD，
    // 需要保留BK24作为潜在唤醒源（清除标志后正常不会触发）

    cpu_24_reduce_voltage_sleep(); //进入低电压睡眠
    // uart_printf("[W1]\r\n");

    __HAL_RF_PowerUp();
    // uart_printf("[W2]\r\n");
    __HAL_RF_CHIP_EN();
    // uart_printf("[W3]\r\n");

}
