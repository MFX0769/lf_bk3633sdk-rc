#include <stdio.h>
#include <stdint.h>
#include "addr_pool.h"


//初始化地址池，传入保留ID列表
void __addrpool_init_internal(SingleByteAddrPool_t *pool, uint8_t *list, uint16_t count)
{
    if (!pool||count>ADDR_POOL_SIZE) return;

    memset(pool, 0, sizeof(SingleByteAddrPool_t));

    if (list != NULL && count > 0) {
        for (uint16_t i = 0; i < count; i++) {
            uint8_t id = list[i];
            //将id对应的位在 reserved_id 数组中置 1
            pool->reserved_id[id / 8] |= (1 << (id % 8));
        }
    }
}

/**
 * @brief 标记ID为已使用，将ID在位图上的对应位:第(id/8)字节的第(id%8)位置1
 */
void addrpool_occupy(SingleByteAddrPool_t* pool, uint8_t id) {
    //将对应位:第(id/8)字节的第(id%8)位置1
    pool->bitmap[id / 8] |= (1 << (id % 8));
}

/**
 * @brief 释放ID，将ID在位图上的对应位:第(id/8)字节的第(id%8)位置0
 */
void addrpool_free(SingleByteAddrPool_t* pool, uint8_t id) {
    pool->bitmap[id / 8] &= ~(1 << (id % 8));
}

/**
 * @brief 顺序查找到第一个可用的 ID (同时避开已占用和保留的ID)
 * @param[out] out_id 找到的可用 ID 存放位置
 * @return 0: 成功, -1: 地址池已满
 */
int8_t addrpool_alloc_addr_first(SingleByteAddrPool_t* pool, uint8_t *out_id) {
    if (!pool || !out_id) return -1;

    for (int i = 0; i < 32; i++) {
        // 计算当前字节的“不可用掩码”
        // 不可用 = (已被占用) 或 (被保留)
        uint8_t unavailable_mask = pool->bitmap[i] | pool->reserved_id[i];

        // 如果该字节所有位都不可用 既没空位，或者是保留位，直接跳过
        if (unavailable_mask == 0xFF) continue;

        for (int j = 0; j < 8; j++) {
            // 检查特定位是否“可用”
            // 只有当 unavailable_mask 的第 j 位为 0 时，才表示既没被占，也没保留
            if (!(unavailable_mask & (1 << j))) {
                
                // 找到可用 ID，标记占用
                pool->bitmap[i] |= (1 << j);
                *out_id = (uint8_t)(i * 8 + j);
                return 0;
            }
        }
    }
    return -1; // 找遍了所有字节，满
}


/**
 * @brief 随机分配一个可用的 ID 并标记占用
 * @param out_id 输出参数，返回分配到的ID
 * @return 0: 成功, -1: 失败(池满了)
 */
int8_t addrpool_alloc_addr_random(SingleByteAddrPool_t* pool, uint8_t *out_id) {
    // 静态计数器，增加不确定熵
    static uint8_t entropy_counter = 0;
    entropy_counter++;

    // 计算高熵起点 混合：系统时间 + 累加器 + 对象地址(不同pool地址不同)
    uint32_t seed = entropy_counter + (uint32_t)pool + Get_SysTick_ms();    
    uint8_t start_index = (uint8_t)((seed * 131)^(seed>>8)); 

    // 定义查找步长 (必须是奇数，推荐质数)；步长越大，相邻分配的 ID 离得越远
    const uint8_t STEP = 31; 

    // 3. 遍历查找
    for (int i = 0; i < 256; i++) {
        // 使用步长跳跃查找
        uint8_t curr_id = (start_index + (i * STEP)) % 256;
        
        uint8_t byte_idx = curr_id / 8;
        uint8_t bit_idx  = curr_id % 8;

        // 检查占用位图
        uint8_t occupied = pool->bitmap[byte_idx] & (1 << bit_idx);
        // 检查保留位图
        uint8_t reserved = pool->reserved_id[byte_idx] & (1 << bit_idx);

        if (!occupied && !reserved) {
            pool->bitmap[byte_idx] |= (1 << bit_idx);
            *out_id = curr_id;
            return 0; // 成功
        }
    }

    return -1;
}
