#include "uv_pwm.h"

/* 根据亮度百分比换算 TIM CCR，占空比为 0 时关闭 UV LED 输出。 */
static void UV_PWM_ApplyDuty(UV_PWM *pwm, uint8_t enabled)
{
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm->timer);
  uint32_t compare = 0U;

  if (enabled != 0U) {
    compare = ((arr + 1U) * pwm->brightness_percent) / 100U;
    if (compare > arr) {
      compare = arr;
    }
  }

  __HAL_TIM_SET_COMPARE(pwm->timer, pwm->channel, compare);
}

/* 启动 PWM 通道。当前工程使用 PA8 / TIM1_CH1。 */
HAL_StatusTypeDef UV_PWM_Init(UV_PWM *pwm, TIM_HandleTypeDef *timer, uint32_t channel)
{
  pwm->timer = timer;
  pwm->channel = channel;
  pwm->brightness_percent = 0U;
  pwm->started = 0U;
  UV_PWM_ApplyDuty(pwm, 0U);

  if (HAL_TIM_PWM_Start(timer, channel) != HAL_OK) {
    return HAL_ERROR;
  }

  pwm->started = 1U;
  return HAL_OK;
}

/* 设置亮度百分比，范围限制在 0~100%。 */
void UV_PWM_SetBrightness(UV_PWM *pwm, uint8_t percent)
{
  if (percent > 100U) {
    percent = 100U;
  }

  pwm->brightness_percent = percent;
}

/* 允许或禁止 UV PWM 输出；禁止时亮度设置会被保留。 */
void UV_PWM_Enable(UV_PWM *pwm, uint8_t enable)
{
  UV_PWM_ApplyDuty(pwm, enable);
}

/* 获取当前亮度设置，主要用于显示或调试。 */
uint8_t UV_PWM_GetBrightness(const UV_PWM *pwm)
{
  return pwm->brightness_percent;
}
