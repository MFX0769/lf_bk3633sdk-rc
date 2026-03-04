#include "bat_protocol.h"
#include "rf_handler.h"
#include "user_config.h"



uint8_t checksum8(const uint8_t *data, uint8_t length)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum & 0xFF;
}
uint16_t crc16_modbus(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}


//目前只接收并处理电池动态信息包发给遥控
void Protocol_ParseByte(my_queue_t* uart_rx_queue)
{
    static uint8_t temp_Parsebuf[sizeof(BatteryDynamicInfo_t)];
    static uint16_t counts=0;
    static uint16_t count2=0;
    my_queue_t *q = uart_rx_queue; 
    while (1)
    {
        //uart_printf("---------------enter %d times----------------\r\n", counts++);
        // 队列里的数据够不够协议头的最小长度 (至少要能看到 length 字节)
        // Header(2) + Cmd(1) + Addr(1) + Len(1) = 5 字节
        if (queue_get_counts(q) < 5) {
            //uart_printf("Parse: 1not enough data\r\n");
            break; // 数据不够，直接退出，等下次
        }
        
        // 检查帧头 (AA 55)
        uint8_t h1,h2;
        queue_peek_at(q, 0, &h1);
        queue_peek_at(q, 1, &h2);

        if (h1 != FRAME_HEADER_1 || h2 != FRAME_HEADER_2) {
            //uart_printf("Parse: 2,frame header error\r\n");
            queue_remove(q, 1);//头不对，移除1个字节，继续下次循环找头
            continue; 
        }
        // 3. 头匹配了，查看命令码，这个解包只处理动态信息
        uint8_t cmd_byte;
        queue_peek_at(q, 2, &cmd_byte); // 读取命令码
        if(cmd_byte != UPCMD_DYNAMIC){
            //不是我们要的包，移除1个字节，继续下次循环找头
            //uart_printf("Parse: 3,cmd error\r\n");
            queue_remove(q, 1);//不是我们要的包，移除1个字节，继续下次循环找头
            continue; 
        }
        uint8_t addr_byte;
        queue_peek_at(q, 3, &addr_byte);
        // 3. 命令码匹配了，查看长度字节 (Offset 4)
        uint8_t data_len;
        queue_peek_at(q, 4, &data_len);
        uint8_t recv_chksum;
        queue_peek_at(q, 5, &recv_chksum);

        uint8_t calc_sum = h1 + h2 + cmd_byte + addr_byte + data_len;
        if(recv_chksum != calc_sum){
            //uart_printf("Parse: 4,checksum error rec:%02X calc:%02X\r\n", recv_chksum, calc_sum);
            queue_remove(q, 1);//帧头校验和错误，移除1个字节，继续下次循环找头
            continue; 
        }

        // 4. 计算这一包完整数据应该有多长 8+46=54
        uint16_t total_frame_len = PROTOCOL_OVERHEAD + data_len; 

        // 5. 检查队列里是否有足够的数据
        if (queue_get_counts(q) < total_frame_len) 
        {
            // 这是一个断包（还没收完），保留数据不动，退出函数
            //uart_printf("Parse: 5,not full data now:%d\r\n", queue_get_counts(q));
            break; 
        }

        //数据完整,从队列里Pop出来到线性数组
        for (uint16_t i = 0; i < total_frame_len; i++){
            queue_pop(q, &temp_Parsebuf[i]);
        }

        // 校验CRC16
        //低字节在前，高字节在后
        uint16_t recv_crc = (uint16_t) (temp_Parsebuf[total_frame_len-1] << 8) | (temp_Parsebuf[total_frame_len - 2]&0xFF);
        uint16_t calc_crc = crc16_modbus(temp_Parsebuf, total_frame_len - 2);
        if(recv_crc != calc_crc){
            //uart_printf("Parse: 6,CRC16 error rec:%x cal:%x\r\n",recv_crc,calc_crc);
            
            // for(uint8_t i=0;i<sizeof(BatteryDynamicInfo_t);i++){
            //     uart_printf("%02X ",temp_Parsebuf[i]);
            // }
            // uart_printf("\r\n");


            continue; // CRC校验失败，丢弃这包数据，继续下次循环找头
        }else{
            //成功接收并校验一包完整数据，进行处理，如果考虑解耦，可以在其他地方处理
            BatteryDynamicInfo_t *p_dynamic_info = (BatteryDynamicInfo_t *)&temp_Parsebuf[0];
            Bat_Soc_t bat_soc={
                .header = 0xAA,
                .cmd = UPCMD_DYNAMIC,
                .length = 0x01,
                .soc = p_dynamic_info->soc,
            };
            bat_soc.crc16 = crc16_modbus((uint8_t *)&bat_soc, sizeof(Bat_Soc_t)-2);

            //push到RF发送队列
            uart_printf("push_to_rf:%dtimes\r\n", count2++);
            //RF_txQueue_Send(&bat_soc, sizeof(Bat_Soc_t));
            //RF_txQueue_Send(&bat_soc, 32);

            uint8_t tmp_buf[sizeof(Bat_Soc_t)] = {0};
            memcpy(tmp_buf, &bat_soc, sizeof(Bat_Soc_t));
            // uart_printf("rf_send_data=");
            // for(int i=0;i<sizeof(Bat_Soc_t);i++)
            //     uart_printf("%x,",tmp_buf[i]);
            // uart_printf("\r\n");
            uart_printf("soc=%d%%\r\n", bat_soc.soc);
            uart0_printf("soc=%d%%\r\n", bat_soc.soc);
            // RF_txQueue_Send(tmp_buf, sizeof(Bat_Soc_t));
            // RF_txQueue_Send(RF_REMOTE_ADDR, tmp_buf, sizeof(Bat_Soc_t));
        }
    }

}