/**
 ****************************************************************************************
 * @file app_bat_passthrough.c
 * @brief 电池透传固件 - 主业务逻辑
 *
 * 模式：
 *   1. 透传模式(RX+ACK)：等待遥控器CMD_BAT_QUERY，通过ACK Payload回复CMD_BAT_STATUS
 *      SOC数据来源：UART接收BMS数据 → bat_protocol解析
 *   2. 配对模式：IO检测 + 串口配对码触发
 *
 * 地址管理：使用 rc_remote 的 rf_config 统一管理
 ****************************************************************************************
 */
#include "app_bat_passthrough.h"

#include <stdint.h>
#include <string.h>

#include "drv_gpio.h"
#include "my_drv_uart.h"
#include "timer_handler.h"
#include "user_config.h"

#include "bat_protocol.h"
#include "rf_handler.h"
#include "rf_pair.h"
#include "rf_config.h"
#include "hal_drv_rf.h"
#include "rc_protocol.h"
#include "debug.h"

#define APP_BAT_LOG 1
#if (APP_BAT_LOG)
    #define APP_BAT_LOG uart_printf
#else
    #define APP_BAT_LOG(...)  ((void)0) 
#endif

#define PAIR_IO_PIN  Port_Pin(0, 3)   /* BMS配对信号线, 高电平有效 */


extern RF_HandleTypeDef hrf;

typedef enum {
    APP_MODE_PASSTHROUGH = 0, //透传业务模式
    APP_MODE_PAIRING          //配对模式
} app_mode_t;

/* ======================== 最新BMS数据缓存 ======================== */
static bat_status_data_t s_bat_status = {
    .temperature = 65,      /* 25°C (偏移+40), 默认值 */
    .soc         = 0,
    .status      = 0x00,
};
static uint8_t s_soc_valid = 0;

static void app_bat_pairing_stop(uint8_t *pair_flag)
{
    *pair_flag = 0;
    Slave_Pairing_Task(pair_flag);
}

/**
 * @brief 处理业务模式下的RF接收 (RX + ACK Payload)
 *        收到CMD_BAT_QUERY → 用ACK Payload回复CMD_BAT_STATUS
 */
static void bat_process_business_rx(void)
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
        case CMD_BAT_QUERY: {
            uint8_t ack_buf[32];
            uint8_t ack_len = proto_pack(ack_buf, CMD_BAT_STATUS, seq,
                                         &s_bat_status, sizeof(s_bat_status));
            HAL_RF_Attach_PL2ACK(&hrf, pipes, ack_buf, ack_len);

            APP_BAT_LOG("Q seq=%d soc=%d%%\r\n", seq, s_bat_status.soc);
            break;
        }
        default:
            break;
        }
    }
}

