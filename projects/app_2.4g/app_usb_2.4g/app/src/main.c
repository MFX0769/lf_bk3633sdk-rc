/**
 ****************************************************************************************
 *
 * @file arch_main.c
 *
 * @brief Main loop of the application.
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ****************************************************************************************
 */
 //#define __AES_TEST__
// #define __USB_TEST__
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include <string.h>   // boolean definition
#include "intc.h"      // Interrupt initialization
#include "my_drv_uart.h"      // UART initialization
#include "my_drv_uart2.h"      // UART2 initialization
#include "flash.h"     // Flash initialization
#include "app.h"       // application functions
#include "reg_access.h"
#include "boot.h"
#include "dbg.h"
#include "icu.h"
#include "user_config.h"
#include "drv_gpio.h"
#include "wdt.h"
#include "spi.h"
#include "adc.h"


#include "aon_rtc.h"
#include "rf.h"
#ifdef __AES_TEST__
#include "aes.h"
#endif
#if(USB_DRIVER)
#include "usb.h"
#include "driver_icu.h"
#endif
#include "Application_mode.h"
#include "rf_handler.h"
#include "driver_timer.h"
#include "rf.h"
#include "timer_handler.h"
#include "bat_protocol.h"
#include "rf_pair.h"
#include "addr_pool.h"
#include "gpio_init.h"
#include "app_sleep.h"
#include "app_key_scan.h"
#include "rf_addr_mgr.h"


extern void  xvr_reg_initial_24(void);

static void stack_integrity_check(void)
{
    if ((REG_PL_RD(STACK_BASE_UNUSED)!= BOOT_PATTERN_UNUSED))
    {
        while(1)
        {
            uart_printf("Stack_Integrity_Check STACK_BASE_UNUSED fail!\r\n");
        }
    }

    if ((REG_PL_RD(STACK_BASE_SVC)!= BOOT_PATTERN_SVC))
    {
        while(1)
        {
            uart_printf("Stack_Integrity_Check STACK_BASE_SVC fail!\r\n");
        }
    }

    if ((REG_PL_RD(STACK_BASE_FIQ)!= BOOT_PATTERN_FIQ))
    {
        while(1)
        {
            uart_printf("Stack_Integrity_Check STACK_BASE_FIQ fail!\r\n");
        }
    }

    if ((REG_PL_RD(STACK_BASE_IRQ)!= BOOT_PATTERN_IRQ))
    {
        while(1)
        {
            uart_printf("Stack_Integrity_Check STACK_BASE_IRQ fail!\r\n");
        }
    }

}

void platform_reset(uint32_t error)
{

    uart_printf("reset error = %x\r\n", error);
    // Disable interrupts
    GLOBAL_INT_STOP();

    cpu_reset();

}

#ifdef __AES_TEST__
extern uint8_t aes_ok;
extern uint8_t encrypted_data[16];

uint8_t value[16]={0x00, 0x01 ,0x02 ,0x03 ,0x04 ,0x05, 0x06 ,0x07 ,0x08,
    0x09 ,0x0A ,0x0B, 0x0C ,0x0D, 0x0E ,0x0F};
uint8_t key[16]={0xED, 0x8D, 0x9C, 0x50, 0xD1, 0x55, 0xAA, 0x1E, 0x4D, 0x8F,
    0xF5, 0x81, 0x13, 0x32, 0x4F, 0x04, };
uint8_t encrypted_data[16]={0x37, 0x37, 0x79, 0x52, 0x26, 0x2E, 0x35, 0x08,
    0xAA, 0x12, 0x32, 0x2D, 0x99, 0xB3, 0xEC, 0x36 };

#endif
#if(USB_DRIVER)

extern void DelayNops_usb(volatile unsigned long nops);
void usb_mod_enable(int en_dis){
    if(en_dis){//usb modal enable
        setf_SYS_Reg0x3_usb_pwd;
        DelayNops_usb(500);
        clrf_SYS_Reg0x3_usb_pwd;
    }
    else{
        setf_SYS_Reg0x3_usb_pwd;
    }
}

