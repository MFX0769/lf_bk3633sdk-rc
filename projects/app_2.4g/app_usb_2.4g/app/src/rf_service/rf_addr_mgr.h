#ifndef _RF_ADDR_MGR_H_
#define _RF_ADDR_MGR_H_

#include <stdint.h>
#include <stdbool.h>

#define ADDR_LEN 5

// 默认配对地址定义 (硬编码)
extern const uint8_t ADDR_DEFAULT_PAIR[ADDR_LEN];

/**
 * @brief 地址管理模块初始化
 * 读取 Flash 中的配对信息，初始化内部状态。
 */
void app_addr_init(void);

/**
 * @brief 获取硬编码的默认配对地址
 * @param buf 输出缓冲区
 */
void app_addr_get_default(uint8_t *buf);

/**
 * @brief 检查指定通道是否已配对（有效）
 * @param pipe_id 0-5
 * @return uint8_t 已配对，1 未配对
 */
uint8_t app_addr_is_pipe_paired(uint8_t pipe_id);

/**
 * @brief 为指定通道执行配对 生成新地址
 * 1. 如果是 Pipe0或Pipe1：生成全随机 5 字节地址
 * 2. 如果是 Pipe 2-5：
 *    - 检查 Pipe 1 是否有效，若无效则先自动生成 Pipe 1 地址
 *    - 高 4 字节强制等于Pipe1
 *    - 低 1 字节随机生成，并确保不与当前其他有效通道冲突
 * 
 * @param pipe_id 0-5
 * @param out_addr [输出] 返回生成的最终地址 (5字节)
 * @return true 成功, false 失败
 */
uint8_t app_addr_genatate_for_pair(uint8_t pipe_id, uint8_t *out_addr);

/**
 * @brief 获取指定通道的当前地址
 * @param pipe_id 0-5
 * @param buf [输出] 5字节地址
 */
void app_addr_get_pipe_addr(uint8_t pipe_id, uint8_t *buf);

/**
 * @brief 用已配对地址更新 Pipe0，并标记为有效
 * @param addr 5字节地址
 * @return uint8_t 1 成功, 0 失败
 */
uint8_t app_addr_set_pipe0_addr(const uint8_t *addr);

/**
 * @brief 清除所有配对信息（恢复出厂）
 */
void app_addr_clear_all(void);

/**
 * @brief 将当前所有 Pipe 地址配置应用到 RF 硬件
 * 必须在 RF 初始化后调用一次，确保地址生效。
 */
void app_addr_apply_to_rf(void);

/**
 * @brief 将当前地址配置保存到 Flash
 */
void save_ctx_to_flash(void);
/**
 * @brief 发送前准备：临时切换 Pipe0 地址
 * 
 * 为了接收 ACK，Pipe0 地址通常需要设置为与 TX 目标地址一致。
 * 本函数会：
 * 1. 备份当前 Pipe0 地址
 * 2. 设置 RF TX 地址为 tx_addr
 * 3. 设置 RF Pipe0 地址为 tx_addr
 * 
 * @param tx_addr 发送目标地址
 */
void app_addr_tx_prepare(const uint8_t *tx_addr);

/**
 * @brief 发送后恢复：恢复 Pipe0 地址
 * 
 * 恢复 Pipe0 地址为备份值。
 */
void app_addr_tx_restore(void);

#endif // _RF_ADDR_MGR_H_
