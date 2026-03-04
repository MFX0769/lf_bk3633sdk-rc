#include "intc.h"            // interrupt controller
#include "my_drv_uart.h"            // uart definitions
#include "BK3633_Reglist.h"
// #include "gpio.h"
#include "drv_gpio.h"
#include "spi.h"
#include "i2c.h"
#include "icu.h"
#include "dma.h"
#include "reg_intc.h"
#include "driver_timer.h"
#include "app.h"
#include "hal_drv_rf.h"
#include "test_my_rf_fun.h"
#include "rf_handler.h"


#if(ADC_DRIVER)
#include "adc.h"
#endif
#if(UART_DRIVER)
#include "my_drv_uart.h"
#endif
#if(UART2_DRIVER)
#include "my_drv_uart2.h"
#endif
#if(AON_RTC_DRIVER)
#include "aon_rtc.h"
#endif
#if (USB_DRIVER)
#include "driver_usb.h"
#endif
/*
 * DEFINES
 ****************************************************************************************
 */

#define RWBT_INT      CO_BIT(INTC_BT)
#define RWBTDM_INT    CO_BIT(INTC_BTDM)
#define UART_INT      CO_BIT(INTC_UART)
#define UART_2_INT    CO_BIT(INTC_UART_2)

#define RWBLE_INT     CO_BIT(INTC_BLE)
#define RWCOMMON_INT  CO_BIT(INTC_COMMON)
#define TIMER_INT     CO_BIT(INTC_TIMER)

extern void bk24_isr();
// enable the supported interrupts
#define PLF_INT     (UART_INT | UART_2_INT | TIMER_INT)




void intc_init(void)
{
    addSYS_Reg0x10 = 0; // int enable 0:disable 1::enable
    addSYS_Reg0x11 = 0; // priority; 0: irq  1:fiq

    //setf_SYS_Reg0x10_int_uart0_en; //enable uart_int irq
    //setf_SYS_Reg0x10_int_timer0_en; //enable timer_int irq

  //  setf_SYS_Reg0x10_int_rwble_en; //enable int rwble
  //  setf_SYS_Reg0x10_int_rwdm_en; //enable int rwdm
  //  setf_SYS_Reg0x10_int_rwbt_en; //enable  rwbt
  // setf_SYS_Reg0x11_int_rwble_pri; // 1:fiq
  //  setf_SYS_Reg0x11_int_rwdm_pri; // 1:fiq
  //  setf_SYS_Reg0x11_int_rwbt_pri; // 1:fiq
  //  setf_SYS_Reg0x10_int_dma_en;

  
  setf_SYS_Reg0x10_int_timer1_en; //enable timer_int irq
  clrf_SYS_Reg0x11_int_timer1_pri; //timer系统级中断设为irq

  setf_SYS_Reg0x10_int_uart0_en; //enable uart0_int irq
  clrf_SYS_Reg0x11_int_uart0_pri; //uart.系统级中断设为irq

  setf_SYS_Reg0x10_int_uart1_en; //enable uart1_int irq
  clrf_SYS_Reg0x11_int_uart1_pri; //uart1系统级中断设为irq

  setf_SYS_Reg0x10_int_bk24_en; //使能bk24系统级中断
  //setf_SYS_Reg0x11_int_bk24_pri; //bk24系统级中断设为fiq
  clrf_SYS_Reg0x11_int_bk24_pri;

  setf_SYS_Reg0x10_int_aon_gpio_en; //使能
  clrf_SYS_Reg0x11_int_aon_gpio_pri; //aon_gpio系统级中断设为irq

  setf_SYS_Reg0x10_int_aon_rtc_en; //使能aon rtc系统级中断
  clrf_SYS_Reg0x11_int_aon_rtc_pri; //aon rtc系统级中断设为irq

}



