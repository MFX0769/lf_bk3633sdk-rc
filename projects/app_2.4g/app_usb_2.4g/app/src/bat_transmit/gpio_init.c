#include "drv_gpio.h"
#include "gpio_init.h"
#include "user_config.h"
#include "timer_handler.h"



static volatile uint32_t last_tick = 0;
uint16_t cnt=0;
void pair_gpio_isr(void) {
    // uint32_t now = Get_SysTick_ms();
    // if(Get_SysTick_ms() - last_tick < 20) //20ms消抖
    // {
    //     return;
    // }
    // if (gpio_get_input(Port_Pin(0,2)) == 1) {
    //     return; 
    // }
        
    // last_tick = now;
    //按键有效,处理事件
    //gpio_int_disable(Port_Pin(pair_port,pair_pin)); //禁用中断，防止按键抖动多次触发
    uart_printf("pair gpio irq!\r\n");
    Timer1_Start(1,150000); //启动15ms定时器
    cnt++;
}


// 假设按键接在GPIO port0 pin2
void pair_gpio_init(void) {
    // 初始化GPIO和中断
    gpio_config(Port_Pin(pair_port,pair_pin), GPIO_INPUT, GPIO_PULL_HIGH); // 输入，上拉
    gpio_int_enable(Port_Pin(pair_port,pair_pin), GPIO_INT_EDGE_FALLING,pair_gpio_isr); // 设置为下降沿中断
    //gpio_int_disable(Port_Pin(pair_port,pair_pin)); // 禁用中断
    
    uart_printf("pair Initialized on GPIO port%d pin%d\r\n", pair_port, pair_pin);
}

