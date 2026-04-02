/**
 ****************************************************************************************
 * @file rc_scheduler.c
 * @brief Remote Control Task Scheduler
 *
 * 职责划分:
 *   1. 调度层: 管理各任务时序
 *   2. 通信层: RF收发/协议打包解析/seq跟踪
 *   3. 控制层: 油门/按键/档位逻辑
 *
 ****************************************************************************************
 */
#include "rc_scheduler.h"
#include "rc_protocol.h"
#include "bat_manage.h"
#include "app_sleep.h"
#include "app_key.h"
#include "app_throttle.h"
#include "rf_handler.h"
#include "rf_pair.h"
#include "rf_config.h"
#include "timer_handler.h"
#include "user_config.h"
#include "pinmap.h"
#include "debug.h"
#include <string.h>
#include "hal_drv_rf.h"
#include "drv_gpio.h"
#include "lcd_kmg.h"
#include "BK3633_RegList.h"
#include "reg_intc.h" 
extern RF_HandleTypeDef hrf;
#define SCHE_LOG_EN 1
#if (SCHE_LOG_EN)
    #define SCHE_LOG uart_printf
#else
    #define SCHE_LOG(...)  ((void)0) 
#endif
/* ======================== 电池管理对象 ======================== */
static bat_manager_t s_bat;

static const bat_hw_config_t s_bat_hw = {
    .pwr_en_gpio     = SYS_PWR_EN,
    .pwr_key_gpio    = KEY_SYS_PWR,
    .adc_channel     = 2,            // ADC Channel 2
    .adc_pwr_en_gpio = ADC_VBAT_PWR_EN,
    .pgood_gpio      = CHG_PGOOD,
    .chrg_gpio       = CHG_CHRG,
    .divider_ratio   = 0.207f,       // 47/(180+47)
    .bat_empty_mv    = 3300,         // 空电 3.3V
    .bat_full_mv     = 4200,         // 满电 4.2V
};

/* ======================== 通信层(static) ======================== */

static uint8_t            s_esc_addr[5];       /* 电控地址 */
static uint8_t            s_esc_paired;        /* 电控已配对标志 */
static esc_ctrl_data_t    s_ctrl;              /* 当前下发数据 */
static proto_tracker_t    s_tracker;           /* seq跟踪器(电控) */
static esc_status_data_t  s_esc_status;        /* 电控状态 */
static uint8_t            s_mc_online;         /* 电控在线标志 */

static uint8_t            s_bat_addr[5];       /* 电池地址 */
static uint8_t            s_bat_paired;        /* 电池已配对标志 */
static bat_status_data_t  s_ext_bat_status;    /* 外部电池状态 */
static uint8_t            s_ext_bat_online;    /* 外部电池在线标志 */
static uint8_t            s_bat_query_seq;     /* 电池查询seq */
static uint32_t           s_bat_last_rx_tick;  /* 最后收到电池回包的时间 */
#define BAT_ONLINE_TIMEOUT_MS  3000            /* 3s无回包视为离线 */

static uint8_t            s_ui_mode;           /* UI显示模式 */
static uint32_t           s_ui_result_tick;    /* 配对结果显示起始时间 */
#define UI_RESULT_SHOW_MS 2000                 /* 配对结果显示持续时间 */

/**
 * @brief 打包并发送控制帧(到电控)
 */
static void comm_send_ctrl_frame(void)
{
    uint8_t tx_buf[32];
    uint8_t seq = tracker_next_seq(&s_tracker);
    uint8_t len = proto_pack(tx_buf, CMD_ESC_CTRL, seq, &s_ctrl, sizeof(s_ctrl));

    RF_Send(s_esc_addr, tx_buf, len);
}

/**
 * @brief 发送电池查询帧(到电池)
 */
static void comm_send_bat_query(void)
{
    uint8_t tx_buf[32];
    s_bat_query_seq++;
    uint8_t len = proto_pack(tx_buf, CMD_BAT_QUERY, s_bat_query_seq, NULL, 0);

    RF_Send(s_bat_addr, tx_buf, len);
}

/**
 * @brief 处理接收队列，解析ACK payload
 */
