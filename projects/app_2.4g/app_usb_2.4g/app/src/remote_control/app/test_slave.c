/**
 ****************************************************************************************
 * @file test_slave.c
 * @brief 电控端（Slave）主循环 - 支持配对和业务模式
 *
 * 功能：
 *   1. 配对模式：响应遥控器的配对请求
 *   2. 业务模式：接收控制帧，回复状态帧（ACK Payload）
 *
 ****************************************************************************************
 */

#include "rc_protocol.h"
#include "rf_handler.h"
#include "rf_pair.h"
#include "rf_config.h"
#include "hal_drv_rf.h"
#include "timer_handler.h"
#include "user_config.h"
#include <string.h>
#include "debug.h"
#include "key_scan.h"

extern RF_HandleTypeDef hrf;

/* 配对标志 */
static uint8_t pair_flag = 0;

/* 按键事件处理 */
void app_key_event_handler_test(key_id_t id, key_event_t event)
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
                pair_flag = !pair_flag;
                uart_printf("Pairing flag toggled: %d\r\n", pair_flag);
                right_cnt = 0;
            }
        }
    }
}

/* 电控状态 */
static mc_status_t s_mc_status = {
    .speed = 0,
    .mileage = 0,
};

/**
 * @brief 处理业务模式下的接收数据
 */
static void process_business_rx(void)
{
    const uint8_t *rx_data;
    uint8_t rx_len, pipes;

    while (RF_rxQueue_Recv(&rx_data, &rx_len, &pipes)) {
        if (rx_len < 6) continue;

        uint8_t cmd = rx_data[5];

        switch (cmd) {
        case CMD_RC_CTRL: {
            uint8_t src_dev, dst_dev, seq;
            rc_ctrl_t ctrl;

            if (proto_parse_ctrl(rx_data, rx_len, &src_dev, &dst_dev, &seq, &ctrl) == 0) {
                /* 校验设备类型 */
                if (dst_dev != DEV_TYPE_ESC) break;

                uart_printf("RX CTRL: seq=%d gear=%d mode=%d throttle=%d\r\n",
                            seq, ctrl.gear, ctrl.mode, ctrl.throttle);

                /* 模拟电控：根据油门更新速度 */
                s_mc_status.speed = ctrl.throttle * 10;
                s_mc_status.mileage += 1;

                /* 打包状态帧作为ACK payload */
                uint8_t ack_buf[32];
                uint8_t ack_len = proto_pack_status(ack_buf, DEV_TYPE_ESC, DEV_TYPE_REMOTE, seq, &s_mc_status);
                HAL_RF_Attach_PL2ACK(&hrf, pipes, ack_buf, ack_len);

                uart_printf("ACK STATUS: seq=%d speed=%d mileage=%lu\r\n",
                            seq, s_mc_status.speed, s_mc_status.mileage);
            }
            break;
        }
        default:
            break;
        }
    }
}

void test_slave_loop(void)
{
    uart_printf("=== ESC (Slave) Mode Start ===\r\n");

    /* 初始化按键 */
    key_config_t test_keys[] = {
        {KEY_ID_LEFT,   Port_Pin(0, 2), 2000, false}, // Left Key: 3s Long Press
        {KEY_ID_RIGHT,  Port_Pin(3, 1), 1000, false}, // Right Key: Default 1s
    };
    key_init(test_keys, sizeof(test_keys)/sizeof(key_config_t));
    key_register_callback(app_key_event_handler_test);

    /* 加载配置 */
    rf_config_load_from_flash();

    /* 初始化RF */
    RF_Handler_Init();

    /* 从Flash加载自己的地址（电控地址） */
    uint8_t esc_addr[5];
    if (rf_config_read_device_addr(DEV_TYPE_ESC, esc_addr)) {
        HAL_RF_SetRxAddress(&hrf, 0, esc_addr, 5);
        uart_printf("ESC addr loaded: %02X %02X %02X %02X %02X\r\n",
                    esc_addr[0], esc_addr[1], esc_addr[2], esc_addr[3], esc_addr[4]);
    } else {
        uart_printf("ESC not paired, using default addr\r\n");
    }

    /* 切换到RX模式 */
    HAL_RF_SetRxMode(&hrf);
    debug_print_rf_registers();

    uart_printf("ESC ready. Press right key 5 times to pair.\r\n");

    while (1) {
        uint32_t now = Get_SysTick_ms();
        static uint32_t last_key_scan = 0;
        static uint8_t last_pair = 0;

        /* 10ms: 按键扫描 */
        if (now - last_key_scan >= 10) {
            last_key_scan = now;
            key_scan(10);
        }

        /* 配对处理（始终调用，内部自行管理状态） */
        Slave_Pairing_Task(&pair_flag);

        if (pair_flag) {
            last_pair = 1;
            delay_ms(10);
            continue;
        }

        /* 配对刚结束，同步本地地址 */
        if (last_pair) {
            last_pair = 0;
            if (rf_config_read_device_addr(DEV_TYPE_ESC, esc_addr)) {
                HAL_RF_SetRxAddress(&hrf, 0, esc_addr, 5);
                uart_printf("ESC addr: %02X %02X %02X %02X %02X\r\n",
                            esc_addr[0], esc_addr[1], esc_addr[2], esc_addr[3], esc_addr[4]);
            }
            HAL_RF_SetRxMode(&hrf);
        }

        /* 业务模式：处理接收数据 */
        RF_Service_Handler(&hrf);
        process_business_rx();

        delay_ms(2);
    }
}