void usb_mod_ie_enable(int en_dis){
    if(en_dis){
        ICU_INT_ENABLE_SET(ICU_INT_ENABLE_IRQ_USB_MASK);
    }else{
        ICU_INT_ENABLE_CLEAR(ICU_INT_ENABLE_IRQ_USB_MASK);
    }
}

int mouseTest=0;

void Hid_RxCbk(void*ptr,int sz){
    int i;
    char*dat=(char*)ptr;
    uart_printf("recieved a hid data = %x\r\n",sz);
    for(i=0;i<sz;i++)
        uart_printf("%x,",dat[i]);
    uart_printf("\r\n");
/*    if(((char*)ptr)[0]==0x10){
        uart_printf("recieved a hid data\r\n");
    }
    AplUsb_StartTx(USB_ENDPID_Hid_DBG_IN, ptr, sz);*/
//    print("Ep3 Rx Data",ptr,sz);
}

//volatile uint8_t b_isDataing = FALSE;
uint8_t user_flash_data1[16] = {0x12,0x34,0xaa,0x00,0x00,0x20,0x00,0x00,
                                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,};
void Ep2_RxCbk(void*ptr,int sz){
//    int i;
    /*�iif(((char*)ptr)[0]==0x10){
        mouseTest=1;
        uart_printf("recieved a cmd\r\n");
    }
    AplUsb_StartTx(USB_ENDPID_Hid_DBG_IN, ptr, sz);
    
    
//    print("Ep3 Rx Data",ptr,sz);
    */
        char * pBuffer;
        pBuffer = (char*)ptr;
        uart_printf("dwLength: %d  \r\n",sz);
        for(int i=0;i<sz;i++)
        {
            uart_printf("%2x ",pBuffer[i]);
        }
        uart_printf("\r\n");
        //01 E0 FC 02 0E A5
        if((pBuffer[0] == 0x01) && (pBuffer[4] == 0x0e) &&(pBuffer[5] == 0xa5)   &&(pBuffer[1] == 0xe0)
                &&(pBuffer[2] == 0xfc) &&(pBuffer[3] == 0x02) )
        {
            flash_wp_256k();
            ///flash_erase(0,0x7ff00,16,NULL);
            flash_write(0,0x7d000,16,user_flash_data1,NULL);
            Delay_us(1000);
            #if 0      
            Delay_us(1000);
            memset(user_flash_data1,0x00,16);
            flash_read(0,0x7ff00,16,user_flash_data1,NULL);

            for(uint8_t i=0;i<16;i++)
            uart_printf("user_flash_data1[%x]=%x\n",i,user_flash_data1[i]);
            #endif                       
            platform_reset(0x1515);
        }

}

void MicIn_cbk(void*ptr,int sz){
    signed short int src[16];
    memset(src,0,sizeof(src));
    int i;
    for(i=0;i<sizeof(src)/sizeof(src[0]);i++){
        src[i]=i*0x100+i;
    }
    memcpy(ptr,src,sizeof(src));
    AplUsb_StartTx(USB_ENDPID_Audio_MIC,src,sizeof(src));
//     uart_send("RX",2);
}
void print(void*msg,void*dat,int len){
    char*ptr=(char*)dat;
    int i;
    uart_printf(msg);
    uart_printf("[%d]={\r\n\t",len);
    for(i=0;i<len;i++){
        uart_printf("%.2x ",ptr[i]);
        if((i&0x07)==0x07)uart_printf("\r\n\t");
    }
    uart_printf("\r\n}\r\n");
}


void AudioOut_Cbk(void*ptr,int sz){
    static int cnt=0;
//     uart_send(&cnt,1);
//     uart_send(&sz,1);

    uart_printf("%dth %dB:\r\n",cnt&0xff,sz);
    //print("data",ptr,8);
    cnt++;
//    memset(ptr,0,sz);
}
#endif