static void comm_process_rx(void)
{
    const uint8_t *rx_data;
    uint8_t rx_len, pipes;

    while (RF_rxQueue_Recv(&rx_data, &rx_len, &pipes)) {
        uint8_t cmd, seq;
        const uint8_t *payload;
        uint8_t plen;

        if (proto_parse(rx_data, rx_len, &cmd, &seq, &payload, &plen) != 0)
            continue;

        switch (cmd) {
        case CMD_ESC_STATUS:
            if (plen == sizeof(esc_status_data_t)) {
                memcpy(&s_esc_status, payload, sizeof(esc_status_data_t));
                SCHE_LOG("speed: %d meter=%d\r\n", s_esc_status.speed, s_esc_status.mileage);
                s_mc_online = 1;
                tracker_on_ack(&s_tracker, seq);
            }
            break;
        case CMD_BAT_STATUS:
            if (plen == sizeof(bat_status_data_t)) {
                memcpy(&s_ext_bat_status, payload, sizeof(bat_status_data_t));
                s_ext_bat_online = 1;
                s_bat_last_rx_tick = Get_SysTick_ms();
                SCHE_LOG("power_battery: temp=%d soc=%d status=0x%02X\r\n",
                            s_ext_bat_status.temperature - 40,
                            s_ext_bat_status.soc,
                            s_ext_bat_status.status);
            }
            break;
        default:
            break;
        }
    }
}

/* ========================控制层======================== */

/**
 * @brief 更新控制数据并判断是否需要发送
 * @note 油门死区判断已在app_throttle内部完成
 */
static void control_update_and_send(void)
{
    /* 更新油门值 */
    uint8_t throttle_val, throttle_changed;
    app_throttle_update(&throttle_val, &throttle_changed);
    s_ctrl.throttle = throttle_val;

    /* 油门变化 → 标记pending */
    if (throttle_changed) {
        tracker_mark_pending(&s_tracker);
        SCHE_LOG("Throttle changed:%d\r\n", throttle_val);
    }

    /* pending期间持续发送，直到收到ACK确认 */
    if (tracker_is_pending(&s_tracker)) {
        comm_send_ctrl_frame();
    }
}

/**
 * @brief 从Flash加载所有已配对设备地址
 */
static void comm_load_all_addrs(void)
{
    /* 电控地址 */
    if (rf_config_read_device_addr(DEV_TYPE_ESC, s_esc_addr)) {
        s_esc_paired = 1;
        SCHE_LOG("ESC addr: %02X %02X %02X %02X %02X\r\n",
                    s_esc_addr[0], s_esc_addr[1], s_esc_addr[2], s_esc_addr[3], s_esc_addr[4]);
    } else {
        s_esc_paired = 0;
        SCHE_LOG("ESC not paired\r\n");
    }

    /* 电池地址 */
    if (rf_config_read_device_addr(DEV_TYPE_BATTERY, s_bat_addr)) {
        s_bat_paired = 1;
        SCHE_LOG("BAT addr: %02X %02X %02X %02X %02X\r\n",
                    s_bat_addr[0], s_bat_addr[1], s_bat_addr[2], s_bat_addr[3], s_bat_addr[4]);
    } else {
        s_bat_paired = 0;
        SCHE_LOG("BAT not paired\r\n");
    }
}

/* ======================== 调试：KEY1中断唤醒回调 ======================== */
static void key1_wakeup_isr(void)
{
    uart_printf("[KEY1_IRQ] TxSt=%d T1_ctrl=0x%X T1_cnt=%d T1_sys_en=%d intst=0x%X time:%d\r\n",
                hrf.TxState,
                addTIMER1_Reg0x3,       /* bit1=timer1_1_en, bit8=timer1_1_int */
                addTIMER1_Reg0x1,       /* timer1_1 当前计数/装载值 */
                get_SYS_Reg0x10_int_timer1_en,  /* 系统级timer1中断使能 */
                intc_status_get(),
                Get_SysTick_ms());     /* 系统中断状态 */
}

/* ======================== 调度层 ======================== */

