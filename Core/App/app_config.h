#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"

/* 主循环任务节拍：业务层每 10ms 扫描一次按键和状态。 */
#define APP_TICK_MS                         10U

/* 按键去抖和长按判定时间。 */
#define APP_BUTTON_DEBOUNCE_MS              30U
#define APP_BUTTON_LONG_PRESS_MS            2000U

/* UV 灯关闭后风扇继续运行 10s，用于余热散热。 */
#define APP_FAN_OFF_DELAY_MS                10000U

/* LCD 状态刷新周期和 UV 传感器采样周期。 */
#define APP_STATUS_REFRESH_MS               500U
#define APP_UV_SAMPLE_MS                    200U
#define APP_UV_LOG_MS                       500U
#define APP_UV_INIT_RETRY_MS                1000U
#define APP_REMAIN_LOG_MS                   10000U
#define APP_SETTING_TIMEOUT_MS              10000U

/* 定时设置：TIME+ / TIME- 每次调整 5 分钟。 */
#define APP_TIME_STEP_SECONDS               (5U * 60U)
#define APP_TIME_DEFAULT_SECONDS            0U
#define APP_TIME_MIN_SECONDS                0U
#define APP_TIME_MAX_SECONDS                (120U * 60U)

/* PWM 亮度设置：默认 80%，长按 TIME- 时按 10% 步进循环调整。 */
#define APP_DEFAULT_BRIGHTNESS_PERCENT      80U
#define APP_BRIGHTNESS_STEP_PERCENT         10U

/*
 * GPIO 有效电平配置。若后续硬件上拉/下拉方式改变，只需要改这里，
 * 上层状态机不需要跟着改。
 */
#define APP_BUTTON_ACTIVE_LEVEL             GPIO_PIN_RESET
#define APP_DOOR_OPEN_LEVEL                 GPIO_PIN_SET
#define APP_FUNCTION_SWITCH_ON_LEVEL        GPIO_PIN_SET

#endif