uint8_t pair_flag=0;
void app_key_event_handler(key_id_t id, key_event_t event)
{
    // 1. Handle Left Key (All States Print)
    if (id == KEY_ID_LEFT) {
        switch(event) {
            case KEY_EVT_DOWN:          uart_printf("Key Left: DOWN (Edge Detect)\r\n"); break;
            case KEY_EVT_UP:            uart_printf("Key Left: UP (Edge Detect)\r\n"); break;
            case KEY_EVT_SHORT_PRESS:   uart_printf("Key Left: SHORT PRESS\r\n"); break;
            case KEY_EVT_LONG_PRESS:    uart_printf("Key Left: LONG PRESS (Power On/Off Trigger)\r\n"); break;
            case KEY_EVT_LONG_HOLD:     /* uart_printf("Key Left: LONG HOLDING...\r\n"); */ break;
            case KEY_EVT_LONG_RELEASE:  uart_printf("Key Left: LONG RELEASE\r\n"); break;
            default: break;
        }
    } 
    // 2. Handle Right Key (5 Short Presses)
    else if (id == KEY_ID_RIGHT) {
        if (event == KEY_EVT_SHORT_PRESS) {
            static uint8_t right_cnt = 0;
            static uint32_t right_last_time = 0;
            uint32_t now = Get_SysTick_ms();
            // Reset count if interval > 1s
            if (now - right_last_time > 1000) {
                right_cnt = 0;
            }
            right_cnt++;
            right_last_time = now;
            
            if (right_cnt >= 5) {
                //翻转pair_flag
                pair_flag=!pair_flag;
                //if(pair_flag)
                    uart_printf("Enter Pairing\r\n");
                //else
                    uart_printf("Exit Pairing\r\n");

                right_cnt = 0;
            }
        }
    }
}


