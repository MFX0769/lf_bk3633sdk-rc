#include "driver_bk24.h"
#include "hal_drv_rf.h"
#include "rf_handler.h" 
#include "rf_pair.h" 
#include "user_config.h" 
#include "app.h"
#include "bk3633_reglist.h"
#include "driver_timer.h"
#include <string.h>
#include "uart.h"
#include "addr_pool.h"
#include "flash.h"
#include "bk3633_reglist.h"

#include "co_error.h"      // error definition
#include "rwip.h"
#include "ll.h"
#include "wdt.h"
#include "rwprf_config.h"

#include "rf_addr_mgr.h"



static const uint8_t PAIR_ADDR_DEFAULT[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

static void printf_all_registers(void)
{
    uart_printf("TRX_CONFIG     = 0x%02X\r\n", TRX_CONFIG);
    uart_printf("TRX_EN_AA      = 0x%02X\r\n", TRX_EN_AA);
    uart_printf("TRX_EN_RXADDR  = 0x%02X\r\n", TRX_EN_RXADDR);
    uart_printf("TRX_SETUP_AW   = 0x%02X\r\n", TRX_SETUP_AW);
    uart_printf("TRX_SETUP_RETR = 0x%02X\r\n", TRX_SETUP_RETR);
    uart_printf("TRX_RF_CH      = 0x%02X\r\n", TRX_RF_CH);
    uart_printf("TRX_RF_SETUP   = 0x%02X\r\n", TRX_RF_SETUP);

    uart_printf("TRX_RX_ADDR_P0 = ");
    for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_ADDR_P1 = ");
    for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P1_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_ADDR_P2 = 0x%02X\r\n", TRX_RX_ADDR_P2);
    uart_printf("TRX_RX_ADDR_P3 = 0x%02X\r\n", TRX_RX_ADDR_P3);
    uart_printf("TRX_RX_ADDR_P4 = 0x%02X\r\n", TRX_RX_ADDR_P4);
    uart_printf("TRX_RX_ADDR_P5 = 0x%02X\r\n", TRX_RX_ADDR_P5);

    uart_printf("TRX_TX_ADDR    = ");
    for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    uart_printf("\r\n");

    uart_printf("TRX_RX_PW_P0   = 0x%02X\r\n", TRX_RX_PW_P0);
    uart_printf("TRX_RX_PW_P1   = 0x%02X\r\n", TRX_RX_PW_P1);
    uart_printf("TRX_RX_PW_P2   = 0x%02X\r\n", TRX_RX_PW_P2);
    uart_printf("TRX_RX_PW_P3   = 0x%02X\r\n", TRX_RX_PW_P3);
    uart_printf("TRX_RX_PW_P4   = 0x%02X\r\n", TRX_RX_PW_P4);
    uart_printf("TRX_RX_PW_P5   = 0x%02X\r\n", TRX_RX_PW_P5);

    uart_printf("TRX_DYNPD      = 0x%02X\r\n", TRX_DYNPD);
    uart_printf("TRX_FEATURE    = 0x%02X\r\n", TRX_FEATURE);

    uart_printf("addXVR_Reg0x24 = 0x%08lX\r\n", addXVR_Reg0x24);
    uart_printf("addXVR_Reg0x3b = 0x%08lX\r\n", addXVR_Reg0x3b);
    uart_printf("addXVR_Reg0x2e = 0x%08lX\r\n", addXVR_Reg0x2e);
    uart_printf("addXVR_Reg0x26 = 0x%08lX\r\n", addXVR_Reg0x26);
    uart_printf("addXVR_Reg0x2  = 0x%08lX\r\n", addXVR_Reg0x2);
    uart_printf("addXVR_Reg0x2c = 0x%08lX\r\n", addXVR_Reg0x2c);
    uart_printf("addXVR_Reg0x2d = 0x%08lX\r\n", addXVR_Reg0x2d);
    uart_printf("addXVR_Reg0x3a = 0x%08lX\r\n", addXVR_Reg0x3a);
}

// 调试打印函数
static void printf_txrx_addr(void)
{
    uart_printf("RX_ADDR: ");
    for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_RX_ADDR_P0_0)[i]);
    uart_printf(" | TX_ADDR: ");
    for(int i=0;i<5;i++) uart_printf("%02X ", (volatile uint32_t*)(&TRX_TX_ADDR_0)[i]);
    uart_printf("\r\n");
}


