// /*
//  * RC调度器使用示例
//  *
//  * 1. 在main.c中添加头文件：
//  */
// #include "rc_scheduler.h"

// /*
//  * 2. 在main函数中使用：
//  */
// int main(void)
// {
//     // ... 其他初始化代码 ...

//     // 初始化RF Handler
//     RF_Handler_Init();

//     // 创建RC调度器实例（使用结构体，避免全局变量）
//     RC_Scheduler_t rc_sched;
//     RC_Scheduler_Init(&rc_sched);

//     // 配置设备地址
//     uint8_t esc_addr[5] = {0x10, 0x11, 0x36, 0x00, 0x00};
//     uint8_t bat_addr[5] = {0x10, 0x56, 0x24, 0xCD, 0x78};
//     RC_Scheduler_SetESCAddr(&rc_sched, esc_addr);
//     RC_Scheduler_SetBATAddr(&rc_sched, bat_addr);

//     // 配置任务周期（可选，已有默认值）
//     RC_Scheduler_SetESCPeriod(&rc_sched, 50);   // 电控50ms
//     RC_Scheduler_SetBATPeriod(&rc_sched, 700);  // 电池700ms

//     // 主循环
//     while(1) {
//         // 调用RC调度器主任务
//         RC_Scheduler_Task(&rc_sched);

//         // TODO: 在这里添加其他周期性任务
//         // 例如：
//         // - app_key_scan()
//         // - app_hall_update()
//         // - 根据霍尔值调用 RC_Scheduler_SendESCCmd(&rc_sched, throttle)
//         // - 定期调用 RC_Scheduler_SendBATQuery(&rc_sched)

//         // 检查是否可以进入sleep
//         if(RC_Scheduler_CanSleep(&rc_sched)) {
//             // 进入深度睡眠，20ms后唤醒
//             app_enter_sleep_with_wakeup_by_timer(20);
//         }
//     }
// }

// /*
//  * 3. 完整示例（带任务调度）：
//  */
// int main(void)
// {
//     // ... 初始化代码 ...
//     RF_Handler_Init();

//     // 创建RC调度器实例
//     RC_Scheduler_t rc_sched;
//     RC_Scheduler_Init(&rc_sched);

//     // 配置地址
//     uint8_t esc_addr[5] = {0x10, 0x11, 0x36, 0x00, 0x00};
//     uint8_t bat_addr[5] = {0x10, 0x56, 0x24, 0xCD, 0x78};
//     RC_Scheduler_SetESCAddr(&rc_sched, esc_addr);
//     RC_Scheduler_SetBATAddr(&rc_sched, bat_addr);

//     static uint32_t last_timestamp[10] = {0};

//     while(1) {
//         uint32_t now = Get_SysTick_ms();

//         // 10ms任务：按键扫描
//         if((now - last_timestamp[0]) >= 10) {
//             app_key_scan(10);
//             last_timestamp[0] = now;
//         }

//         // 50ms任务：霍尔更新和发送油门
//         if((now - last_timestamp[1]) >= 50) {
//             // app_hall_update();
//             // uint16_t throttle = get_throttle_value();
//             // RC_Scheduler_SendESCCmd(&rc_sched, throttle);
//             last_timestamp[1] = now;
//         }

//         // 700ms任务：查询电池
//         if((now - last_timestamp[2]) >= 700) {
//             RC_Scheduler_SendBATQuery(&rc_sched);
//             last_timestamp[2] = now;
//         }

//         // RC调度器主任务（处理RF收发）
//         RC_Scheduler_Task(&rc_sched);

//         // 检查是否可以进入sleep
//         if(RC_Scheduler_CanSleep(&rc_sched)) {
//             app_enter_sleep_with_wakeup_by_timer(20);
//         }
//     }
// }

// /*
//  * 模块化设计优势：
//  *
//  * 1. 使用结构体封装状态，避免全局变量
//  *    - RC_Scheduler_t 结构体包含所有状态
//  *    - 可以创建多个实例（如果需要）
//  *
//  * 2. 清晰的接口设计
//  *    - Init: 初始化
//  *    - Set*: 配置参数
//  *    - Send*: 发送命令
//  *    - Task: 主任务处理
//  *    - CanSleep: 检查sleep条件
//  *
//  * 3. 易于测试和维护
//  *    - 状态封装在结构体中
//  *    - 接口清晰，职责明确
//  */
