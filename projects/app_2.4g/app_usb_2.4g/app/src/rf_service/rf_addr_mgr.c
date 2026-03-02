#include "rf_addr_mgr.h"
#include "bk3633_reglist.h"
#include "flash.h"
#include "uart.h"
#include "hal_drv_rf.h"
#include <string.h>
#include <stdlib.h> // for rand()
#include <stddef.h>
#include "addr_pool.h"
#include "rf_handler.h"

#include "driver_bk24.h"

#include "rf_pair.h" 
#include "user_config.h" 
#include "app.h"
#include "driver_timer.h"

#include "co_error.h"      // error definition
#include "rwip.h"
#include "ll.h"
#include "wdt.h"
#include "rwprf_config.h"




// -----------------------------------------------------------
// 配置宏
// -----------------------------------------------------------
#define FLASH_PAIR_ADDR       0x7E000
#define FLASH_MAGIC_NUM       0x5AA5BEEF

// Flash 操作相关宏 (移植自 rf_pair.c)
#define OP_IDX_PP             12   // Page Program 索引
#define OP_IDX_SE             13   // Sector Erase 索引
#define BIT_ADDRESS_SW        0
#define BIT_OP_TYPE_SW        24
#define BIT_OP_SW             29

// -----------------------------------------------------------
// 类型定义
// -----------------------------------------------------------

// Flash 存储结构 6通道独立管理
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         // 0x5AA5BEEF
    
    // Pipe 0 & 1: 完整 5 字节
    uint8_t pipe0_addr[5]; 
    uint8_t pipe1_addr[5]; 

    // Pipe 2-5: 仅存储 LSB (高 4 字节强制等于 Pipe 1)
    uint8_t pipe2_lsb;
    uint8_t pipe3_lsb;
    uint8_t pipe4_lsb;
    uint8_t pipe5_lsb;

    // 有效位标记 (Bit 0-5 分别代表 Pipe 0-5 是否有效)
    uint8_t valid_mask; 

    uint8_t padding[3];    // 对齐填充 (4 + 10 + 4 + 1 + 3 = 22 bytes)
    uint8_t checksum;      // 校验和
} PairInfo_t;
#pragma pack(pop)

// 地址管理上下文结构体
typedef struct {
    // 6 个通道的完整逻辑地址缓存
    uint8_t pipe_cfg[6][ADDR_LEN];
    uint8_t valid_mask; // Bit 0-5
    
    // Pipe0 备份区 用于 TX 临时切换
    uint8_t backup_pipe0[ADDR_LEN];
    uint8_t backup_valid;
} addr_manage_context_t;

// -----------------------------------------------------------
// 静态变量
// -----------------------------------------------------------
const uint8_t ADDR_DEFAULT_PAIR[ADDR_LEN] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

static addr_manage_context_t addr_manager;
// -----------------------------------------------------------
// 内部 Flash 辅助函数
// -----------------------------------------------------------
/**
 * @brief 强制解锁 Flash 写保护
 */
static void Flash_Force_Unlock(void) { flash_write_sr(1, 0x80); }

/**
 * @brief 强制擦除指定地址所在扇区
 * @param address 目标地址
 */
static void Flash_Erase_Force(uint32_t address) 
{
    Flash_Force_Unlock();
    GLOBAL_INT_DISABLE(); 
    while(REG_FLASH_OPERATE_SW & 0x80000000);

    REG_FLASH_OPERATE_SW = (  (address << BIT_ADDRESS_SW)
                            | (OP_IDX_SE << BIT_OP_TYPE_SW)
                            | (0x1 << BIT_OP_SW));
    while(REG_FLASH_OPERATE_SW & 0x80000000);

    flash_wp_all();
    GLOBAL_INT_RESTORE(); 
}

/**
 * @brief Flash 写入用户数据
 * @param address 写入起始地址
 * @param data 数据指针
 * @param len 写入长度
 */
static void Flash_Write_User_Data(uint32_t address, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t addr = address & (~0x1F); 
    uint32_t buf[8];                   
    uint8_t *pb = (uint8_t *)&buf[0];

    if (len == 0) return;

    Flash_Force_Unlock();
    GLOBAL_INT_DISABLE(); 

    while(len) 
    {
        if((address & 0x1F) || (len < 32)) {
            flash_read_data(pb, addr, 32);
        } else {
            memset(buf, 0xFF, 32); 
        }

        for(i = (address & 0x1F); i < 32; i++) {
            if(len) {
                pb[i] = *data++;
                address++;
                len--;
            }
        }

        while(REG_FLASH_OPERATE_SW & 0x80000000); 
        for (i=0; i<8; i++) REG_FLASH_DATA_SW_FLASH = buf[i]; 

        REG_FLASH_OPERATE_SW = (  (addr << BIT_ADDRESS_SW)
                                | (OP_IDX_PP << BIT_OP_TYPE_SW)
                                | (0x1 << BIT_OP_SW));
        
        while(REG_FLASH_OPERATE_SW & 0x80000000); 
        addr += 32; 
    }
    flash_wp_all(); 
    GLOBAL_INT_RESTORE(); 
}