static SlavePairCtrl_t g_slave_ctrl = {
    .state = SLAVE_PAIR_IDLE, 
    .start_wait = 0,
    .verify_cnt = 0,
    .verify_target_count = 5,
    .reach_goal_flag = 0,
    .last_ping_recv_timestamp = 0
};

void Slave_Pairing_Task(uint8_t* flag) {
    static uint8_t last_flag = 0;
    uint8_t def_addr[5];

    if (!(*flag)) {
        if (g_slave_ctrl.state != SLAVE_PAIR_IDLE) {
            g_slave_ctrl.state = SLAVE_PAIR_IDLE;
            uart_printf("Slave: Pairing Stopped.\n");
        }
        last_flag = 0;
        return;
    }

    if ((*flag) && !last_flag) {
        last_flag = 1;
        memset(&g_slave_ctrl, 0, sizeof(g_slave_ctrl));
        g_slave_ctrl.state = SLAVE_PAIR_IDLE;
        g_slave_ctrl.verify_target_count = 5;
        
        app_addr_get_default(def_addr);
        HAL_RF_SetTxAddress(&hrf, def_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
        HAL_RF_SetTxMode(&hrf);
        printf_all_registers();
        uart_printf("Slave: Start Pairing Task...\n");
    }

    pair_req_pkt req_pkt = {CMD_PAIR_REQ, 0x10345678};
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
            uart_printf("Slave: Send REQ\n");
            // 确保地址正确
            uint8_t def_addr[5];
            app_addr_get_default(def_addr);
            HAL_RF_SetTxAddress(&hrf, def_addr, 5);
            HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
            RF_txQueue_Send(def_addr,(uint8_t*)&req_pkt, sizeof(req_pkt));

            g_slave_ctrl.start_wait = Get_SysTick_ms();
            g_slave_ctrl.state = SLAVE_PAIR_WAIT_RESP;
            break;

        case SLAVE_PAIR_WAIT_RESP:
            //uart_printf("Slave: Wait RESP:%d\n", Get_SysTick_ms());
            if (RF_rxQueue_Recv(&recv_resp_pkg, &len, NULL) == 1) {
                if (len == sizeof(pair_resp_pkt) && recv_resp_pkg->cmd == CMD_PAIR_RESP) {

                    memcpy(&g_slave_ctrl.resp_pkt, recv_resp_pkg, sizeof(pair_resp_pkt));
                    uart_printf("Slave: Got RESP! Switch to New Addr:");
                    for(int i=0;i<5;i++) uart_printf("%02X ", g_slave_ctrl.resp_pkt.new_addr[i]);
                    uart_printf("\n");

                    // 立即切换地址
                    RF_rxQueue_clear();
                    HAL_RF_SetTxAddress(&hrf, g_slave_ctrl.resp_pkt.new_addr, 5);
                    HAL_RF_SetRxAddress(&hrf, 0, g_slave_ctrl.resp_pkt.new_addr, 5);

                    //进入验证
                    g_slave_ctrl.verify_cnt = 0;
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
            uart_printf("ping,mode=%d,Pipe0 Addr:", __HAL_RF_Get_TRxMode_Bit());
            for(int i=0;i<5;i++) uart_printf("%02X ", pipe0_addr[i]);
            uart_printf("\n");
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
                    uart_printf("Slave: Verified %d/%d\n", g_slave_ctrl.verify_cnt, g_slave_ctrl.verify_target_count);

                    if (g_slave_ctrl.verify_cnt >= g_slave_ctrl.verify_target_count) {
                        // 标记,host端达标了,但要陪Host跑到终点
                        g_slave_ctrl.reach_goal_flag = 1; 
                        uart_printf("Slave: ping pong reached!\n");
                    }
                }
            }
            //A：已经成功了,且Host很久(比如3秒)没ping->说明Host也好了
            if (g_slave_ctrl.reach_goal_flag == 1) {
                if (Get_SysTick_ms() - g_slave_ctrl.last_ping_recv_timestamp > 3000) {
                    uart_printf("Slave: Host silent, Pairing Process DONE.\n");
                    g_slave_ctrl.state = SLAVE_PAIR_DONE;
                }
            }
            //B：还没成功，但是总超时了 -> 说明配对失败,host没在这个地址发包
            else {
                if (Get_SysTick_ms() - g_slave_ctrl.start_wait > VERIFY_TIMEOUT) {
                    uart_printf("Slave: Verify Timeout! Failed.\n");
                    g_slave_ctrl.state = SLAVE_PAIR_SEND_REQ;
                }
            }

            break;

        case SLAVE_PAIR_DONE:
            uart_printf("Slave: Pairing Process Success Completed.\n");
            printf_txrx_addr();
            app_addr_set_pipe0_addr(g_slave_ctrl.resp_pkt.new_addr);
            save_ctx_to_flash();
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

// -----------------------------------------------------------
// Host 端设备ID映射策略
// -----------------------------------------------------------
static uint8_t get_pipe_by_slave_id(uint32_t slave_id)
{
    // 示例策略：根据ID最高字节判断
    // 0x10xxxxxx -> Pipe 0 
    // 0x11xxxxxx -> Pipe 1 

    uint8_t prefix = (slave_id >> 24) & 0xFF;
    switch (prefix) {
        case 0x10: return 0;
        case 0x11: return 1;
        case 0x12: return 2;
        case 0x13: return 3;
        case 0x14: return 4;
        case 0x15: return 5;
        default:   
            //未知设备，默认拒绝或分配给 Pipe 5
            return 0xFF; 
    }
}

/**
* @brief: Host端配对任务
* @param: flag: 0->stop, 1->start
* @return: None
*/
void Host_Pairing_Task(uint8_t* flag) {
    static uint8_t last_flag = 0;
    uint8_t def_addr[5];
    //stop直接返回
    if (!(*flag)) {
        if (g_host_ctrl.state != HOST_PAIR_IDLE) {
            g_host_ctrl.state = HOST_PAIR_IDLE;
            uart_printf("Host: Pairing Stopped.\n");
        }
        last_flag = 0;
        return;
    }
    //flag从0->1,开始配对,进行一次初始化
    if ((*flag) && !last_flag) {
        last_flag = 1;
        //初始化状态机
        memset(&g_host_ctrl, 0, sizeof(g_host_ctrl));
        g_host_ctrl.state = HOST_PAIR_IDLE;
        g_host_ctrl.verify_target_count = 10;
        //设置默认地址
        app_addr_get_default(def_addr);
        HAL_RF_SetTxAddress(&hrf, def_addr, 5);
        HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
        HAL_RF_SetRxMode(&hrf);
        printf_all_registers();
        uart_printf("Host: Start Pairing Task...\n");
    }
    
    pair_req_pkt *recv_req = NULL;
    pair_verify_pkt *recv_pong_pkt = NULL;
    pair_verify_pkt ping_pkt = {CMD_PAIR_VERIFY_PING, host_magic_number};
    uint8_t len;
    
    app_addr_get_default(def_addr);
    RF_Service_Handler(&hrf);
    
    switch (g_host_ctrl.state) {
        
        case HOST_PAIR_IDLE:{
            g_host_ctrl.state = HOST_PAIR_WAIT_REQ;
            uart_printf("Host: Waiting for Req...\n");
            break;
        }
            
        case HOST_PAIR_WAIT_REQ:{
            HAL_RF_SetTxAddress(&hrf, def_addr, 5);
            HAL_RF_SetRxAddress(&hrf, 0, def_addr, 5);
            
            if (RF_rxQueue_Recv(&recv_req, &len, NULL) == 1) {
                if (len == sizeof(pair_req_pkt) && recv_req->cmd == CMD_PAIR_REQ) {
                    uart_printf("Host: Recv Req from %08X\n", recv_req->slave_id);
                    
                    // 根据设备id判断pipe
                    uint8_t target_pipe = get_pipe_by_slave_id(recv_req->slave_id);
                    if (target_pipe == 0xFF) {
                        uart_printf("Host: Unknown Device ID. Ignore.\n");
                        break;
                    }
                    
                    // 调用地址管理模块生成新地址 (生成+查重)
                    uint8_t new_addr[5];
                    if (!app_addr_genatate_for_pair(target_pipe, new_addr)) {
                        uart_printf("Host: Addr Alloc Failed!\n");
                        break;
                    }

                    //准备 RESP 包
                    g_host_ctrl.resp_pkt.cmd = CMD_PAIR_RESP;
                    memcpy(g_host_ctrl.resp_pkt.new_addr, new_addr, 5);

                    uart_printf("Host: Assigned Pipe %d Addr: %02X %02X %02X %02X %02X\n",
                                target_pipe,
                                g_host_ctrl.resp_pkt.new_addr[0],
                                g_host_ctrl.resp_pkt.new_addr[1],
                                g_host_ctrl.resp_pkt.new_addr[2],
                                g_host_ctrl.resp_pkt.new_addr[3],
                                g_host_ctrl.resp_pkt.new_addr[4]);

                    //准备连发
                    g_host_ctrl.state = HOST_PAIR_SEND_RESP;
                }
            }
            break;

        }

        case HOST_PAIR_SEND_RESP:{
            uart_printf("Host: Burst Sending RESP (Blind Switch)...\n");
            
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
            if (rf_int_count_txds >= g_host_ctrl.txds_snapshot) {
                uart_printf("Host: Burst Done. Switch Addr.\n");
                RF_txQueue_Clear();
                //切换新地址
                HAL_RF_SetTxAddress(&hrf, g_host_ctrl.resp_pkt.new_addr, 5);
                HAL_RF_SetRxAddress(&hrf, 0, g_host_ctrl.resp_pkt.new_addr, 5);

                //进入验证
                g_host_ctrl.verify_cnt = 0;
                g_host_ctrl.last_ping_tick = 0; 
                g_host_ctrl.start_wait = Get_SysTick_ms();
                g_host_ctrl.state = HOST_PAIR_VERIFY_PHASE;
                uart_printf("Host: goto verify phase.\n");
            
            }
            else if (Get_SysTick_ms() - g_host_ctrl.start_wait > 50) {
                uart_printf("Host: TX Timeout! Skip.\n");
                g_host_ctrl.state = HOST_PAIR_WAIT_REQ; // 失败回退
            }
            break;

        }
            
        case HOST_PAIR_VERIFY_PHASE:{
            //定时PING 50ms一次
            if (Get_SysTick_ms() - g_host_ctrl.last_ping_tick > 50) {
                RF_txQueue_Send(g_host_ctrl.resp_pkt.new_addr,(uint8_t*)&ping_pkt, sizeof(ping_pkt));
                g_host_ctrl.last_ping_tick = Get_SysTick_ms();
                uart_printf("Host: Sent PING,time:%d\n", g_host_ctrl.last_ping_tick);
            }
            //接收PONG
            if (RF_rxQueue_Recv(&recv_pong_pkt, &len, NULL) == 1) {
                if (recv_pong_pkt->cmd == CMD_PAIR_VERIFY_PONG &&
                    recv_pong_pkt->magic_number == slave_magic_number) {
                    
                    //收到 Pong，重置超时计时
                    g_host_ctrl.start_wait = Get_SysTick_ms();

                    g_host_ctrl.verify_cnt++;
                    uart_printf("Host: Verified %d/%d\n", g_host_ctrl.verify_cnt, g_host_ctrl.verify_target_count);

                    if (g_host_ctrl.verify_cnt >= g_host_ctrl.verify_target_count) {
                        uart_printf("Host: Pairing SUCCESS!\n");
                        printf_txrx_addr();
                        g_host_ctrl.state = HOST_PAIR_DONE;
                        return;
                    }
                }
            }

            // 如果切过来4000ms 还没收到 PONG，说明Slave没跟过来，回退
            if (Get_SysTick_ms() - g_host_ctrl.start_wait > 4000) {
                uart_printf("Host: Verify Timeout! Slave Lost. Revert.\n");
                RF_txQueue_Clear();
                g_host_ctrl.state = HOST_PAIR_WAIT_REQ;
            }
            break;

        }
        
        case HOST_PAIR_DONE:{
            uart_printf("Host: Pairing Process Success Completed.\n");
            printf_txrx_addr();
            save_ctx_to_flash();
            *flag = 0;//配对完成后，将flag设置为0，结束流程
            break;
        }
            
    }
}
