#include "driver_bk24.h"
#include "hal_drv_rf.h"
#include "rf_handler.h"
#include "rf_pair.h"
#include "rf_config.h"
#include "user_config.h"
//#include "app.h"
#include "bk3633_reglist.h"
#include "driver_timer.h"
#include <string.h>
#include "my_drv_uart.h"
#include "flash.h"

#include "co_error.h"
#include "rwip.h"
#include "ll.h"
#include "wdt.h"
#include "rwprf_config.h"

#define PAIR_LOG_EN 0
#if (PAIR_LOG_EN)
    #define pair_log uart_printf
#else
    #define pair_log(...)  ((void)0) 
#endif

static const uint8_t PAIR_ADDR_DEFAULT[5] = RF_PAIR_ADDR_DEFAULT;

static void printf_all_registers(void)
{
    pair_log("TRX_CONFIG     = 0x%02X\r\n", TRX_CONFIG);
    pair_log("TRX_EN_AA      = 0x%02X\r\n", TRX_EN_AA);
    pair_log("TRX_EN_RXADDR  = 0x%02X\r\n", TRX_EN_RXADDR);
    pair_log("TRX_SETUP_AW   = 0x%02X\r\n", TRX_SETUP_AW);
    pair_log("TRX_SETUP_RETR = 0x%02X\r\n", TRX_SETUP_RETR);
    pair_log("TRX_RF_CH      = 0x%02X\r\n", TRX_RF_CH);
    pair_log("TRX_RF_SETUP   = 0x%02X\r\n", TRX_RF_SETUP);

    pair_log("TRX_RX_ADDR_P0 = ");
    //for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    pair_log("\r\n");

    pair_log("TRX_RX_ADDR_P1 = ");
    //for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P1_0)[i]);
    pair_log("\r\n");

    pair_log("TRX_RX_ADDR_P2 = 0x%02X\r\n", TRX_RX_ADDR_P2);
    pair_log("TRX_RX_ADDR_P3 = 0x%02X\r\n", TRX_RX_ADDR_P3);
    pair_log("TRX_RX_ADDR_P4 = 0x%02X\r\n", TRX_RX_ADDR_P4);
    pair_log("TRX_RX_ADDR_P5 = 0x%02X\r\n", TRX_RX_ADDR_P5);

    pair_log("TRX_TX_ADDR    = ");
    //for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    pair_log("\r\n");

    pair_log("TRX_RX_PW_P0   = 0x%02X\r\n", TRX_RX_PW_P0);
    pair_log("TRX_RX_PW_P1   = 0x%02X\r\n", TRX_RX_PW_P1);
    pair_log("TRX_RX_PW_P2   = 0x%02X\r\n", TRX_RX_PW_P2);
    pair_log("TRX_RX_PW_P3   = 0x%02X\r\n", TRX_RX_PW_P3);
    pair_log("TRX_RX_PW_P4   = 0x%02X\r\n", TRX_RX_PW_P4);
    pair_log("TRX_RX_PW_P5   = 0x%02X\r\n", TRX_RX_PW_P5);

    pair_log("TRX_DYNPD      = 0x%02X\r\n", TRX_DYNPD);
    pair_log("TRX_FEATURE    = 0x%02X\r\n", TRX_FEATURE);

    pair_log("addXVR_Reg0x24 = 0x%08lX\r\n", addXVR_Reg0x24);
    pair_log("addXVR_Reg0x3b = 0x%08lX\r\n", addXVR_Reg0x3b);
    pair_log("addXVR_Reg0x2e = 0x%08lX\r\n", addXVR_Reg0x2e);
    pair_log("addXVR_Reg0x26 = 0x%08lX\r\n", addXVR_Reg0x26);
    pair_log("addXVR_Reg0x2  = 0x%08lX\r\n", addXVR_Reg0x2);
    pair_log("addXVR_Reg0x2c = 0x%08lX\r\n", addXVR_Reg0x2c);
    pair_log("addXVR_Reg0x2d = 0x%08lX\r\n", addXVR_Reg0x2d);
    pair_log("addXVR_Reg0x3a = 0x%08lX\r\n", addXVR_Reg0x3a);
}

// 调试打印函数
static void printf_txrx_addr(void)
{
    pair_log("RX_ADDR: ");
    //for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    pair_log(" | TX_ADDR: ");
   // for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    pair_log("\r\n");
}


