/*
	Copyright 2016 - 2019 Benjamin Vedder	benjamin@vedder.se

	This file is part of the VESC firmware.

	The VESC firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The VESC firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <stdint.h>
#include <stdbool.h>

#define DMA_SIZE     930
#define USE_DMA        0
#define SPI_BATCH_N    64
#define SPI_PIC_BATCH_N    64

// Data types
typedef enum {
    FRAME_SRC_REMOTE = 0,   // 遥控器发送端
    FRAME_SRC_PEDAL  = 1,   // 脚踏发送端
    FRAME_SRC_BATTERY= 2    // 电池发送端
} FrameSourceType_t;

// Data types
typedef enum
{
    MOTE_PACKET_BATT_LEVEL = 3,
    MOTE_PACKET_BUTTONS,
    MOTE_PACKET_ALIVE,
    MOTE_PACKET_FILL_RX_BUFFER,
    MOTE_PACKET_FILL_RX_BUFFER_LONG,
    MOTE_PACKET_PROCESS_RX_BUFFER,
    MOTE_PACKET_PROCESS_SHORT_BUFFER,
    MOTE_PACKET_PAIRING_INFO,
    MOTE_PACKET_START_STOP_CMD,
    MOTE_PACKET_STOP_CMD,
    MOTE_PACKET_SPEED_UP,
    MOTE_PACKET_SLOW_DOWN,
    MOTE_PACKET_HALL_THROTTLE_VALUE,
    MOTE_PACKET_CVT_DEBUG_BMT_START_STOP,
    MOTE_PACKET_CVT_DEBUG_BMT_SPEED_UP,
    MOTE_PACKET_CVT_DEBUG_BMT_SLOW_DOWN,
    MOTE_PACKET_PAS_STOP,
    MOTE_PACKET_CVT_DEBUG_SMT_REVERSE,
    MOTE_PACKET_CVT_DEBUG_SMT_STOP,
    MOTE_PACKET_MOTE_PACKET_IMU_DATA,
    MOTE_PACKET_ADC_HALL_THROTTLE,
    MOTE_PACKET_NRF_THORTTLE_RESPONSE,
    MOTE_PACKET_NRF_ALIVE,
    MOTE_PACKET_NRF_KEY_PRESS_RESPONSE,
    MOTE_PACKET_NRF_START_POWER_MOTOR,
    MOTE_PACKET_NRF_STOP_POWER_MOTOR,
	MOTE_PACKET_NRF_SPEEDUP_POWER_MOTOR,
    MOTE_PACKET_NRF_SLOWDOWN_POWER_MOTOR,
    MOTE_PACKET_PAS_IMU_DATA,
	MOTE_PACKET_GET_MOTOR_INFO,
    MOTE_PACKET_EBIKE_MODEL,
    MOTE_PACKET_PAS_UNLOCK,
    MOTE_PACKET_STATE_IDLE,
} MOTE_PACKET;

//typedef struct


typedef enum
{
    MOTE_REPLY_REMOTER = 11,
    MOTE_REPLY_KEY_PRESS,
    MOTE_REPLY_CADENCE,
    MOTE_REPLY_BATTERY,
} mote_reply_cmd;

//nrf remote control
typedef enum
{
	START_STOP_CMD = 8,
	STOP_CMD,
	SPEED_UP,
	SLOW_DOWN,
	SWITCH_2_NORMAL_MODEL,
	SWITCH_2_DEBUG_MODEL,
	SWITCH_2_SETTING_MODEL,
	SWITCH_2_LAST_MODEL,
	SWITCH_2_LAST_DEBUG_MODEL,
	SWITCH_2_BMT_MODEL,
	SWITCH_2_SMT_MODEL,
	SWITCH_2_DEBUG_SETTING_MODEL,
	ADC_HALL_THROTTLE,
	MOTE_PACKET_IMU_DATA,
	NRF_THORTTLE_RESPONSE,
	NRF_ALIVE,
	NRF_KEY_PRESS_RESPONSE
} Nrf_control_command;

typedef enum _REPLY_TYPE
{
	HALL_THROTTLE = 12,
	KEY_CMD
} reply_type;

typedef enum {
	RF_RT_TRANS_FAILED = 0,
    RF_RT_SUCCESS = 1,
    RF_RT_SUCCESS_MISMATCH_PIPE = 2,
    RF_RT_CONFLICT = -2
} pipe_status;

typedef enum _PAIRING_STATUS {
	UN_PAIRING = 0,
	IN_PAIRING,
	PAIRING_DONE,
    PAIRING_LOST
} rf_pairing_status;

typedef enum {
	BLOCK_SEND_SUCCESS = 1,
	BLOCK_SEND_HALL_V_UNCHANGE = -1,
	BLOCK_SEND_FAILED = -2,
    BLOCK_SEND_FRAME_TYPE_UNCHANGE = -3
} block_send_status;

typedef enum {
    CTL_INVALID_STATE = -1,
    CTL_MODEL_STOP = 0,
    CTL_MODEL_UNLOCK,
    CTL_MODEL_B1_LEVEL,
    CTL_MODEL_B2_LEVEL,
} control_model_state;

typedef enum {
    VOLTAGE_LEVEL_NORMAL,
    VOLTAGE_LEVEL_LOW,
    VOLTAGE_LEVEL_CRITICAL
} Voltage_level;



typedef struct {
    uint8_t frame_head;    // 帧头（固定值，比如0xAA）
    FrameSourceType_t src_type;  // 来源端类型
    uint8_t sub_cmd;       // 子命令（根据src_type来解释是哪一类子命令）
    uint16_t payload_len;  // 有效负载长度
    uint8_t *payload;      // 有效负载指针
    uint16_t crc;          // 校验和（或者CRC16）
} ProtocolFrame_t;

#endif /* DATATYPES_H_ */