void RC_Scheduler_Init(RC_Scheduler_t *sched)
{
    memset(sched, 0, sizeof(RC_Scheduler_t));

    app_key_init();

            
    /* 初始化电池管理并锁定电源 */
    bat_manage_init(&s_bat, &s_bat_hw);
    bat_manage_power_on(&s_bat);

    rf_config_load_from_flash();
    

    /* 初始化硬件 */
    RF_Handler_Init();
    app_throttle_init();

    /* 初始化通信层 */
    tracker_init(&s_tracker);
    s_ctrl.gear = 1;
    s_ctrl.mode = MODE_ASSIST;
    s_ctrl.throttle = 0;
    s_mc_online = 0;
    s_ext_bat_online = 0;
    s_bat_query_seq = 0;

    /* 从Flash加载所有已配对设备地址 */
    comm_load_all_addrs();

    /* 默认使用电控地址作为RF收发地址 */
    if (s_esc_paired) {
        HAL_RF_SetTxAddress(&hrf, s_esc_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, s_esc_addr, 5);
    } else {
        HAL_RF_GetRxAddress(&hrf, 0, s_esc_addr);
        SCHE_LOG("Using default addr\r\n");
    }

    sched->initialized = 1;
    SCHE_LOG("RC_Scheduler_Init done\r\n");

    /* 配置KEY1 GPIO中断，按下可唤醒卡死的sleep，用于调试 */
    gpio_config(KEY1, GPIO_INPUT, GPIO_PULL_LOW);
    gpio_int_enable(KEY1, GPIO_INT_EDGE_RISING, key1_wakeup_isr);
    SCHE_LOG("KEY1 wakeup IRQ enabled (gpio=%d)\r\n", KEY1);

        // gpio_config(KEY1,GPIO_INPUT,GPIO_PULL_LOW);
        // gpio_config(KEY2,GPIO_INPUT,GPIO_PULL_LOW);
        // gpio_config(Port_Pin(0,0),GPIO_FLOAT,GPIO_PULL_NONE); //uart关掉
        // gpio_config(Port_Pin(0,1),GPIO_FLOAT,GPIO_PULL_NONE);
        // while(1){
        //     //cpu_24_reduce_voltage_sleep();
        //     app_enter_sleep_with_wakeup_by_timer(10000, 1);
        // }

}