int main(void)
{
    
    icu_init(); //不能用串口打印，还没初始化

    //wdt_disable();
    intc_init(); //不能用串口打印，还没初始化！
    
    #if(UART_PRINTF_ENABLE)
        #if(!USB_DRIVER)
        #endif

        uart2_init(115200);
        uart_init(115200);
    #endif
    uart_printf("init uarts\r\n");

    flash_init();
    uart_printf("init flash\r\n");

    rf_addr_mgr_init();
    uart_printf("init rf_addr_mgr\r\n");

    xvr_reg_initial();
    #ifdef __USB_TEST__
    mcu_clk_switch(MCU_CLK_64M);
    #else
    mcu_clk_switch(MCU_CLK_16M);
    uart_printf("select MCU_CLK_16M\r\n");
    #endif   
    #if(AON_RTC_DRIVER)
    aon_rtc_init();
    uart_printf("init aon_rtc\r\n");
    #endif
    #if(SPI_DRIVER)
    spi_init(0,0,0);
    uart_printf("init spi\r\n");
    #endif

    #if(ADC_DRIVER)
    adc_init(1,1);
    uart_printf("init adc\r\n");
    #endif
    #if(USB_DRIVER)
    usb_init(usb_mod_enable,usb_mod_ie_enable);
    AplUsb_SetRxCbk(USB_ENDPID_Hid_DBG_OUT, (void*)Ep2_RxCbk);
    AplUsb_SetTxCbk(USB_ENDPID_Audio_MIC,(void*)MicIn_cbk);
    AplUsb_SetRxCbk(USB_ENDPID_Audio_SPK, (void*)AudioOut_Cbk);
    AplUsb_SetRxCbk(USB_ENDPID_Hid_MSE_OUT, (void*)Hid_RxCbk);
    uart_printf("init usb\r\n");
    #endif
    
    GLOBAL_INT_START();
    uart_printf("GLOBAL_INT_START\r\n");

    #ifdef __USB_TEST__
    uint8_t ms_buf[16];
    // send media key ,no id,the another endpoint
    gpio_config(0x10,0,0);
     ms_buf[0]= 0x00;
     ms_buf[1]= 0x00;
     ms_buf[2] = 0;
     ms_buf[3] = 0;
     ms_buf[4] = 0;
     ms_buf[5] = 0;
     ms_buf[6] = 0;
     ms_buf[7] = 0;
     ms_buf[8] = 0;
     ms_buf[9] = 0;

    while(1)
    {
        if(gpio_get_input(0x10)==0)
        {
            if(ms_buf[0]== 0)
            {
                ms_buf[0]= 0x02;
                AplUsb_StartTx(USB_ENDPID_Hid_MKEY, ms_buf, 2);
                uart_printf("send R \r\n");
            }
        }
        else
        {
            if(ms_buf[0]== 0x02)
            {
            ms_buf[0]= 0x00;
            AplUsb_StartTx(USB_ENDPID_Hid_MKEY, ms_buf, 2);
            uart_printf("release \r\n");
            }
        }
        Delay_ms(100);
    }
    // send keyboard
    gpio_config(0x10,0,0);
    ms_buf[0]= 0x02;
    ms_buf[1]= 0x00;
    ms_buf[2] = 0;
    ms_buf[3] = 0;
    ms_buf[4] = 0;
    ms_buf[5] = 0;
    ms_buf[6] = 0;
    ms_buf[7] = 0;
    ms_buf[8] = 0;
    ms_buf[9] = 0;

    while(1)
    {
        if(gpio_get_input(0x10)==0)
        {
            if(ms_buf[1]== 0)
            {
                ms_buf[1]= 0x02;
                AplUsb_StartTx(USB_ENDPID_Hid_MSE, ms_buf, 9);
                uart_printf("send R \r\n");
            }
        }
        else
        {
            if(ms_buf[1]== 0x02)
            {
                ms_buf[1]= 0x00;
                AplUsb_StartTx(USB_ENDPID_Hid_MSE, ms_buf, 9);
                uart_printf("release \r\n");
            }
        }
        Delay_ms(100);
    }
      // send mouse
    gpio_config(0x10,0,0);
    ms_buf[0]= 0x01;
    ms_buf[1]= 0x00;
    ms_buf[2] = 0;
    ms_buf[3] = 0;
    ms_buf[4] = 0;
    ms_buf[5] = 0;
    ms_buf[6] = 0;
    ms_buf[7] = 0;
    ms_buf[8] = 0;
    ms_buf[9] = 0;

    while(1)
    {
        if(gpio_get_input(0x10)==0)
        {
            static int16_t deltaX,deltaY;
            //if(ms_buf[1]== 0)
            {
                static uint8_t dcount = 100;
                dcount++;
                if(dcount>=100)
                {
                    dcount = 0;
                    if ((deltaX == 0) && (deltaY == 4))
                    {
                        deltaX=4;
                        deltaY = 0;
                    }
                    else if ((deltaX == 4) && (deltaY == 0))
                    {
                        deltaX = 0;
                        deltaY = -4;
                    }
                    else if ((deltaX == 0) && (deltaY== -4))
                    {
                        deltaX = -4;
                        deltaY = 0;
                    }
                    else if((deltaX == -4) && (deltaY == 0))
                    {
                        deltaX = 0;
                        deltaY = 4;
                    }
                    else
                    {
                        deltaX = 4;
                        deltaY = 0;
                    }
                }
                ms_buf[1]= 0x02;
                ms_buf[2] = deltaX;
                ms_buf[3] = ((deltaY<<4)&0xf0)|((deltaX>>8)&0x0f);
                ms_buf[4] = deltaY>>4;
                AplUsb_StartTx(USB_ENDPID_Hid_MSE, ms_buf, 6);
                uart_printf("send R \r\n");
                Delay_ms(8);
            }
        }
        else
        {
            if(ms_buf[1]== 0x02)
            {
                ms_buf[1]= 0x00;
                AplUsb_StartTx(USB_ENDPID_Hid_MSE, ms_buf, 6);
                uart_printf("release \r\n");
            }
        }
        Delay_ms(100);
    }
    #endif
    #ifdef __AES_TEST__
    // aes test
    gpio_config(KEY_RIGHT,INPUT, PULL_HIGH);
    gpio_config(KEY_LEFT,INPUT, PULL_HIGH);
    gpio_config(KEY_MIDDLE,INPUT, PULL_HIGH);
    gpio_config(KEY_PAIR,INPUT, PULL_HIGH);
    while(1)
    {
        app_aes_test(key,value,encrypted_data);
        app_gpio_sleep();
        cpu_reduce_voltage_sleep();
        uart_printf("wake up\r\n");

        Delay_ms(1000);
    }
    while(1)
    {
        if(aes_ok)
        {
            aes_ok = 0;
            uart_printf("data=");
            for(i=0;i<16;i++)
                uart_printf("%x,",encrypted_data[i]);
            uart_printf("\r\n");
            while(1);
        }
    }
    #endif

    //定时器初始化(依赖xvr里初始化rc32k时钟，放在xvr初始化后面)
    Timer_Handler_Init();
    uart_printf("Timer_Handler_Init done\r\n");

    /*----------------------------测试按键功能--------------------------------------*/
     const key_config_t my_keys[] = {
        {KEY_ID_LEFT,   Port_Pin(0, 2), 2000, false}, // Left Key: 3s Long Press
        {KEY_ID_RIGHT,  Port_Pin(3, 1), 1000, false}, // Right Key: Default 1s
    };
    app_key_init(my_keys, sizeof(my_keys)/sizeof(key_config_t));
    app_key_register_callback(app_key_event_handler);
    
    uart_printf("Key Test Start...\r\n");
    RF_Handler_Init();//初始化RF句柄及队列
    
    #define is_host 1
    while(1) {

        while(1){
            uint8_t dest_addr[5] = {0xA0, 0xA0, 0xA0, 0xA0, 0xA0};
            uint8_t test_data[32] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14};
            //发包测试 
            RF_txQueue_Send(dest_addr,test_data, 16);
            RF_Service_Handler(&hrf);
            delay_ms(10);
        }

        static uint32_t last_scan_time = 0;
        if (Get_SysTick_ms() - last_scan_time >= 10) {
            last_scan_time = Get_SysTick_ms();
            app_key_scan(10);

            #if is_host
            Host_Pairing_Task(&pair_flag);
            #else
            Slave_Pairing_Task(&pair_flag);
            #endif
        }
    }


    /*------------------------------------测试队列性能------------------------------------*/
    // Key Test Init
   

    // gpio_config(Port_Pin(0,3), GPIO_OUTPUT, GPIO_PULL_NONE); 
    // gpio_config(Port_Pin(0,7), GPIO_OUTPUT, GPIO_PULL_NONE); 
    // gpio_set(Port_Pin(0,7),0);

    // while(1){
    //    //uart_printf("e:%d\r\n",Get_SysTick_ms());

    //    if(Get_SysTick_ms()>=1000*60){ //运行1分钟测试
    //           uart_printf("test over 1 min\r\n");
    //           while(1);
    //    }
    // }
    

    // uart_printf("e:%d\r\n",Get_SysTick_ms());
    // // cpu延时等串口寄存器发完
    // for(int i=0;i<10000;i++){
    //     __nop();
    // }

    // cpu_24_reduce_voltage_sleep();   //进入低电压睡眠
    // while(1){
    //     app_enter_deep_sleep_with_wakeup_by_rtc(100); //进入深度睡眠，5秒后被RTC唤醒
    //     uart_printf("wake up from sleep:%d\r\n",Get_SysTick_ms());
    // }

    

    // gpio_set(Port_Pin(0,3),1);
    // gpio_set(Port_Pin(0,3),0);
    // Rf_rxQueueItem_t temp;
    // temp.pipes = 0;
    // temp.len = 20;
    // //gpio_set(Port_Pin(0,3),1);
    // for(int i=0;i<128;i++)
    //     queue_push_overwrite(&rf_rxQueue, (Rf_rxQueueItem_t*)&temp);
    // //gpio_set(Port_Pin(0,3),0);  
    // //delay_ms(5); 
    // gpio_set(Port_Pin(0,3),1); 
    // for(int i=0;i<1;i++)
    //     queue_pop(&rf_rxQueue, (Rf_rxQueueItem_t*)&temp);
    // gpio_set(Port_Pin(0,3),0); 
    // //delay_ms(10);
    
    //     while(1){
    //         gpio_set(Port_Pin(0,3),1);
    //         // delay_ms(10);
    //         // gpio_set(Port_Pin(0,3),0);
    //         // delay_ms(20);
    //     }


    /* ------------------------------------双通道收发-------------------------------------- */
    //使用pipe0接收ACK
    uint8_t pipe0_addr[5] = {0xA0, 0xA0, 0xA0, 0xA0, 0xA0};
    uint8_t pipe1_addr[5] = {0xA1, 0xA1, 0xA1, 0xA1, 0xA1};
     RF_Handler_Init();//初始化RF句柄及队列
     //printf_all_registers();
     HAL_RF_SetTxMode(&hrf);//设置为发送模式
     HAL_RF_SetTxPower(&hrf, RF_TX_POWER_N4p6_dBm);//设置发送功率
     uint32_t txcount=0;
     uint32_t txcount1=0;
     uint8_t test_send_data0[5];
     uint8_t test_send_data1[14];
    for(int i=0;i<5;i++) test_send_data0[i]=i;
    for(int i=0;i<14;i++) test_send_data1[i]=i;
    set_power(RF_TX_POWER_N8p6_dBm); //设置发射功率为-8.6dBm
    while(1) //发送
    {
        txcount++;
        txcount1++;

        //发10次pipe0,再换地址发10次pipe1
        if(txcount<=10){
            test_send_data0[0]=txcount;
            HAL_RF_SetTxAddress(&hrf, pipe0_addr, 5);//设置发送地址为pipe0地址
            //HAL_RF_SetRxAddress(&hrf,0, pipe0_addr, 5);//设置发送地址为pipe0地址
            RF_txQueue_Send(pipe0_addr,test_send_data0, sizeof(test_send_data0));//测试发送数据入队
        }
        else{ //换地址发！

            test_send_data1[0]=txcount-10;
            HAL_RF_SetTxAddress(&hrf, pipe1_addr, 5);//设置发送地址为pipe1地址
            //HAL_RF_SetRxAddress(&hrf,0, pipe1_addr, 5);//设置发送地址为pipe1地址
            RF_txQueue_Send(pipe1_addr,test_send_data1, sizeof(test_send_data1));//测试发送数据入队

            if(txcount>25) {
                txcount=0;
            }   
        }
        //只发送240次，测试
        if(txcount1>=100){
            while(1){
                uart_printf("send all count=%d \r\n", txcount1);

                //app_enter_deep_sleep_with_wakeup_by_rtc(1000);
                //__HAL_RF_PowerDown();
                //delay_ms(10000);
            }
        }
        delay_ms(100);
        RF_Service_Handler(&hrf);

        //
       
    }

    HAL_RF_SetRxMode(&hrf);//设置为接收模式
    HAL_RF_SetRxAddress(&hrf, 0, pipe0_addr, 5);//设置pipe0地址
    HAL_RF_SetRxAddress(&hrf, 1, pipe1_addr, 5);//设置pipe1地址
    uint32_t rx_cnt=0;
    //printf_all_registers();
    while(1)//接收                                                
    {
        uint8_t* rec_data;
        uint8_t  out_len;
        uint8_t  pipes;
        gpio_set(Port_Pin(0,3),1);
        if(RF_rxQueue_Recv(&rec_data, &out_len, &pipes)!=0){
            gpio_set(Port_Pin(0,3),0);
            rx_cnt++;
            //uart_printf("len=%d, pipe=%d,data0=%d \r\n", out_len, pipes,rec_data[0]);
            uart_printf("%d %d %d %d\r\n",rec_data[0], rx_cnt,rf_int_count_rxdr,rf_rxQueue.overwrite_cnt);
        }
        delay_ms(15);
    }


    /* ------------------------------------配对测试,过-------------------------------------- */

    // Save_Pair_Info((uint8_t *)"\xDE\xAD\xBE\xEF\x01");
    //Load_Pair_Info();

    /* 非阻塞slave */
    //my_key_init();
    // RF_Handler_Init();     //初始化RF句柄及队列
    // Slave_Pairing_Start(); //启动从机配对模式
    // while(1)
    // {
    //     Slave_Pairing_Task(&pair_flag); //非阻塞配对任务调用
    //     //delay_ms(10);
    //     //RF_Service_Handler(&hrf);  
    // }
    /* 非阻塞host */
    // RF_Handler_Init();//初始化RF句柄及队列
    // while(1)
    // {
    //     Host_Pairing_Task(&pair_flag); //非阻塞配对任务调用
    // }
    

   //按键+阻塞式配对测试
    // while(1)
    // {   
    //     static uint16_t cnt_last=0;
    //     if(cnt!=cnt_last){
    //         cnt_last=cnt;
    //         uart_printf("Key Pressed! Count: %d\r\n",cnt_last);
    //     }

    //     //按下5s进入配对模式，非阻塞判断
    //     static uint32_t press_start_time = 0;
    //     static uint8_t key_stable = 0;

    //     if(gpio_get_input(Port_Pin(1,0))==0){
    //         if(key_stable == 0){
    //             //第一次检测到低电平，开始消抖
    //             delay_ms(20);
    //             if(gpio_get_input(Port_Pin(1,0))==1){
    //                 //抖动，放弃
    //                 continue;
    //             }
    //             // 消抖成功，记录时间
    //             key_stable = 1;
    //             press_start_time = Get_SysTick_ms();
    //         }else{
    //             // 已经稳定为低，累计时间
    //             if((Get_SysTick_ms()-press_start_time)>=5000){
    //                 uart_printf("Enter Pairing Mode!\r\n");
    //                 //Do_Pairing_As_Host_SM();
    //                 Do_Pairing_As_slave_SM(); //传入0表示开始配对
    //                 key_stable = 0; //防止重复进入
    //             }
    //         }
    //     }else{
    //         key_stable = 0; //松开或抖动时重置
    //     }
    
    //    //Do_Pairing_As_Host_SM();
    //    //Do_Pairing_As_slave_SM();
    // }
    
    
    // Do_Pairing_As_Host();

    

    /* --------------------------------------简单RF收发测试---------------------------- */
    //  static uint32_t txcount=0;
    //  RF_Handler_Init();//初始化RF句柄及队列
    //  printf_all_registers();
    //  HAL_RF_SetTxMode(&hrf);//设置为发送模式
    //  while(1) //发送
    //  {
    //     txcount++;
    //     test_sand_data[0] = txcount;
    //     if(txcount<=10){
    //         RF_txQueue_Send(test_sand_data, 32);//测试发送数据入队
    //         RF_Service_Handler(&hrf);  //处理发送队列，卡死在这里
    //         uart_printf("send data count=%d\r\n", txcount);
    //     }
    //     else{ //换地址发！
    //         uint32_t new_addr[5] = {0xD2, 0xD2, 0xD2, 0xD2, 0xD2};
    //         HAL_RF_SetTxAddress(&hrf, new_addr, 5);
    //         HAL_RF_SetRxAddress(&hrf, 0, new_addr, 5);
    //         uart_printf("change addr\r\n");
    //         uart_printf("TRX_RX_ADDR_P0 = ");
    //         for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    //         uart_printf("\r\n");

    //         uart_printf("TRX_TX_ADDR    = ");
    //         for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    //         uart_printf("\r\n");

    //         RF_txQueue_Send(test_sand_data, 32);//测试发送数据入队
    //         RF_Service_Handler(&hrf);  //处理发送队列，卡死在这里
    //     }
    //     delay_ms(1000);
    // }
    
    // HAL_RF_SetRxMode(&hrf);//设置为接收模式
    // while(1)//接收
    // {
    //     txcount++;
    //     uint8_t* rec_data;
    //     uint8_t  out_len;
    //     if(RF_rxQueue_Recv(&rec_data, &out_len)!=0){
    //         uart_printf("rec_data=");
    //         for(int i=0;i<out_len;i++)
    //             uart_printf("%x,",rec_data[i]);
    //         uart_printf("\r\n");
    //     }
    //     Delay_ms(1);
    // }

    /*----------------------------电池透传逻辑----------------------------*/
    RF_Handler_Init();//初始化RF句柄及队列
    uint8_t rec_data[32];
    //简易调度器，根据时间戳调用task
    uart_printf("enter main loop\r\n");
    uart0_printf("enter main loop\r\n");
    while(1){
        //任务调用周期分别为 5ms 10ms 20ms 50ms
        static uint32_t last_timestamp[10] = {0};

        //主逻辑状态机
        if((Get_SysTick_ms() - last_timestamp[0]) >= 2){
            //5ms任务
            //串口收到电池数据解包
            Protocol_ParseByte(&uart0_rxQueue);
            
            last_timestamp[0] = Get_SysTick_ms();
        }
        if((Get_SysTick_ms() - last_timestamp[1]) >= 5){
            last_timestamp[1] = Get_SysTick_ms();
        }

        if((Get_SysTick_ms() - last_timestamp[2]) >= 20){
            //20ms任务
            last_timestamp[2] = Get_SysTick_ms();
        }

        if((Get_SysTick_ms() - last_timestamp[3]) >= 50){
            //50ms任务
            last_timestamp[3] = Get_SysTick_ms();
        }

        //100ms任务
        if((Get_SysTick_ms() - last_timestamp[4]) >= 100){
            RF_Service_Handler(&hrf);
            last_timestamp[4] = Get_SysTick_ms();
        }

        //200ms任务
        if((Get_SysTick_ms() - last_timestamp[5]) >= 200){
            uint8_t *rec_data_p;
            uint8_t len;

            if(RF_rxQueue_Recv(&rec_data_p, &len, NULL) == 1 &&
                len == sizeof(Bat_Soc_t) ){

                uint16_t crc = crc16_modbus(rec_data_p, len - 2);
                Bat_Soc_t bat_soc_tmp;
                memcpy(&bat_soc_tmp, rec_data_p, sizeof(Bat_Soc_t));
                uart_printf("calc crc=%x,recv crc=%x\r\n", crc, bat_soc_tmp.crc16);

                if(crc==bat_soc_tmp.crc16){
                    uart_printf("soc=%d%%", bat_soc_tmp.soc);
                    uart_printf("\r\n");
                }
                
            }
            last_timestamp[5] = Get_SysTick_ms();
        }
        //500ms任务
        if((Get_SysTick_ms() - last_timestamp[6]) >= 500){
            
            last_timestamp[6] = Get_SysTick_ms();
        }
        //1000ms任务
        if((Get_SysTick_ms() - last_timestamp[7]) >= 1000){
            last_timestamp[7] = Get_SysTick_ms();
            /* 接收解析透传信息 */

            // uart_printf("--------------1000ms-task---------------------- \r\n");
            // if(RF_rxQueue_Recv(rec_data,32)){
            //     uart_printf("rf_rec_data=");
            //     for(int i=0;i<32;i++)
            //         uart_printf("%x,",rec_data[i]);
            //     uart_printf("\r\n");
            // }

            // uart_printf("--------------500ms-task---------------------- \r\n");
            // uint16_t counts = queue_get_counts(&uart2_rxQueue);
            // uart_printf("uart2_rxQueue counts=%d\r\n", counts);
            // uart_printf("queue_datas=: ");
            // for(int i=0;i<counts;i++)
            // {
            //     uint8_t data;
            //     queue_peek_at(&uart2_rxQueue, i, &data);
            //     uart_printf("%02X ", data);
            // }
            // uart_printf("\r\n");
            uart_printf("pair cnt=%d\r\n", cnt);
        }
    }

/*-----------------------------------串口测试---------------------------*/
    while(1)
    {
        //uart_printf("in main loop\r\n");
        //串口queue测试
        uart_printf("tick=%d\r\n", Get_SysTick_ms());

        if(!queue_is_empty((&uart2_rxQueue)))
        {
            uint8_t byte;
            queue_pop(&uart2_rxQueue, &byte);
            uart_printf("%d\r\n", byte);
        }

    }
    
   
    while(1)
    {
        stack_integrity_check();
    }
}






