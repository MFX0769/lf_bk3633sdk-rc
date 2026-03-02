#ifndef __RF_PAIR_H__
#define __RF_PAIR_H__
#include <stdint.h>


#define PAIR_CH_DEFAULT           78
#define slave_magic_number        0x5A
#define host_magic_number         0xA5

//用户flash：0x7E000~0x7F000
//Flash存储配置配对信息的位置
// 已迁移至 app_addr_manage 模块统一管理

typedef enum {
    SLAVE_PAIR_IDLE = 0,
    SLAVE_PAIR_SEND_REQ,      // 发送配对请求
    SLAVE_PAIR_WAIT_RESP,     // 等待配对响应 (超时即重发)
    SLAVE_PAIR_VERIFY_PHASE,  // 新地址验证 (被动回复 PONG)
    SLAVE_PAIR_DONE
} slave_pair_state_t;


typedef enum {
    HOST_PAIR_IDLE = 0,
    HOST_PAIR_WAIT_REQ,       // 等待配对请求
    HOST_PAIR_SEND_RESP,      // 突发发送响应 (Burst)
    HOST_PAIR_WAIT_TX_DONE,   // 等待发送完成
    HOST_PAIR_VERIFY_PHASE,   // 新地址验证 (主动 PING，超时则回退)
    HOST_PAIR_DONE
} host_pair_state_t;



// 命令码
typedef enum {
    CMD_PAIR_REQ = 0x10,  // Slave -> Host
    CMD_PAIR_RESP = 0x20, // Host -> Slave 
    
    // 握手验证
    CMD_PAIR_VERIFY_PING = 0x40,  // Host -> Slave
    CMD_PAIR_VERIFY_PONG = 0x50   // Slave -> Host
} rf_pair_cmd_t;


#pragma pack(1)
typedef struct {
    uint8_t cmd;
    uint32_t slave_id;
} pair_req_pkt;

typedef struct {
    uint8_t cmd;
    uint8_t new_addr[5]; 
    uint8_t new_chn;
} pair_resp_pkt;

typedef struct {
    uint8_t cmd;
    uint8_t magic_number;
} pair_verify_pkt; 
#pragma pack()


typedef struct {
    slave_pair_state_t state;
    uint32_t start_wait;
    uint32_t global_start_tick; // 总超时计时
    uint8_t is_running;

    pair_resp_pkt resp_pkt; // 缓存接收到的RESP包
    uint32_t verify_cnt;               // ping-pong成功计数
    uint32_t verify_target_count;      // 目标ping-pong次数
    uint8_t reach_goal_flag;           // 达到目标计数标志
    uint32_t last_ping_recv_timestamp; // 上次收到ping的时间戳
} SlavePairCtrl_t;




typedef struct {
    host_pair_state_t state;
    uint8_t is_running;
    
    uint32_t start_wait;        // 阶段超时
    uint32_t global_start_tick; // 总超时
    uint32_t last_ping_tick;    // Ping 频率控制
    
    pair_resp_pkt resp_pkt;     // 缓存发送RESP包
    uint32_t txds_snapshot;     // 发送计数快照
    
    uint32_t verify_cnt;               // ping-pong成功计数
    uint32_t verify_target_count;      // 目标ping-pong次数，小于slave的目标ping-pong次数
} HostPairCtrl_t;

extern uint8_t slave_pair_success_flag;
extern uint8_t host_pair_success_flag;

// 非阻塞配对接口
void Slave_Pairing_Task(uint8_t *flag);
void Host_Pairing_Task(uint8_t *flag);

//配对信息存取接口
//已废弃，使用 app_addr_manage 模块接口
//void Load_Pair_Info(void);          
//void Save_Pair_Info(uint8_t *addr); 
//void Clear_Pair_Info(void);         



#endif // __RF_PAIR_H__