#if 0
#else
void RC_Scheduler_Task(RC_Scheduler_t *sched)
{
    if (!sched->initialized) return;

    SCHE_LOG("enter RC_Scheduler_Task\r\n");
    debug_print_rf_registers();

    uint32_t ts[8] = {0};
    static uint8_t allow_sleep_flag = 1;
    static uint8_t last_pair = 0;
    static uint8_t old_bat_addr[5], old_esc_addr[5];
    static uint8_t old_bat_paired, old_esc_paired;

    while (1) {
        
        /* ========== 40ms: 按键扫描 ========== */
        if (Get_SysTick_ms() - ts[0] >= 40) {
            ts[0] = Get_SysTick_ms();
            app_key_scan(40);
        }

        /* ========== 配对处理（始终调用，内部自行管理状态） ========== */
        uint8_t *pair_flag_ptr = app_key_get_pair_flag_ptr();
        Host_Pairing_Task(pair_flag_ptr);

        if (*pair_flag_ptr) { //如果配对标志被按键设置为1进入配对，记录last_pair状态
            if (!last_pair) {
                /* 刚进入配对，快照当前地址用于后续比较 */
                memcpy(old_bat_addr, s_bat_addr, 5);
                old_bat_paired = s_bat_paired;
                memcpy(old_esc_addr, s_esc_addr, 5);
                old_esc_paired = s_esc_paired;
            }
            last_pair = 1;
            s_ui_mode = UI_MODE_PAIRING;
            delay_ms(10);
            allow_sleep_flag = 0;
        } else {
            /* 配对刚结束，重新加载所有设备地址 */
            if (last_pair) {
                last_pair = 0;
                tracker_init(&s_tracker);
                comm_load_all_addrs();

                /* 判断配对结果：新配对 或 地址变化 */
                uint8_t bat_ok = s_bat_paired && (!old_bat_paired || memcmp(s_bat_addr, old_bat_addr, 5) != 0);
                uint8_t esc_ok = s_esc_paired && (!old_esc_paired || memcmp(s_esc_addr, old_esc_addr, 5) != 0);
                if (bat_ok || esc_ok) {
                    s_ui_mode = UI_MODE_PAIR_OK;
                } else {
                    s_ui_mode = UI_MODE_PAIR_FAIL;
                }
                s_ui_result_tick = Get_SysTick_ms();

                /* 恢复默认RF地址到电控 */
                if (s_esc_paired) {
                    HAL_RF_SetTxAddress(&hrf, s_esc_addr, 5);
                    HAL_RF_SetRxAddress(&hrf, 0, s_esc_addr, 5);
                }
            }

            /* 配对结果显示超时，回到Normal */
            if ((s_ui_mode == UI_MODE_PAIR_OK || s_ui_mode == UI_MODE_PAIR_FAIL)
                && (Get_SysTick_ms() - s_ui_result_tick >= UI_RESULT_SHOW_MS)) {
                s_ui_mode = UI_MODE_NORMAL;
            }
            allow_sleep_flag = 1;

            /* ========== 80ms: 电控通信 ========== */
            if (Get_SysTick_ms() - ts[2] >= 80) {
                ts[2] = Get_SysTick_ms();
                static uint8_t hb_cnt;
                hb_cnt++;
                //SCHE_LOG("[T1]%d\r\n", ts[2]);
                comm_process_rx();

                if (s_esc_paired) {
                    control_update_and_send();
                    //SCHE_LOG("[T1a]\r\n");

                    /* 心跳：800ms发一次保活 */
                    if (hb_cnt >= 10) {
                        hb_cnt = 0;
                        if (!tracker_is_pending(&s_tracker)) {
                            comm_send_ctrl_frame();
                        }
                    }
                }
                //SCHE_LOG("[T1b]\r\n");
            }

            /* ========== 800ms: 电池查询（独立时隙，避免与电控冲突） ========== */
            if (Get_SysTick_ms() - ts[5] >= 800) {
                ts[5] = Get_SysTick_ms();
                //SCHE_LOG("[T2]%d\r\n", ts[5]);
                if (s_bat_paired) {
                    /* 等待电控TX完成 */
                    uint32_t tw = Get_SysTick_ms();
                    while (hrf.TxState != TX_IDLE) {
                        if (Get_SysTick_ms() - tw > 10) break;
                    }
                    //SCHE_LOG("[T2a]wait=%d\r\n", Get_SysTick_ms() - tw);
                    comm_send_bat_query();
                    //SCHE_LOG("[T2b]\r\n");
                }
            }
        }

        /* ========== 120ms: LCD刷新 ========== */
        if (Get_SysTick_ms() - ts[3] >= 120) {
            ts[3] = Get_SysTick_ms();
            //SCHE_LOG("[T3]%d\r\n", ts[3]);
            #if(ENABLE_LED_DISPLAY)
            {
                uint8_t bat_conn;
                if (!s_bat_paired) {
                    bat_conn = BAT_CONN_UNPAIRED;
                } else if (Get_SysTick_ms() - s_bat_last_rx_tick > BAT_ONLINE_TIMEOUT_MS) {
                    bat_conn = BAT_CONN_OFFLINE;
                } else {
                    bat_conn = BAT_CONN_ONLINE;
                }
                update_ui_test(0, s_ext_bat_status.soc,
                               s_ui_mode, s_bat_paired, s_bat_addr,
                               bat_conn);
            }
            #endif
            //SCHE_LOG("[T3a]\r\n");
        }

        /* ========== 240ms: 电量检测/充电状态/关机 ========== */
        if (Get_SysTick_ms() - ts[4] >= 240) {
            ts[4] = Get_SysTick_ms();
            //SCHE_LOG("[T4]%d\r\n", ts[4]);
            static uint8_t tmp_cnt;
            if(++tmp_cnt>20) //4.8s检测一次锂电池电压
            {   tmp_cnt=0;
                bat_manage_update(&s_bat);
                SCHE_LOG("ADC: %d, li_BAT: %dmV SOC:%d%% CHG:%d\r\n",
                        s_bat.data.adc_raw, s_bat.data.voltage_mv, s_bat.data.soc, s_bat.data.chg_state);
            }

            if (app_key_get_shutdown_flag()) {
                bat_manage_power_off(&s_bat);
            }
        }

        /* ========== 睡眠判断 ========== */
        //SCHE_LOG("[T5]\r\n");
        //等到rf射频模块空闲为止(最大是maxrt的发送时间)，且加上超时防止意外卡死
        uint32_t start_time = Get_SysTick_ms();
        while(hrf.TxState!=TX_IDLE) {
            if (Get_SysTick_ms() - start_time > 10) { // 超时10ms
                break;
            }
        }
        //SCHE_LOG("[T5a]st=%d\r\n", hrf.TxState);

        // gpio_config(Port_Pin(0,0),GPIO_FLOAT,GPIO_PULL_NONE); //uart关掉
        // gpio_config(Port_Pin(0,1),GPIO_FLOAT,GPIO_PULL_NONE);
        // delay_ms(3);
        app_enter_sleep_with_wakeup_by_timer(40, allow_sleep_flag);
        uart_printf(".\r\n");
        //SCHE_LOG("[T6]\r\n");
    }
}
#endif
void RC_Scheduler_SetESCAddr(RC_Scheduler_t *sched, uint8_t *addr)
{
}