/*__IRQ */void intc_irq(void)
{

    uint32_t IntStat;
    uint32_t irq_status = 0; //记录挂起位，方便最后统一清除

    //cpu_wakeup();
    cpu_24_wakeup();
    
    IntStat = intc_status_get();

    if(IntStat & INT_BK24_bit)
    {
        //uart_printf("in intc_fiq\n");
        irq_status |= INT_BK24_bit;
        HAL_RF_IRQ_Handler(&hrf);
    }

    // call the function handler
    if(IntStat & INT_STATUS_UART0_bit)
    {
        //uart_printf("in ua0\r\n");
        irq_status |= INT_STATUS_UART0_bit;
        uart_isr();
    }
    if(IntStat & INT_STATUS_UART2_bit)
    {
        //uart2_printf("in ua2\r\n");
        irq_status |= INT_STATUS_UART2_bit;
        uart2_isr();
    }
#if (ADC_DRIVER)
    if(IntStat & INT_STATUS_ADC_bit)
    {
        irq_status |= INT_STATUS_ADC_bit;
        adc_isr();
    }
#endif
#if (AON_RTC_DRIVER)
    if(IntStat & INT_STATUS_AON_RTC_bit)
    {
        irq_status |= INT_STATUS_AON_RTC_bit;
        aon_rtc_isr();
    }
#endif


#if (GPIO_DRIVER)
    if(IntStat & INT_STATUS_AON_GPIO_bit)
    {
        irq_status |= INT_STATUS_AON_GPIO_bit;
        gpio_isr();
    }
#endif
#if (SPI_DRIVER)
    if(IntStat & INT_STATUS_SPI0_bit)
    {
        irq_status |= INT_STATUS_SPI0_bit;
        spi_isr();
    }
#endif
#if (I2C_DRIVER)
    if(IntStat & INT_STATUS_I2C0_bit)
    {
        irq_status |= INT_STATUS_I2C0_bit;
        i2c_isr();
    }
#endif

    if(IntStat & INT_STATUS_TMR0_bit)
    {
        irq_status |= INT_STATUS_TMR0_bit;
        Timer_ISR0();
    }
    if(IntStat & INT_STATUS_TMR1_bit)
    {
        irq_status |= INT_STATUS_TMR1_bit;
        Timer_ISR1();
    }
#if (PWM_DRIVER)
    if(IntStat & INT_STATUS_PWM0_bit)
    {
        irq_status |= INT_STATUS_PWM1_bit;
        pwm_isr();
    }
    if(IntStat & INT_STATUS_PWM1_bit)
    {
        irq_status |= INT_STATUS_PWM2_bit;
        pwm_isr();
    }

#endif

#if(USB_DRIVER)
    if(IntStat & INT_STATUS_USB_bit)
    {

        irq_status |= INT_STATUS_USB_bit;
        USB_InterruptHandler();
    }
#endif

    intc_status_clear(irq_status);

    //uart_printf("0x%08X\r\n", intc_status_get());
}

/*__FIQ */void intc_fiq(void)
{
    uint32_t IntStat;
    uint32_t fiq_status=0;

    IntStat = intc_status_get();

    if(IntStat & INT_STATUS_RWDM_bit)
    {
        fiq_status |= INT_STATUS_RWDM_bit;
        rwip_isr();
    }

    if(IntStat & INT_STATUS_DMA_bit)
    {
        fiq_status |= INT_STATUS_DMA_bit;
        dma_isr();
    }

    // if(IntStat & INT_BK24_bit)
    // {
    //     uart_printf("in intc_fiq\n");
    //     //fiq_status |= INT_BK24_bit;
    //     intc_status_clear(INT_BK24_bit);//先清除中断标志，防止丢失中断
    //     HAL_RF_IRQ_Handler(&hrf);
    // }

    intc_status_clear(fiq_status);
}

void Undefined_Exception(void)
{

}
void SoftwareInterrupt_Exception(void)
{


}
void PrefetchAbort_Exception(void)
{


}
void DataAbort_Exception(void)
{


}

void Reserved_Exception(void)
{

}


#pragma ARM//不要修改以下两个文件
__IRQ FAST_IRQ_ENTRY void SYSirq_IRQ_Handler(void)
{
    intc_irq();
}

__FIQ FAST_FIQ_ENTRY void SYSirq_FIQ_Handler(void)
{
    intc_fiq();
}


/// @} INTC