static SlavePairCtrl_t g_slave_ctrl = {
    .state = SLAVE_PAIR_IDLE,
    .start_wait = 0,
    .dev_type = DEV_TYPE_ESC,
    .addr_mode = ADDR_MODE_HOST_ASSIGN,
    .slave_addr = {0},
    .verify_cnt = 0,
    .verify_target_count = 5,
    .reach_goal_flag = 0,
    .last_ping_recv_timestamp = 0
};

void Slave_Pairing_SetConfig(uint8_t dev_type, uint8_t addr_mode, const uint8_t *addr)
{
    g_slave_ctrl.dev_type = dev_type;
    g_slave_ctrl.addr_mode = addr_mode;
    if (addr_mode == ADDR_MODE_SLAVE_PROVIDE && addr != NULL) {
        memcpy(g_slave_ctrl.slave_addr, addr, 5);
    } else {
        memset(g_slave_ctrl.slave_addr, 0, 5);
    }
}

void Slave_Pairing_Task(uint8_t* flag) {
    static uint8_t last_flag = 0;
    static uint8_t def_addr[5];  // static: 保持配对地址

    // flag=0: 未配对或已退出
    if (!(*flag)) {
        if (last_flag) {
            // 1→0 跳变：cleanup一次
            rf_config_load_from_flash();
            RF_Handler_Init_ToNormal();
            RF_txQueue_Clear();
            RF_rxQueue_clear();
            g_slave_ctrl.state = SLAVE_PAIR_IDLE;
            pair_log("Slave: Pairing Stopped.\n");
        }
        last_flag = 0;
        return;
    }

    if ((*flag) && !last_flag) {
        last_flag = 1;
        /* 保存配置（memset会清掉） */
        uint8_t saved_dev_type = g_slave_ctrl.dev_type;
        uint8_t saved_addr_mode = g_slave_ctrl.addr_mode;
        uint8_t saved_addr[5];
        memcpy(saved_addr, g_slave_ctrl.slave_addr, 5);
        memset(&g_slave_ctrl, 0, sizeof(g_slave_ctrl));
        g_slave_ctrl.state = SLAVE_PAIR_IDLE;
        g_slave_ctrl.verify_target_count = 5;
        g_slave_ctrl.dev_type = saved_dev_type;
        g_slave_ctrl.addr_mode = saved_addr_mode;
        memcpy(g_slave_ctrl.slave_addr, saved_addr, 5);

        // 初始化为配对模式
        RF_Handler_Init_ToPair();

        rf_config_get_pair_addr(def_addr);
        HAL_RF_SetTxAddress(&hrf, def_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
        HAL_RF_SetTxMode(&hrf);
        printf_all_registers();
        pair_log("Slave: Start Pairing Task...\n");
    }

    /* v2: REQ包从g_slave_ctrl读取dev_type + addr_mode + addr */
    pair_req_pkt req_pkt;
    memset(&req_pkt, 0, sizeof(req_pkt));
    req_pkt.cmd = CMD_PAIR_REQ;
    req_pkt.dev_type = g_slave_ctrl.dev_type;
    req_pkt.addr_mode = g_slave_ctrl.addr_mode;
    if (g_slave_ctrl.addr_mode == ADDR_MODE_SLAVE_PROVIDE) {
        memcpy(req_pkt.addr, g_slave_ctrl.slave_addr, 5);
    }

    pair_resp_pkt *recv_resp_pkg = NULL;
    pair_verify_pkt pong_pkt = {CMD_PAIR_VERIFY_PONG, slave_magic_number};
    pair_verify_pkt *recv_ping_pkt = NULL;
    uint8_t len;
    
    const uint16_t RESP_WAIT_TIMEOUT = 100;  //这个时间可以短
    const uint16_t VERIFY_TIMEOUT = 5000;    //5s，这个时间不能太短

    // 维持RF底层
    RF_Service_Handler(&hrf);

    switch (g_slave_ctrl.state) {
        case SLAVE_PAIR_IDLE:
            g_slave_ctrl.state = SLAVE_PAIR_SEND_REQ;
            break;

        case SLAVE_PAIR_SEND_REQ:
            pair_log("Slave: Send REQ T: ");
            for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
            pair_log("R: ");
            for(int i=0;i<5;i++) pair_log("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
            pair_log("\n");

            // 确保地址正确
            uint8_t def_addr[5];
            rf_config_get_pair_addr(def_addr);
            HAL_RF_SetTxAddress(&hrf, def_addr, 5);
            HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
            RF_txQueue_Send(def_addr,(uint8_t*)&req_pkt, sizeof(req_pkt));

            g_slave_ctrl.start_wait = Get_SysTick_ms();
            g_slave_ctrl.state = SLAVE_PAIR_WAIT_RESP;
            break;

        case SLAVE_PAIR_WAIT_RESP:
            //pair_log("Slave: Wait RESP:%d\n", Get_SysTick_ms());
            if (RF_rxQueue_Recv(&recv_resp_pkg, &len, NULL) == 1) {
                if (len == sizeof(pair_resp_pkt) && recv_resp_pkg->cmd == CMD_PAIR_RESP) {

                    memcpy(&g_slave_ctrl.resp_pkt, recv_resp_pkg, sizeof(pair_resp_pkt));
                    pair_log("Slave: Got RESP! Switch to New Addr:");
                    for(int i=0;i<5;i++) pair_log("%02X ", g_slave_ctrl.resp_pkt.new_addr[i]);
                    pair_log("\n");

                    // 立即切换地址前，清空队列
                    RF_txQueue_Clear();
                    RF_rxQueue_clear();
                    HAL_RF_SetTxAddress(&hrf, g_slave_ctrl.resp_pkt.new_addr, 5);
                    HAL_RF_SetRxAddress(&hrf, 0, g_slave_ctrl.resp_pkt.new_addr, 5);

                    //进入验证
                    g_slave_ctrl.verify_cnt = 0;
                    g_slave_ctrl.reach_goal_flag = 0;
                    g_slave_ctrl.last_ping_recv_timestamp = Get_SysTick_ms();
                    g_slave_ctrl.start_wait = Get_SysTick_ms();
                    g_slave_ctrl.state = SLAVE_PAIR_VERIFY_PHASE;
                    return; 
                }
            }

            //超时快速重发
            if (Get_SysTick_ms() - g_slave_ctrl.start_wait > RESP_WAIT_TIMEOUT) {
                g_slave_ctrl.state = SLAVE_PAIR_SEND_REQ;
            }
            break;

        case SLAVE_PAIR_VERIFY_PHASE:
            //监听Ping
            uint8_t pipe0_addr[5];
            HAL_RF_GetRxAddress(&hrf, 0, pipe0_addr);
            
            if (RF_rxQueue_Recv(&recv_ping_pkt, &len, NULL) == 1) {
                if (len == sizeof(pair_verify_pkt) &&
                    recv_ping_pkt->cmd == CMD_PAIR_VERIFY_PING &&
                    recv_ping_pkt->magic_number == host_magic_number) {
                    
                    // 重置超时计时和最后一次收到ping时间
                    g_slave_ctrl.start_wait = Get_SysTick_ms();
                    g_slave_ctrl.last_ping_recv_timestamp = Get_SysTick_ms();
                    // 收到 Ping，回复 Pong
                    RF_txQueue_Send(g_slave_ctrl.resp_pkt.new_addr,(uint8_t*)&pong_pkt, sizeof(pong_pkt));
                    
                    g_slave_ctrl.verify_cnt++;
                    pair_log("Slave: Verified %d/%d\n", g_slave_ctrl.verify_cnt, g_slave_ctrl.verify_target_count);

                    if (g_slave_ctrl.verify_cnt >= g_slave_ctrl.verify_target_count) {
                        // 标记,host端达标了,但要陪Host跑到终点
                        g_slave_ctrl.reach_goal_flag = 1; 
                        pair_log("Slave: ping pong reached!\n");
                    }
                }
            }
            //A：已经成功了,且Host很久(比如3秒)没ping->说明Host也好了
            if (g_slave_ctrl.reach_goal_flag == 1) {
                if (Get_SysTick_ms() - g_slave_ctrl.last_ping_recv_timestamp > 3000) {
                    pair_log("Slave: Host silent, Pairing Process DONE.\n");
                    g_slave_ctrl.state = SLAVE_PAIR_DONE;
                }
            }
            //B：还没成功，但是总超时了 -> 说明配对失败,host没在这个地址发包
            else {
                if (Get_SysTick_ms() - g_slave_ctrl.start_wait > VERIFY_TIMEOUT) {
                    pair_log("Slave: Verify Timeout! Failed.\n");
                    g_slave_ctrl.state = SLAVE_PAIR_SEND_REQ;
                }
            }

            break;

        case SLAVE_PAIR_DONE:
            pair_log("Slave: Pairing Process Success Completed.\n");
            printf_txrx_addr();

            rf_config_update_device_addr((device_type_t)g_slave_ctrl.dev_type, g_slave_ctrl.resp_pkt.new_addr);
            rf_config_save_to_flash();

            // flag置0，下次调用时last_flag=1触发cleanup恢复RF
            *flag = 0;
            break;
    }
}



static HostPairCtrl_t g_host_ctrl={
    .state = HOST_PAIR_IDLE, 
    .start_wait = 0,
    .last_ping_tick = 0,
    .verify_cnt = 0,
    .verify_target_count = 10
};

/**
* @brief: Host端配对任务
* @param: flag: 0->stop, 1->start
* @return: None
*/
void Host_Pairing_Task(uint8_t* flag) {
    static uint8_t last_flag = 0;
    static uint8_t def_addr[5];  // static: 保持配对地址

    // flag=0: 未配对或已退出
    if (!(*flag)) {
        if (last_flag) {
            // 1→0 跳变：cleanup一次
            rf_config_load_from_flash();
            RF_Handler_Init_ToNormal();
            RF_txQueue_Clear();
            RF_rxQueue_clear();
            g_host_ctrl.state = HOST_PAIR_IDLE;
            pair_log("Host: Pairing Stopped.\n");
        }
        last_flag = 0;
        return;
    }
    //flag从0->1,开始配对,进行一次初始化
    if ((*flag) && !last_flag) {
        pair_log("Host: Init pairing (flag=%d, last_flag=%d)\n", *flag, last_flag);
        last_flag = 1;
        //初始化状态机
        memset(&g_host_ctrl, 0, sizeof(g_host_ctrl));
        g_host_ctrl.state = HOST_PAIR_IDLE;
        g_host_ctrl.verify_target_count = 10;

        // 初始化rf为配对模式
        RF_Handler_Init_ToPair();

        //设置默认地址
        rf_config_get_pair_addr(def_addr);
        HAL_RF_SetTxAddress(&hrf, def_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
        
        HAL_RF_SetRxMode(&hrf);
        printf_all_registers();
        pair_log("Host: Start Pairing Task...\n");
    }
    
    pair_req_pkt *recv_req = NULL;
    pair_verify_pkt *recv_pong_pkt = NULL;
    pair_verify_pkt ping_pkt = {CMD_PAIR_VERIFY_PING, host_magic_number};
    uint8_t len;

    RF_Service_Handler(&hrf);

    //pair_log("Host: state=%d, flag=%d\n", g_host_ctrl.state, *flag);

    switch (g_host_ctrl.state) {
        
        case HOST_PAIR_IDLE:{
            g_host_ctrl.state = HOST_PAIR_WAIT_REQ;
            pair_log("Host: Waiting for Req...\n");
            break;
        }
            
        case HOST_PAIR_WAIT_REQ:{
            HAL_RF_SetTxAddress(&hrf, def_addr, 5);
            HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
            
            if (RF_rxQueue_Recv(&recv_req, &len, NULL) == 1) {
                if (len == sizeof(pair_req_pkt) && recv_req->cmd == CMD_PAIR_REQ) {
                    /* v2: 直接从REQ包读取dev_type */
                    device_type_t dev_type = (device_type_t)recv_req->dev_type;
                    uint8_t addr_mode = recv_req->addr_mode;
                    pair_log("Host: Recv REQ dev_type=%d addr_mode=%d\n", dev_type, addr_mode);

                    /* 根据addr_mode决定地址来源 */
                    uint8_t new_addr[5];
                    if (addr_mode == ADDR_MODE_SLAVE_PROVIDE) {
                        /* Slave自带地址，直接采纳 */
                        memcpy(new_addr, recv_req->addr, 5);
                        pair_log("Host: Use slave addr: %02X %02X %02X %02X %02X\n",
                                    new_addr[0], new_addr[1], new_addr[2], new_addr[3], new_addr[4]);
                    } else {
                        /* Host生成随机地址(默认) */
                        rf_config_generate_addr(new_addr);
                        pair_log("Host: Generated addr: %02X %02X %02X %02X %02X\n",
                                    new_addr[0], new_addr[1], new_addr[2], new_addr[3], new_addr[4]);
                    }

                    /* 准备 RESP 包 */
                    g_host_ctrl.resp_pkt.cmd = CMD_PAIR_RESP;
                    memcpy(g_host_ctrl.resp_pkt.new_addr, new_addr, 5);
                    g_host_ctrl.resp_pkt.new_chn = 0; /* 预留 */
                    g_host_ctrl.paired_dev_type = dev_type;

                    /* 准备连发 */
                    g_host_ctrl.state = HOST_PAIR_SEND_RESP;
                }
            }
            break;

        }

        case HOST_PAIR_SEND_RESP:{
            pair_log("Host: Burst Sending RESP (Blind Switch)...\n");
            
            HAL_RF_SetTxAddress(&hrf, def_addr, 5);
            HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
            
            //连放5包到发送队列
            RF_txQueue_Clear();
            RF_rxQueue_clear();
            for(int i=0; i<5; i++) 
                RF_txQueue_Send(def_addr,(uint8_t*)&g_host_ctrl.resp_pkt, sizeof(pair_resp_pkt));
            
            // 记录基准
            g_host_ctrl.txds_snapshot = rf_int_count_txds; 
            g_host_ctrl.start_wait = Get_SysTick_ms();
            g_host_ctrl.state = HOST_PAIR_WAIT_TX_DONE;
            break;

        }
            
        case HOST_PAIR_WAIT_TX_DONE:{
            if (rf_int_count_txds > g_host_ctrl.txds_snapshot) {
                pair_log("Host: Burst Done. Switch Addr.\n");
                // 切换地址前清空队列
                RF_txQueue_Clear();
                RF_rxQueue_clear();
                //切换新地址
                HAL_RF_SetTxAddress(&hrf, g_host_ctrl.resp_pkt.new_addr, 5);
                HAL_RF_SetRxAddress(&hrf, 0, g_host_ctrl.resp_pkt.new_addr, 5);

                //进入验证
                g_host_ctrl.verify_cnt = 0;
                g_host_ctrl.last_ping_tick = 0; 
                g_host_ctrl.start_wait = Get_SysTick_ms();
                g_host_ctrl.state = HOST_PAIR_VERIFY_PHASE;
                pair_log("Host: goto verify phase.\n");
            
            }
            else if (Get_SysTick_ms() - g_host_ctrl.start_wait > 50) {
                pair_log("Host: TX Timeout! Skip.\n");
                g_host_ctrl.state = HOST_PAIR_WAIT_REQ; // 失败回退
            }
            break;

        }
            
        case HOST_PAIR_VERIFY_PHASE:{
            //定时PING 50ms一次
            if (Get_SysTick_ms() - g_host_ctrl.last_ping_tick > 50) {
                RF_txQueue_Send(g_host_ctrl.resp_pkt.new_addr,(uint8_t*)&ping_pkt, sizeof(ping_pkt));
                g_host_ctrl.last_ping_tick = Get_SysTick_ms();
                pair_log("Host: Sent PING,time:%d\n", g_host_ctrl.last_ping_tick);
            }
            //接收PONG
            if (RF_rxQueue_Recv(&recv_pong_pkt, &len, NULL) == 1) {
                if (recv_pong_pkt->cmd == CMD_PAIR_VERIFY_PONG &&
                    recv_pong_pkt->magic_number == slave_magic_number) {
                    
                    //收到 Pong，重置超时计时
                    g_host_ctrl.start_wait = Get_SysTick_ms();

                    g_host_ctrl.verify_cnt++;
                    pair_log("Host: Verified %d/%d\n", g_host_ctrl.verify_cnt, g_host_ctrl.verify_target_count);

                    if (g_host_ctrl.verify_cnt >= g_host_ctrl.verify_target_count) {
                        pair_log("Host: Pairing SUCCESS!\n");
                        printf_txrx_addr();
                        g_host_ctrl.state = HOST_PAIR_DONE;
                        return;
                    }
                }
            }

            // 如果切过来4000ms 还没收到 PONG，说明Slave没跟过来，回退
            if (Get_SysTick_ms() - g_host_ctrl.start_wait > 4000) {
                pair_log("Host: Verify Timeout! Slave Lost. Revert.\n");
                RF_txQueue_Clear();
                g_host_ctrl.state = HOST_PAIR_WAIT_REQ;
            }
            break;

        }
        
        case HOST_PAIR_DONE:{
            pair_log("Host: Pairing Process Success Completed.\n");
            printf_txrx_addr();

            // 保存配对结果
            rf_config_update_device_addr(g_host_ctrl.paired_dev_type, g_host_ctrl.resp_pkt.new_addr);
            rf_config_save_to_flash();

            // flag置0，下次调用时last_flag=1触发cleanup恢复RF
            *flag = 0;
            break;
        }
            
    }
}
