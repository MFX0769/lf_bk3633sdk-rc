#include "drv_gpio.h"
#include <stddef.h>



#define GPIO_MAX_NUM        40
#define GPIO_INT_MAX_NUM    32

// 回调函数表
static gpio_isr_t s_gpio_callbacks[GPIO_INT_MAX_NUM] = {NULL};

void gpio_config(gpio_num_t gpio_num, gpio_dir_t dir, gpio_pull_t pull)
{
    if (gpio_num >= GPIO_MAX_NUM) return;

    volatile uint32_t *reg = &REG_GPIO_CFG(gpio_num);
    uint32_t val = *reg;

    // 1. 先清除相关配置位
    // Bit 2: Input En (高电平有效)
    // Bit 3: Output En (低电平有效)
    // Bit 4: Pull Mode (1=Pull Up, 0=Pull Down)
    // Bit 5: Pull En  (高电平有效)
    // Bit 6: 2nd Fun En
    val &= ~((1<<GPIO_INPUT_EN) | (1<<GPIO_OUTPUT_EN) | (1<<GPIO_PULL_MODE) | (1<<GPIO_PULL_EN) | (1<<GPIO_2FUN_EN));

    // 2. 设置方向
    switch(dir) {
        case GPIO_OUTPUT:
            // Input En = 0 (Disable)
            // Output En = 0 (Enable, Low Active)
            val &= ~(1 << GPIO_INPUT_EN); 
            val &= ~(1 << GPIO_OUTPUT_EN); 
            break; 

        case GPIO_INPUT:
            // Input En = 1 (Enable)
            // Output En = 1 (Disable, Low Active)
            val |= (1 << GPIO_INPUT_EN); 
            val |= (1 << GPIO_OUTPUT_EN); 
            break;

        case GPIO_FLOAT:  
            val &= ~(1 << GPIO_INPUT_EN);
            val |= (1 << GPIO_OUTPUT_EN); 
            break;

        case GPIO_SC_FUN: 
            val |= (1 << GPIO_2FUN_EN); 
            val |= (1 << GPIO_OUTPUT_EN); 
            break;
    }

    // 3. 设置上下拉
   switch(pull) {
    case GPIO_PULL_HIGH:        
        val |= (1<<GPIO_PULL_EN);
        val |= (1<<GPIO_PULL_MODE);
        break;
    case GPIO_PULL_LOW:
        val |= (1<<GPIO_PULL_EN);
        val &= ~(1<<GPIO_PULL_MODE);
        break;
    case GPIO_PULL_NONE:
        val &= ~(1<<GPIO_PULL_EN);
        break;
    }

    //写回寄存器
    *reg = val;
}

void gpio_set(gpio_num_t gpio_num, uint8_t val)
{
    if (gpio_num >= GPIO_MAX_NUM) return;
    // Bit 1: Output Value
    if (val) 
        REG_GPIO_CFG(gpio_num) |= (1 << GPIO_OUTPUT_VALUE);
    else     
        REG_GPIO_CFG(gpio_num) &= ~(1 << GPIO_OUTPUT_VALUE);
}

void gpio_trigger(gpio_num_t gpio_num)
{
    gpio_set(gpio_num, 1);
    gpio_set(gpio_num, 0);
}

uint8_t gpio_get_input(gpio_num_t gpio_num)
{
    if (gpio_num >= GPIO_MAX_NUM) return 0;
    return (REG_GPIO_CFG(gpio_num) & (1 << GPIO_INPUT_VALUE)) ? 1 : 0;
}

void gpio_toggle(gpio_num_t gpio_num)
{
    if (gpio_num >= GPIO_MAX_NUM) return;
    // 异或翻转 Bit 1
    REG_GPIO_CFG(gpio_num) ^= (1 << GPIO_OUTPUT_VALUE);
}

// -----------------------------------------------------------
// 中断部分 (gpio_num < 32)
// -----------------------------------------------------------

void gpio_int_enable(gpio_num_t gpio_num, gpio_int_mode_t mode, gpio_isr_t cb)
{
    if (gpio_num >= GPIO_INT_MAX_NUM) return;

    if(cb != NULL)
    {
        s_gpio_callbacks[gpio_num] = cb;
    }
    
    // 配置触发类型 每2位控制一个GPIO
    if (gpio_num < 16) {
        REG_GPIO_INT_TYPE_L &= ~(3 << (gpio_num * 2));
        REG_GPIO_INT_TYPE_L |=  (mode << (gpio_num * 2));
    } else {
        uint8_t shift = (gpio_num - 16) * 2;
        REG_GPIO_INT_TYPE_H &= ~(3 << shift);
        REG_GPIO_INT_TYPE_H |=  (mode << shift);
    }

    // 先清标志，再使能
    REG_GPIO_INT_STAT = (1 << gpio_num);
    REG_GPIO_INT_EN |= (1 << gpio_num);
}

void gpio_int_disable(gpio_num_t gpio_num)
{
    if (gpio_num >= GPIO_INT_MAX_NUM) return;
    
    REG_GPIO_INT_EN &= ~(1 << gpio_num);
    s_gpio_callbacks[gpio_num] = NULL;
}