void app_bat_passthrough_run(void)
{
    const uint32_t pair_total_timeout_ms = 10000;
    uint8_t pair_flag = 0;
    app_mode_t app_mode = APP_MODE_PASSTHROUGH;
    uint8_t pair_rsp_addr = 0;
    uint8_t pair_io_pending = 0;
    uint8_t pair_cmd_pending = 0;
    uint8_t pairing_running = 0;
    uint8_t last_pair = 0;
    uint32_t pair_start_tick = 0;
    uint32_t last_tick_2ms = 0;
    uint32_t last_tick_10ms = 0;

    /* 加载RF地址配置 */
    rf_config_load_from_flash();

    /* 初始化RF */
    RF_Handler_Init();
    HAL_RF_SetTxPower(&hrf, RF_TX_POWER_P7p6_dBm); //透传改大功率

    /* 初始化配对IO检测 (BMS拉高电平触发) */
    gpio_config(PAIR_IO_PIN, GPIO_INPUT, GPIO_PULL_LOW);

    /* 设置配对参数：电池设备，由Host分配地址 */
    Slave_Pairing_SetConfig(DEV_TYPE_BATTERY, ADDR_MODE_HOST_ASSIGN, NULL);
    
    /* 从Flash加载电池地址 */
    uint8_t bat_addr[5];
    if (rf_config_read_device_addr(DEV_TYPE_BATTERY, bat_addr)) {
        HAL_RF_SetRxAddress(&hrf, 0, bat_addr, 5);
        APP_BAT_LOG("BAT addr: %02X %02X %02X %02X %02X\r\n",
                    bat_addr[0], bat_addr[1], bat_addr[2], bat_addr[3], bat_addr[4]);
    } else {
        APP_BAT_LOG("BAT not paired, default addr\r\n");
    }

    /* 切换到RX模式 */
    HAL_RF_SetRxMode(&hrf);
    debug_print_rf_registers();

    APP_BAT_LOG("=== BAT passthrough v3 (RX+ACK) start ===\r\n");

    while (1) {
        uint32_t now = Get_SysTick_ms();

        /* 2ms: UART协议解析，配对码和动态电池信息 */
        if ((now - last_tick_2ms) >= 2) {
            last_tick_2ms = now;
            extern my_queue_t uart0_rxQueue;
            Protocol_ParseByte(&uart0_rxQueue);

            /* 取出最新电池状态，更新到bat_status缓存 */
            Bat_Status_t status_pkt;
            if (bat_protocol_take_status(&status_pkt)) {
                s_bat_status.temperature = status_pkt.temperature;
                s_bat_status.soc = status_pkt.soc;
                s_bat_status.status = status_pkt.status;
                s_soc_valid = 1;
                APP_BAT_LOG("temp=%d soc=%d%% st=0x%02X\r\n",
                            status_pkt.temperature, status_pkt.soc, status_pkt.status);
            }
        }

        /* 10ms: 配对逻辑 */
        if ((now - last_tick_10ms) >= 10) {
            uint8_t cmd_addr = 0;
            last_tick_10ms = now;

            /* IO检测 (非配对模式下) */
            if (app_mode != APP_MODE_PAIRING) {
                if (gpio_get_input(PAIR_IO_PIN)) {
                    if (!pair_io_pending) {
                        pair_io_pending = 1;
                        APP_BAT_LOG("PAIR_IO high\r\n");
                    }    
                } else {
                    if (pair_io_pending) {
                        APP_BAT_LOG("PAIR_IO low\r\n");
                    }
                    pair_io_pending = 0;
                }
            }

            /* 串口配对码检测 */
            if (bat_protocol_take_pair_cmd(&cmd_addr)) {
                pair_cmd_pending = 1;
                pair_rsp_addr = cmd_addr;
                APP_BAT_LOG("PAIR_CMD addr=0x%02X\r\n", pair_rsp_addr);
            }

            /* IO + 串口配对码同时满足 → 进入配对 */
            if (app_mode == APP_MODE_PASSTHROUGH &&
                pair_io_pending && pair_cmd_pending) {
                pair_flag = 1;
                pairing_running = 1;
                pair_start_tick = now;
                app_mode = APP_MODE_PAIRING;
                pair_io_pending = 0;
                pair_cmd_pending = 0;
                APP_BAT_LOG("Enter pairing\r\n");
            }

            /* 配对状态机 */
            if (app_mode == APP_MODE_PAIRING) {
                Slave_Pairing_Task(&pair_flag);

                if (pairing_running && pair_flag == 0) {
                    app_bat_pairing_stop(&pair_flag);
                    bat_protocol_send_pair_resp(pair_rsp_addr, 0x00);
                    pairing_running = 0;
                    app_mode = APP_MODE_PASSTHROUGH;
                    pair_io_pending = gpio_get_input(PAIR_IO_PIN) ? 1 : 0;
                    pair_cmd_pending = 0;
                    last_pair = 1;
                } else if (pairing_running && (now - pair_start_tick > pair_total_timeout_ms)) {
                    app_bat_pairing_stop(&pair_flag);
                    bat_protocol_send_pair_resp(pair_rsp_addr, 0x01);
                    pairing_running = 0;
                    app_mode = APP_MODE_PASSTHROUGH;
                    pair_io_pending = gpio_get_input(PAIR_IO_PIN) ? 1 : 0;
                    pair_cmd_pending = 0;
                    last_pair = 1;
                }

                continue; /* 配对中不处理业务 */
            }

            /* 配对刚结束，重新加载地址并切RX */
            if (last_pair) {
                last_pair = 0;
                /* 配对成功后rf_pair已经save_to_flash了，重新加载 */
                if (rf_config_read_device_addr(DEV_TYPE_BATTERY, bat_addr)) {
                    HAL_RF_SetRxAddress(&hrf, 0, bat_addr, 5);
                    APP_BAT_LOG("BAT addr: %02X %02X %02X %02X %02X\r\n",
                                bat_addr[0], bat_addr[1], bat_addr[2], bat_addr[3], bat_addr[4]);
                }
                HAL_RF_SetRxMode(&hrf);
                APP_BAT_LOG("Pair done, back to RX\r\n");
            }
        }

        /* 业务模式：RF收发 + 处理CMD_BAT_QUERY */
        if (app_mode == APP_MODE_PASSTHROUGH) {
            RF_Service_Handler(&hrf);
            bat_process_business_rx();
        }

        delay_ms(2);
    }
}