/**
 * @brief 计算 PairInfo_t 校验和
 * @param info 结构体指针
 * @return 校验值
 */
static uint8_t calc_checksum(const PairInfo_t *info) {
    uint8_t sum = 0;
    const uint8_t *p = (const uint8_t *)info;
    // 只累计 checksum 字段之前的字节，避免把 checksum 自身算进去
    for (size_t i = 0; i < offsetof(PairInfo_t, checksum); i++) {
        sum += p[i];
    }
    return sum;
}

// -----------------------------------------------------------
// 辅助逻辑：地址生成与同步
// -----------------------------------------------------------

/**
 * @brief 生成随机字节
 * @return 0~255 随机值
 */
static uint8_t gen_random_byte(void) {

    static SingleByteAddrPool_t pool;
    static uint8_t inited = 0;
    uint8_t id = 0;
    if (!inited) {
        ADDR_POOL_INIT_NORESERVED(&pool);
        inited = 1;
    }

    if (addrpool_alloc_addr_random(&pool, &id) != 0) {
        //分配失败则重新初始化地址池重新分配
        ADDR_POOL_INIT_NORESERVED(&pool);
        if (addrpool_alloc_addr_random(&pool, &id) != 0) {
            return 0;
        }
    }
    return id;
}

/**
 * @brief 检查 LSB 是否与已配对通道冲突
 * @param target_lsb 待检查的 LSB
 * @param ignore_pipe 当前正在配对的 Pipe，忽略其自身
 * @return 1 冲突，0 不冲突
 */
static uint8_t is_lsb_conflict(uint8_t target_lsb, uint8_t ignore_pipe) {
    // 检查 Pipe 1 的 LSB (因为 Pipe 2-5 与 Pipe 1 共享高位，若 LSB 相同则完全冲突)
    if (ignore_pipe != 1 && (addr_manager.valid_mask & (1<<1))) {
        if (addr_manager.pipe_cfg[1][0] == target_lsb) return 1;
    }
    
    // 检查 Pipe 2-5
    for(int i=2; i<=5; i++) {
        if (i == ignore_pipe) continue;
        if (addr_manager.valid_mask & (1<<i)) {
            if (addr_manager.pipe_cfg[i][0] == target_lsb) return 1;
        }
    }
    return 0;
}

/**
 * @brief 将当前地址配置保存到 Flash
 */
void save_ctx_to_flash(void) {
    PairInfo_t info;
    memset(&info, 0, sizeof(info));
    info.magic = FLASH_MAGIC_NUM;
    
    memcpy(info.pipe0_addr, addr_manager.pipe_cfg[0], 5);
    memcpy(info.pipe1_addr, addr_manager.pipe_cfg[1], 5);
    
    info.pipe2_lsb = addr_manager.pipe_cfg[2][0];
    info.pipe3_lsb = addr_manager.pipe_cfg[3][0];
    info.pipe4_lsb = addr_manager.pipe_cfg[4][0];
    info.pipe5_lsb = addr_manager.pipe_cfg[5][0];
    
    info.valid_mask = addr_manager.valid_mask;
    info.checksum = calc_checksum(&info);
    
    uart_printf("[Addr] Saving Flash. Mask=0x%02X\r\n", info.valid_mask);
    Flash_Erase_Force(FLASH_PAIR_ADDR);
    Flash_Write_User_Data(FLASH_PAIR_ADDR, (uint8_t*)&info, sizeof(PairInfo_t));
}

// -----------------------------------------------------------
// 核心接口实现
// -----------------------------------------------------------

/**
 * @brief 地址管理模块初始化
 */
