#include "key_scan.h"
#include <string.h>
#include "user_config.h"

// 配置参数（以毫秒为单位）
#define KEY_DEBOUNCE_MS             20     // 默认消抖时间 20ms
#define KEY_DEFAULT_LONG_PRESS_MS   1000   // 默认长按时间 1s

// 按键控制结构体
typedef struct {
    uint8_t  gpio_pin;             // 对应的物理 GPIO Pin (0-39)
    uint16_t debounce_ms_acc;      // 消抖累计毫秒
    uint16_t press_ms;             // 按下累计毫秒
    uint16_t long_press_ms;        // 长按判定毫秒
    bool     enabled;              // 是否已启用/配置
    bool     active_state;         // true为高电平有效，false为低电平有效
    bool     stable_level;         // 当前稳定电平状态 (true=按下, false=释放)
    bool     long_press_reported;
    key_event_t event;             // 待处理事件
} key_ctrl_t;

static key_ctrl_t m_keys[KEY_ID_MAX];
//全局统一中断回调,中心化事件分发,根据内部通过id+envent实现业务逻辑
static key_callback_t m_key_callback = NULL;

void key_register_callback(key_callback_t cb)
{
    m_key_callback = cb;
}

static void trigger_event(key_id_t id, key_event_t event)
{
    m_keys[id].event = event;
    if (m_key_callback != NULL && event != KEY_EVT_NONE) {
        m_key_callback(id, event);
    }
}

void key_init(const key_config_t *p_config, uint8_t count)
{
    // 1. 清空所有按键状态
    memset(m_keys, 0, sizeof(m_keys));

    // 2. 遍历配置数组进行初始化
    if (p_config == NULL || count == 0) {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        key_id_t id = p_config[i].id;
        gpio_num_t pin = p_config[i].pin;

        if (id >= KEY_ID_MAX) {
            continue; // 忽略无效 ID
        }

        key_ctrl_t *p_key = &m_keys[id];

        p_key->gpio_pin = pin;
        p_key->enabled = true;
        
        // 设置长按时间
        uint16_t lp_ms = p_config[i].long_press_ms;
        if (lp_ms == 0) lp_ms = KEY_DEFAULT_LONG_PRESS_MS;
        p_key->long_press_ms = lp_ms;

        // 有效电平
        p_key->active_state = p_config[i].active_level ? true : false;

        // 3. 配置 GPIO
        // 根据有效电平选择合适的上下拉
        // uart_printf("KEY[%d] Pin config: phys_pin=%d, active_state=%d\r\n", 
        //     i, pin, p_key->active_state);
        gpio_config(pin, GPIO_INPUT, p_key->active_state ? GPIO_PULL_LOW : GPIO_PULL_HIGH);
        
        // 4. 初始化当前状态
        // 读取当前 GPIO 电平，设置初始状态以避免上电误触发
        uint8_t val = gpio_get_input(pin);
        
        // 计算当前是否按下：(读取值 == 激活电平)
        bool is_pressed = (val == (p_key->active_state ? 1 : 0));
        p_key->stable_level = is_pressed;
        p_key->debounce_ms_acc = 0;
        p_key->press_ms = 0;
        p_key->long_press_reported = false;
    }
}

void key_scan(uint16_t period_ms)
{
    if (period_ms == 0) {
        return;
    }
    for(int i = 0; i < KEY_ID_MAX; i++) {
        key_ctrl_t *p_key = &m_keys[i];
        
        // 跳过未启用的按键
        if (!p_key->enabled) {
            continue;
        }

        // 读取原始状态
        uint8_t val = gpio_get_input(p_key->gpio_pin);
        bool is_pressed_raw = (val == (p_key->active_state ? 1 : 0));
        
        // 消抖逻辑
        if (is_pressed_raw != p_key->stable_level) {
            p_key->debounce_ms_acc += period_ms;
            if (p_key->debounce_ms_acc >= KEY_DEBOUNCE_MS) {
                // 状态确认
                p_key->stable_level = is_pressed_raw;
                p_key->debounce_ms_acc = 0;
                
                if (p_key->stable_level) {
                    // 刚按下
                    trigger_event(i, KEY_EVT_DOWN);
                    p_key->press_ms = 0;
                    p_key->long_press_reported = false;
                } else {
                    // 刚释放
                    trigger_event(i, KEY_EVT_UP);
                    
                    if (p_key->long_press_reported) {
                        trigger_event(i, KEY_EVT_LONG_RELEASE);
                    } else {
                        // 如果不是长按，则判定为短按
                        trigger_event(i, KEY_EVT_SHORT_PRESS);
                    }
                }
            }
        } else {
            p_key->debounce_ms_acc = 0;
        }
        
        // 长按逻辑（仅在按下状态时检测）
        if (p_key->stable_level) {
            p_key->press_ms += period_ms;
            if (p_key->press_ms >= p_key->long_press_ms) {
                if (!p_key->long_press_reported) {
                    trigger_event(i, KEY_EVT_LONG_PRESS);
                    p_key->long_press_reported = true;
                } else {
                    trigger_event(i, KEY_EVT_LONG_HOLD); 
                }
            }
        }
    }
}

void key_scan_10ms(void)
{
    key_scan(10);
}

key_event_t key_get_event(key_id_t key)
{
    if (key >= KEY_ID_MAX) return KEY_EVT_NONE;
    if (!m_keys[key].enabled) return KEY_EVT_NONE;
    
    key_event_t evt = m_keys[key].event;
    m_keys[key].event = KEY_EVT_NONE; // 清除事件
    return evt;
}

bool key_is_pressed(key_id_t key)
{
    if (key >= KEY_ID_MAX) return false;
    if (!m_keys[key].enabled) return false;

    return m_keys[key].stable_level;
}

bool key_check_combo(uint32_t key_mask)
{
    for(int i = 0; i < KEY_ID_MAX; i++) {
        if (key_mask & (1 << i)) {
            // 如果按键未启用或未按下，则返回失败
            if (!m_keys[i].enabled || !m_keys[i].stable_level) {
                return false;
            }
        }
    }
    return true;
}
