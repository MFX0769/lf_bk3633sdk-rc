#ifndef __APP_SLEEP_H__
#define __APP_SLEEP_H__

#include <stdint.h>
#include "aon_rtc.h"

void app_enter_sleep_with_wakeup_by_timer(uint32_t sleep_ms,uint8_t sleep_flag);

#endif // SLEEP_H