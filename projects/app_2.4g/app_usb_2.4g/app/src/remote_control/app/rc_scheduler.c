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
#include "app_shutdown.h"
#include "app_sleep.h"
#include "app_key.h"
#include "app_throttle.h"
#include "rf_handler.h"
#include "rf_pair.h"
#include "rf_config.h"
#include "timer_handler.h"
#include "user_config.h"
#include "debug.h"
#include <string.h>
#include "hal_drv_rf.h"
#include "drv_gpio.h"

extern RF_HandleTypeDef hrf;

/* RF发送后的保护窗口(ms) */
#define RF_GUARD_WINDOW_MS  1

/* ======================== 通信层(static) ======================== */

static uint8_t         s_esc_addr[5];       /* 电控地址 */
static rc_ctrl_t       s_ctrl;              /* 当前下发数据 */
static proto_tracker_t s_tracker;           /* seq跟踪器 */
static mc_status_t     s_mc_status;         /* 电控状态 */
static uint8_t         s_mc_online;         /* 电控在线标志 */

/**
 * @brief 打包并发送控制帧
 */
static void comm_send_ctrl_frame(void)
{
    uint8_t tx_buf[32];
    uint8_t seq = tracker_next_seq(&s_tracker);
    uint8_t len = proto_pack_ctrl(tx_buf, DEV_REMOTE, DEV_ESC, seq, &s_ctrl);

    RF_Send(s_esc_addr, tx_buf, len);
}

/**
 * @brief 处理接收队列，解析ACK payload
 */
static void comm_process_rx(void)
{
    const uint8_t *rx_data;
    uint8_t rx_len, pipes;

    while (RF_rxQueue_Recv(&rx_data, &rx_len, &pipes)) {
        if (rx_len < 6) continue;
        uint8_t cmd = rx_data[5];
        switch (cmd) {
        case CMD_MC_STATUS: {
            uint8_t src_dev, dst_dev, ack_seq;
            mc_status_t status;
            if (proto_parse_status(rx_data, rx_len, &src_dev, &dst_dev, &ack_seq, &status) == 0) {
                /* 校验设备类型 */
                if (dst_dev != DEV_REMOTE) break;
                s_mc_status = status;
                uart_printf("speed: %d meter=%d\r\n", s_mc_status.speed, s_mc_status.mileage);
                s_mc_online = 1;
                tracker_on_ack(&s_tracker, ack_seq);
            }
            break;
        }
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
        uart_printf("Throttle changed:%d\r\n", throttle_val);
    }

    /* pending期间持续发送，直到收到ACK确认 */
    if (tracker_is_pending(&s_tracker)) {
        comm_send_ctrl_frame();
    }
}

/* ======================== 调度层 ======================== */

void RC_Scheduler_Init(RC_Scheduler_t *sched)
{
    memset(sched, 0, sizeof(RC_Scheduler_t));

    app_key_init();

    /* 锁定电源 */
    app_board_power_on();

    /* 从Flash读取RF设备地址 */
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


    /* 从Flash加载电控地址，如果已配对则应用 */
    if (rf_config_read_device_addr(DEV_TYPE_ESC, s_esc_addr)) {
        HAL_RF_SetTxAddress(&hrf, s_esc_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, s_esc_addr, 5);
        uart_printf("ESC addr loaded: %02X %02X %02X %02X %02X\r\n",
                    s_esc_addr[0], s_esc_addr[1], s_esc_addr[2], s_esc_addr[3], s_esc_addr[4]);
    } else {
        /* 未配对，使用默认地址 */
        HAL_RF_GetRxAddress(&hrf, 0, s_esc_addr);
        uart_printf("ESC not paired, using default addr\r\n");
    }
    
    sched->initialized = 1;
    uart_printf("RC_Scheduler_Init done\r\n");
}

void RC_Scheduler_Task(RC_Scheduler_t *sched)
{
    if (!sched->initialized) return;

    uart_printf("enter RC_Scheduler_Task\r\n");
    debug_print_rf_registers();

    uint32_t ts[8] = {0};
    static uint8_t sleep_flag = 1;
    static uint8_t last_pair = 0;

    while (1) {
        uint32_t now = Get_SysTick_ms();

        /* ========== 10ms: 按键扫描 ========== */
        if (now - ts[0] >= 20) {
            ts[0] = now;
            app_key_scan(20);
        }

        /* ========== 配对处理（始终调用，内部自行管理状态） ========== */
        uint8_t *pair_flag_ptr = app_key_get_pair_flag_ptr();
        Host_Pairing_Task(pair_flag_ptr);

        if (*pair_flag_ptr) { //如果配对标志被按键设置为1进入配对，记录last_pair状态
            last_pair = 1;
            delay_ms(10);
            sleep_flag = 0;
        } else {
            /* 配对刚结束，同步本地ESC地址缓存 */
            if (last_pair) {
                last_pair = 0;
                tracker_init(&s_tracker);
                if (rf_config_read_device_addr(DEV_TYPE_ESC, s_esc_addr)) {
                    HAL_RF_SetTxAddress(&hrf, s_esc_addr, 5);
                    HAL_RF_SetRxAddress(&hrf, 0, s_esc_addr, 5);
                    uart_printf("ESC addr: %02X %02X %02X %02X %02X\r\n",
                        s_esc_addr[0], s_esc_addr[1], s_esc_addr[2], s_esc_addr[3], s_esc_addr[4]);
                }
            }
            sleep_flag = 1;

            /* ========== 50ms: 油门更新+RF发送+处理ACK ========== */
            if (now - ts[2] >= 50) {
                ts[2] = now;
                static uint8_t hb_cnt;
                hb_cnt++;

                control_update_and_send();
                if (hb_cnt >= 10) {
                    hb_cnt = 0;
                    comm_send_ctrl_frame();
                }
                comm_process_rx();
            }
        }
        
        /* ========== 100ms: LCD刷新 ========== */
        if (now - ts[3] >= 100) {
            ts[3] = now;
            /* TODO: app_lcd_refresh(&s_mc_status); */
        }

        /* ========== 200ms: 关机检测 ========== */
        if (now - ts[4] >= 200) {
            ts[4] = now;
            app_board_shutdown(sched->shutdown_flag);
        }

        /* ========== 5000ms: 电池状态查询 ========== */  
        if (now - ts[6] >= 5000) {
            ts[6] = now;
            /* TODO: app_bat_status_process(); */
        }

        /* ========== 睡眠判断 ========== */
        //delay_ms(6); //延时一段时间等rf射频模块处理完
        
        //等到rf射频模块空闲为止(最大是maxrt的发送时间)，且加上超时防止意外卡死
        uint32_t start_time = Get_SysTick_ms();
        while(hrf.TxState!=TX_IDLE) {
            if (Get_SysTick_ms() - start_time > 10) { // 超时10ms
                break;
            }
        }

         
        // gpio_config(Port_Pin(0,0),GPIO_FLOAT,GPIO_PULL_NONE); //uart关掉
        // gpio_config(Port_Pin(0,1),GPIO_FLOAT,GPIO_PULL_NONE);
        app_enter_sleep_with_wakeup_by_timer(40, sleep_flag);
    }
}

void RC_Scheduler_SetESCAddr(RC_Scheduler_t *sched, uint8_t *addr)
{
}