void app_addr_init(void)
{
    PairInfo_t info;
    memset(&addr_manager, 0, sizeof(addr_manager));
    
    // 1. 读取 Flash
    flash_read(0, FLASH_PAIR_ADDR, sizeof(PairInfo_t), (uint8_t*)&info, NULL);
    
    // 2. 校验
    if (info.magic == FLASH_MAGIC_NUM && info.checksum == calc_checksum(&info)) {
        uart_printf("[Addr] Load Success. Mask=0x%02X\r\n", info.valid_mask);
        uart_printf("[Addr] Info: pipe0=%02X%02X%02X%02X%02X\r\n", info.pipe0_addr[0], info.pipe0_addr[1], info.pipe0_addr[2], info.pipe0_addr[3], info.pipe0_addr[4]);
        uart_printf("[Addr] Info: pipe1=%02X%02X%02X%02X%02X\r\n", info.pipe1_addr[0], info.pipe1_addr[1], info.pipe1_addr[2], info.pipe1_addr[3], info.pipe1_addr[4]);
        addr_manager.valid_mask = info.valid_mask;
        
        // 恢复 Pipe 0
        if (info.valid_mask & (1<<0)) memcpy(addr_manager.pipe_cfg[0], info.pipe0_addr, 5);
        else memcpy(addr_manager.pipe_cfg[0], ADDR_DEFAULT_PAIR, 5);

        // 恢复 Pipe 1
        if (info.valid_mask & (1<<1)) memcpy(addr_manager.pipe_cfg[1], info.pipe1_addr, 5);
        else memcpy(addr_manager.pipe_cfg[1], ADDR_DEFAULT_PAIR, 5); // 这里的默认其实无意义，如果无效会被覆盖
        
        // 恢复 Pipe 2-5 (高位取 Pipe 1)
        uint8_t *base_high = &addr_manager.pipe_cfg[1][1]; // 取 Pipe 1 的高 4 字节
        
        // Helper lambda-like macro
        #define RESTORE_PIPE(idx, lsb) \
            addr_manager.pipe_cfg[idx][0] = lsb; \
            memcpy(&addr_manager.pipe_cfg[idx][1], base_high, 4); 

        RESTORE_PIPE(2, info.pipe2_lsb);
        RESTORE_PIPE(3, info.pipe3_lsb);
        RESTORE_PIPE(4, info.pipe4_lsb);
        RESTORE_PIPE(5, info.pipe5_lsb);
        
    } else {
        uart_printf("[Addr] Invalid Flash. Reset to Default.\r\n");
        app_addr_clear_all(); // 初始化为默认
    }
    
    addr_manager.backup_valid = 0;
}

/**
 * @brief 获取默认配对地址ADDR_DEFAULT_PAIR
 * @param buf 输出缓冲区
 */
void app_addr_get_default(uint8_t *buf)
{
    if (buf) 
        memcpy(buf, ADDR_DEFAULT_PAIR, ADDR_LEN);
}

/**
 * @brief 判断指定通道是否已配对
 * @param pipe_id 通道号 0-5
 * @return uint8_t 已配对，0 未配对
 */
uint8_t app_addr_is_pipe_paired(uint8_t pipe_id)
{
    if (pipe_id >= 6) 
        return 0;
    return (addr_manager.valid_mask & (1 << pipe_id)) ? 1 : 0;
}

/**
 * @brief 获取指定通道地址
 * @param pipe_id 通道号 0-5
 * @param buf 输出缓冲区
 */
void app_addr_get_pipe_addr(uint8_t pipe_id, uint8_t *buf)
{
    if (pipe_id >= 6 || !buf) return;
    
    // 如果该通道有效，返回缓存的地址
    // 如果无效，返回默认地址
    if (addr_manager.valid_mask & (1 << pipe_id)) {
        memcpy(buf, addr_manager.pipe_cfg[pipe_id], ADDR_LEN);
    } else {
        memcpy(buf, ADDR_DEFAULT_PAIR, ADDR_LEN);
    }
}

uint8_t app_addr_set_pipe0_addr(const uint8_t *addr)
{
    if (!addr) {
        return 0;
    }

    memcpy(addr_manager.pipe_cfg[0], addr, ADDR_LEN);
    addr_manager.valid_mask |= (1 << 0);
    return 1;
}

/**
 * @brief 为指定通道生成新配对地址
 * @param pipe_id 通道号 0-5
 * @param out_addr 输出生成的地址，可为 NULL
 * @return true 成功，false 失败
 */
