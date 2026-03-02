#ifndef __ADDR_POOL_H__
#define __ADDR_POOL_H__
#include <stdint.h>

/* 地址池管理模块 */
//实际通信地址由base组地址+后缀ID组成，base地址固定，后缀ID(0~255)由地址池分配
#define ADDR_POOL_SIZE      256   // 总容量 256 个
#define ADDR_POOL_START_ID  0x00  // 从 0x00 开始
#define ADDR_BASE_BYTE0     0x11  // 你的固定前缀
#define ADDR_BASE_BYTE1     0x22
#define ADDR_BASE_BYTE2     0x33
#define ADDR_BASE_BYTE3     0x44

//用户使用的初始化宏：有保留ID
//eg: ADDR_POOL_INIT(&my_pool, 0x00, 0xFF);
#define ADDR_POOL_INIT(pool_ptr, ...) \
    __addrpool_init_internal(pool_ptr, (uint8_t[]){__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__}))

//用户使用的初始化宏：无保留ID
//eg: ADDR_POOL_INIT_NORESERVED(&my_pool);
#define ADDR_POOL_INIT_NORESERVED(pool_ptr) \
    __addrpool_init_internal(pool_ptr, NULL, 0)


typedef struct {
    uint8_t bitmap[32]; //32字节占用位图 表示256个地址使用情况
    uint8_t reserved_id[32]; //32字节保留位图 表示256个地址预留情况
} SingleByteAddrPool_t;

//内部函数，用户不直接调用该函数，使用宏 ADDR_POOL_INIT 初始化地址池
void __addrpool_init_internal(SingleByteAddrPool_t *pool, uint8_t *list, uint16_t count);

void addrpool_occupy(SingleByteAddrPool_t* pool, uint8_t id);
void addrpool_free(SingleByteAddrPool_t* pool, uint8_t id);
int8_t addrpool_alloc_addr_first(SingleByteAddrPool_t* pool,uint8_t *out_id) ;
int8_t addrpool_alloc_addr_random(SingleByteAddrPool_t* pool,uint8_t *out_id) ;


#endif // __ADDR_POOL_H__