// 中断服务程序
// void gpio_isr(void)
// {
//     //获取触发且使能了的中断位掩码
//     uint32_t status = REG_GPIO_INT_STAT;
//     uint32_t enable = REG_GPIO_INT_EN;
//     uint32_t pending = status & enable;

//    // 只要有位为1，就处理
// while (pending) {
//     // 这是一个位操作技巧：pending & -pending 可以提取最低位的 1
//     // 但为了简单，我们还是遍历 0~31
//     for (int i = 0; i < 32; i++) {
//         if (pending & (1 << i)) {
//             // 清除该位标志 (写1清0)
//             REG_GPIO_INT_STAT = (1 << i);
            
//             // 必须在清除后从 pending 中移除该位，避免死循环（如果用while pending逻辑）
//             // 这里用 for 循环比较安全，但效率略低，可优化
            
//             if (s_gpio_callbacks[i]) {
//                 s_gpio_callbacks[i]();
//             }
//         }
//     }
//     break; // 这里的简单实现只扫一遍
//     }
// }

//利用 CPU 指令直接查找，无循环轮询
void gpio_isr(void)
{
    // 1. 读取状态 (Status) 和 使能 (Enable)
    // 必须与运算，只处理 "既触发了 又 使能了" 的中断
    uint32_t status = REG_GPIO_INT_STAT;
    uint32_t enable = REG_GPIO_INT_EN;
    uint32_t pending = status & enable;

    // 只要还有待处理的位，就一直循环
    while (pending) {
        // 直接使用__builtin_ctz编译器内建函数 获取最低位1的索引 (0~31)
        // 例如 pending = 0x00000008 (二进制 ...1000)，结果就是 3
        // ARM 编译器会把它编译成一条 CLZ 或 CTZ 汇编指令，极快
        int idx = __builtin_ctz(pending); 

        // 清除硬件中断标志 (写1清0)
        REG_GPIO_INT_STAT = (1UL << idx);

        //执行用户回调
        if (s_gpio_callbacks[idx]) {
            s_gpio_callbacks[idx]();
        }
        // 在本地变量中清除这一位，准备处理下一个
        pending &= ~(1UL << idx);
    }
}


//原厂测试代码
void gpio_init(void)
{
#if DEBUG_HW
#if(DEBUG_HW_DIGITAL)
    SYS_REG0X0C = (0x01 << 5) + (0x01 << 4) + (4);
#else
    SYS_REG0X0C |= (0x01 << 4);
    SYS_REG0X0C |= (0x01 << 5);
#endif    
    gpio_config(Port_Pin(2,0), GPIO_SC_FUN, GPIO_PULL_NONE); // port2, pin0
    gpio_config(Port_Pin(2,1), GPIO_SC_FUN, GPIO_PULL_NONE); // port2, pin1
    gpio_config(Port_Pin(2,2), GPIO_SC_FUN, GPIO_PULL_NONE);
    gpio_config(Port_Pin(2,3), GPIO_SC_FUN, GPIO_PULL_NONE);
    gpio_config(Port_Pin(2,4), GPIO_SC_FUN, GPIO_PULL_NONE);
    gpio_config(Port_Pin(2,5), GPIO_SC_FUN, GPIO_PULL_NONE);
    gpio_config(Port_Pin(2,6), GPIO_SC_FUN, GPIO_PULL_NONE);
    gpio_config(Port_Pin(2,7), GPIO_SC_FUN, GPIO_PULL_NONE); // port2, pin7
#if(!DEBUG_HW_DIGITAL)    
    SYS_REG0X0C |= (0x01 << 3);
    SYS_REG0X0C &= ~(0x07 << 0);
    SYS_REG0X0C |= (0x07 << 0);
#endif
#endif

    gpio_config(Port_Pin(0,4), GPIO_OUTPUT, GPIO_PULL_NONE);
    gpio_config(Port_Pin(0,5), GPIO_OUTPUT, GPIO_PULL_NONE);
    gpio_config(Port_Pin(0,6), GPIO_OUTPUT, GPIO_PULL_NONE);
    gpio_config(Port_Pin(0,7), GPIO_OUTPUT, GPIO_PULL_NONE);

    REG_GPIO_INT_EN = 0;
}
void DEBUG_MSG(uint8_t x)
{
#if GPIO_DBG_MSG
    addAON_GPIO_Reg0xe=0x00;
    addAON_GPIO_Reg0x2=(x&0x01)<<1;
    addAON_GPIO_Reg0x3=(x&0x02);
    addAON_GPIO_Reg0x4=(x&0x04)>>1;
    addAON_GPIO_Reg0x5=(x&0x08)>>2;
    addAON_GPIO_Reg0x6=(x&0x10)>>3;
    addAON_GPIO_Reg0x7=(x&0x20)>>4;
    addAON_GPIO_Reg0xc=(x&0x40)>>5;
    addAON_GPIO_Reg0xd=(x&0x80)>>6;
    addAON_GPIO_Reg0xe=0x02;
#endif
}