uint8_t app_addr_genatate_for_pair(uint8_t pipe_id, uint8_t *out_addr)
{
    if (pipe_id >= 6) return 0;
    
    uart_printf("[Addr] Pairing Pipe %d...\r\n", pipe_id);

    // Pipe0或Pipe1独立地址
    if (pipe_id == 0 || pipe_id == 1) {
        // 生成全随机5字节地址
        for(int i=0; i<5; i++) 
            addr_manager.pipe_cfg[pipe_id][i] = gen_random_byte();
        
        // 标记有效
        addr_manager.valid_mask |= (1 << pipe_id);
        
        // 如果修改了Pipe1，需要同步更新Pipe2-5的高4字节
        if (pipe_id == 1) {
            for(int i=2; i<=5; i++) {
                memcpy(&addr_manager.pipe_cfg[i][1], &addr_manager.pipe_cfg[1][1], 4);
            }
        }
    }
    // Pipe 2-5 (依赖Pipe1)
    else {
        //检查 Pipe 1 是否有效
        if (!(addr_manager.valid_mask & (1 << 1))) {
            uart_printf("[Addr] Pipe 1 invalid, auto-generating Pipe 1 base...\r\n");
            //递归调用自己先生成Pipe1的地址
            app_addr_genatate_for_pair(1, NULL); 
        }
        
        // 复制Pipe1的高4字节到Pipe2-5
        memcpy(&addr_manager.pipe_cfg[pipe_id][1], &addr_manager.pipe_cfg[1][1], 4);
        
        // 生成不冲突的LSB
        uint8_t lsb;
        int retry = 0;
        do {
            lsb = gen_random_byte();
            retry++;
        } while (is_lsb_conflict(lsb, pipe_id) && retry < 100);
        
        if (retry >= 100) {
            uart_printf("[Addr] LSB generation failed (too many conflicts)\r\n");
            return 0;
        }
        
        addr_manager.pipe_cfg[pipe_id][0] = lsb;
        addr_manager.valid_mask |= (1 << pipe_id);
    }
    
    // 返回结果
    if (out_addr) 
        memcpy(out_addr, addr_manager.pipe_cfg[pipe_id], ADDR_LEN);

    return 1;
}

/**
 * @brief 清除所有配对信息
 */
void app_addr_clear_all(void)
{
    uart_printf("[Addr] Clearing all pairing info...\r\n");
    Flash_Erase_Force(FLASH_PAIR_ADDR);
    
    addr_manager.valid_mask = 0;
    for(int i=0; i<6; i++) {
        memcpy(addr_manager.pipe_cfg[i], ADDR_DEFAULT_PAIR, ADDR_LEN);
    }
}

/**
 * @brief 将当前地址配置应用到 RF 硬件
 */
void app_addr_apply_to_rf(void)
{
    // 应用Pipe0-5 (HAL层会自动只取 LSB)
    for(int i=0; i<6; i++) {
        HAL_RF_SetRxAddress(&hrf, i, addr_manager.pipe_cfg[i], ADDR_LEN);
    }
}

/**
 * @brief 发送前准备，备份当前临时切换Pipe0地址
 * @param tx_addr 目标发送地址
 */
void app_addr_tx_prepare(const uint8_t *tx_addr)
{
    uint8_t cur_pipe0[ADDR_LEN];

    if (!tx_addr) {
        return;
    }

    // 读取当前硬件 Pipe0；仅当临时改写 Pipe0 时才做一次备份
    if (HAL_RF_GetRxAddress(&hrf, 0, cur_pipe0) == HAL_RF_STATE_READY) {
        if (!addr_manager.backup_valid && memcmp(cur_pipe0, tx_addr, ADDR_LEN) != 0) {
            memcpy(addr_manager.backup_pipe0, cur_pipe0, ADDR_LEN);
            addr_manager.backup_valid = 1;
        }
    }

    // 发送前同时切换 TX 地址和 Pipe0 地址，保证 ACK 匹配
    HAL_RF_SetTxAddress(&hrf, (uint8_t *)tx_addr, ADDR_LEN);
    HAL_RF_SetRxAddress(&hrf, 0, (uint8_t *)tx_addr, ADDR_LEN);
}

/**
 * @brief 发送后恢复 Pipe0 地址
 */
void app_addr_tx_restore(void)
{
    if (addr_manager.backup_valid) {
        // 恢复 Pipe0 地址
        uart_printf("[Addr] Restoring Pipe 0 address to %02X:%02X:%02X:%02X:%02X\r\n", 
                   addr_manager.backup_pipe0[0], addr_manager.backup_pipe0[1],
                   addr_manager.backup_pipe0[2], addr_manager.backup_pipe0[3],
                   addr_manager.backup_pipe0[4]);   
        HAL_RF_SetRxAddress(&hrf, 0, addr_manager.backup_pipe0, ADDR_LEN);
        addr_manager.backup_valid = 0;
    }
}
