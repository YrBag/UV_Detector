#include "uv_app.h"
#include "app_config.h"
#include "board_io.h"
#include "button.h"
#include <stdio.h>
#include "i2c.h"
#include "lcd_status.h"
#include "ltr390.h"
#include "tim.h"
#include "uv_pwm.h"

typedef enum {
  UV_APP_STATE_OFF = 0,       /* 关灯待机。 */
  UV_APP_STATE_RUNNING,       /* UV 灯按 PWM 亮度输出，倒计时运行。 */
  UV_APP_STATE_PAUSED,        /* 用户短按暂停，保留剩余时间。 */
  UV_APP_STATE_FINISHED       /* 定时结束，等待下一次启动。 */
} UV_AppState;

typedef enum {
  UV_ADJUST_TIME = 0,
  UV_ADJUST_UNIT,
  UV_ADJUST_BRIGHTNESS
} UV_AdjustMode;

/* 应用层上下文：集中保存业务状态，避免各模块互相直接耦合。 */
typedef struct {
  UV_AppState state;
  UV_PWM uv_pwm;
  LTR390 uv_sensor;
  Button light_button;
  Button function_button;
  Button time_add_button;
  Button time_reduce_button;
  UV_AdjustMode adjust_mode;
  uint32_t configured_seconds;
  uint32_t remain_seconds;
  uint32_t last_tick_ms;
  uint32_t last_second_ms;
  uint32_t last_uv_sample_ms;
  uint32_t last_uv_log_ms;
  uint32_t last_uv_init_retry_ms;
  uint32_t last_status_ms;
  uint32_t last_remain_log_ms;
  uint32_t last_setting_ms;
  uint32_t fan_off_deadline_ms;
  uint32_t raw_uv;
  uint32_t uv_unit;
  uint8_t brightness_percent;
  uint8_t sensor_ok;
  uint8_t output_light_on;
  uint8_t setting_active;
} UV_App;

static UV_App app;

static const char *UV_App_ButtonEventName(ButtonEvent event)
{
  switch (event) {
    case BUTTON_EVENT_SHORT_PRESS:
      return "short";
    case BUTTON_EVENT_LONG_PRESS:
      return "long";
    case BUTTON_EVENT_NONE:
    default:
      return "none";
  }
}

static void UV_App_LogButtonEvent(const char *name, ButtonEvent event, uint32_t now_ms)
{
  if (event != BUTTON_EVENT_NONE) {
    printf("[UV] Button %s %s press at %lu ms\r\n",
           name,
           UV_App_ButtonEventName(event),
           (unsigned long)now_ms);
  }
}

static void UV_App_LogRemainingTime(const char *reason)
{
  uint32_t minutes = app.remain_seconds / 60U;
  uint32_t seconds = app.remain_seconds % 60U;

  printf("[UV] %s remain time: %lu:%02lu\r\n",
         reason,
         (unsigned long)minutes,
         (unsigned long)seconds);
}

static const char *UV_App_AdjustModeName(UV_AdjustMode mode)
{
  switch (mode) {
    case UV_ADJUST_TIME:
      return "TIME";
    case UV_ADJUST_UNIT:
      return "UV_UNIT";
    case UV_ADJUST_BRIGHTNESS:
      return "BRIGHTNESS";
    default:
      return "UNKNOWN";
  }
}

static void UV_App_LogAdjustMode(void)
{
  printf("[UV] Adjust mode: %s\r\n", UV_App_AdjustModeName(app.adjust_mode));
}

static void UV_App_ExitSettingMode(void)
{
  if (app.setting_active != 0U) {
    app.setting_active = 0U;
    printf("[UV] Exit setting mode\r\n");
  }
}

static void UV_App_UpdateSettingTimeout(uint32_t now_ms)
{
  if ((app.setting_active != 0U) &&
      ((now_ms - app.last_setting_ms) >= APP_SETTING_TIMEOUT_MS)) {
    UV_App_ExitSettingMode();
  }
}

static void UV_App_HandleFunctionButton(ButtonEvent event, uint32_t now_ms)
{
  if (event != BUTTON_EVENT_SHORT_PRESS) {
    return;
  }

  app.last_setting_ms = now_ms;

  if (app.setting_active == 0U) {
    app.setting_active = 1U;
    app.adjust_mode = UV_ADJUST_TIME;
    printf("[UV] Enter setting mode\r\n");
  } else if (app.adjust_mode == UV_ADJUST_BRIGHTNESS) {
    app.adjust_mode = UV_ADJUST_TIME;
  } else {
    app.adjust_mode = (UV_AdjustMode)((uint32_t)app.adjust_mode + 1U);
  }

  UV_App_LogAdjustMode();
}

/* 启动或继续运行：若倒计时已耗尽，则恢复到用户设置的定时时间。 */
static void UV_App_StartRun(void)
{
  if (app.configured_seconds == 0U) {
    printf("[UV] Cannot start: remain time is 0:00\r\n");
    return;
  }

  if (app.remain_seconds == 0U) {
    app.remain_seconds = app.configured_seconds;
  }

  app.state = UV_APP_STATE_RUNNING;
  app.last_remain_log_ms = HAL_GetTick();
  app.fan_off_deadline_ms = 0U;
}

/* 长按关灯：回到待机状态，并启动风扇 10s 延时关闭计时。 */
static void UV_App_Stop(uint32_t now_ms)
{
  app.state = UV_APP_STATE_OFF;
  app.remain_seconds = app.configured_seconds;
  app.fan_off_deadline_ms = now_ms + APP_FAN_OFF_DELAY_MS;
}

/* SW2 控灯逻辑：短按开灯/暂停/继续，长按关灯。 */
static void UV_App_HandleLightButton(ButtonEvent event, uint32_t now_ms)
{
  if (event == BUTTON_EVENT_LONG_PRESS) {
    UV_App_Stop(now_ms);
    return;
  }

  if (event != BUTTON_EVENT_SHORT_PRESS) {
    return;
  }

  if ((app.state == UV_APP_STATE_OFF) || (app.state == UV_APP_STATE_FINISHED)) {
    UV_App_StartRun();
  } else if (app.state == UV_APP_STATE_RUNNING) {
    app.state = UV_APP_STATE_PAUSED;
  } else {
    app.state = UV_APP_STATE_RUNNING;
  }
}

/* 定时调整：限制在最小/最大范围内，待机或结束状态同步显示剩余时间。 */
static void UV_App_AdjustTime(int32_t delta_seconds)
{
  int32_t next_seconds = (int32_t)app.configured_seconds + delta_seconds;

  if (next_seconds < (int32_t)APP_TIME_MIN_SECONDS) {
    next_seconds = (int32_t)APP_TIME_MIN_SECONDS;
  } else if (next_seconds > (int32_t)APP_TIME_MAX_SECONDS) {
    next_seconds = (int32_t)APP_TIME_MAX_SECONDS;
  }

  app.configured_seconds = (uint32_t)next_seconds;
  app.remain_seconds = app.configured_seconds;
}

/* 亮度调整：0~100% PWM，占空比按固定步进循环。 */
static void UV_App_AdjustBrightness(int32_t delta_percent)
{
  int32_t next = (int32_t)app.brightness_percent + delta_percent;

  if (next < 0) {
    next = 0;
  } else if (next > 100) {
    next = 100;
  }

  app.brightness_percent = (uint8_t)next;
  UV_PWM_SetBrightness(&app.uv_pwm, app.brightness_percent);
  printf("[UV] Brightness: %u%%\r\n", app.brightness_percent);
}

static void UV_App_AdjustUvUnit(int32_t delta)
{
  int32_t next = (int32_t)app.uv_unit + delta;

  if (next < 1) {
    next = 1;
  } else if (next > 1000) {
    next = 1000;
  }

  app.uv_unit = (uint32_t)next;
  printf("[UV] UV unit: %lu\r\n", (unsigned long)app.uv_unit);
}

static void UV_App_ScanI2cBus(void)
{
  uint8_t found = 0U;
  uint8_t addr;
  GPIO_PinState scl = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
  GPIO_PinState sda = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12);

  printf("[UV] I2C2 scan begin, SCL=%s, SDA=%s\r\n",
         (scl == GPIO_PIN_SET) ? "HIGH" : "LOW",
         (sda == GPIO_PIN_SET) ? "HIGH" : "LOW");
  for (addr = 1U; addr < 0x7FU; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1U), 1U, 5U) == HAL_OK) {
      printf("[UV] I2C2 device found at 0x%02X\r\n", addr);
      found = 1U;
    }
  }

  if (found == 0U) {
    printf("[UV] I2C2 scan found no devices\r\n");
  }
}

static uint8_t UV_App_InitUvSensor(void)
{
  uint8_t ok = (LTR390_Init(&app.uv_sensor, &hi2c2) == HAL_OK);

  if (ok != 0U) {
    (void)LTR390_SetThreshold(&app.uv_sensor, 5U, 30U);
    printf("[UV] LTR390 init ok, PART_ID=0x%02X, UVS active mode\r\n",
           app.uv_sensor.last_part_id);
  } else {
    printf("[UV] LTR390 init failed: %s, PART_ID=0x%02X\r\n",
           LTR390_GetErrorString(app.uv_sensor.last_error),
           app.uv_sensor.last_part_id);
    UV_App_ScanI2cBus();
  }

  return ok;
}

/* SW3/SW4 定时和亮度逻辑：TIME+ 加时，TIME- 短按减时、长按调光。 */
static void UV_App_HandleTimeButtons(ButtonEvent add_event, ButtonEvent reduce_event, uint32_t now_ms)
{
  if ((add_event == BUTTON_EVENT_NONE) && (reduce_event == BUTTON_EVENT_NONE)) {
    return;
  }

  if (app.setting_active == 0U) {
    return;
  }

  if (add_event == BUTTON_EVENT_SHORT_PRESS) {
    app.last_setting_ms = now_ms;
    if (app.adjust_mode == UV_ADJUST_TIME) {
      UV_App_AdjustTime((int32_t)APP_TIME_STEP_SECONDS);
      UV_App_LogRemainingTime("TIME+");
    } else if (app.adjust_mode == UV_ADJUST_UNIT) {
      UV_App_AdjustUvUnit(1);
    } else {
      UV_App_AdjustBrightness((int32_t)APP_BRIGHTNESS_STEP_PERCENT);
    }
  }

  if (reduce_event == BUTTON_EVENT_SHORT_PRESS) {
    app.last_setting_ms = now_ms;
    if (app.adjust_mode == UV_ADJUST_TIME) {
      UV_App_AdjustTime(-((int32_t)APP_TIME_STEP_SECONDS));
      UV_App_LogRemainingTime("TIME-");
    } else if (app.adjust_mode == UV_ADJUST_UNIT) {
      UV_App_AdjustUvUnit(-1);
    } else {
      UV_App_AdjustBrightness(-((int32_t)APP_BRIGHTNESS_STEP_PERCENT));
    }
  }
}

/* 安全输出条件：只有运行中、门关闭、剩余时间大于 0 时才允许 UV 输出。 */
static uint8_t UV_App_ShouldLightOutput(void)
{
  return (app.state == UV_APP_STATE_RUNNING) &&
         (BoardIO_IsDoorOpen() == 0U) &&
         (app.remain_seconds > 0U);
}

/* 倒计时逻辑：开门时暂停计时，关门后继续。 */
static void UV_App_UpdateTimer(uint32_t now_ms)
{
  if ((app.state != UV_APP_STATE_RUNNING) || (BoardIO_IsDoorOpen() != 0U)) {
    app.last_second_ms = now_ms;
    return;
  }

  while ((now_ms - app.last_second_ms) >= 1000U) {
    app.last_second_ms += 1000U;
    if (app.remain_seconds > 0U) {
      app.remain_seconds--;
    }

    if (app.remain_seconds == 0U) {
      /* 定时结束：关闭 UV 输出，并让风扇继续延时运行。 */
      app.state = UV_APP_STATE_FINISHED;
      app.fan_off_deadline_ms = now_ms + APP_FAN_OFF_DELAY_MS;
      break;
    }
  }
}

/* 输出控制：统一处理 UV PWM、风扇联动和风扇延时关闭。 */
static void UV_App_UpdateRemainLog(uint32_t now_ms)
{
  if ((now_ms - app.last_remain_log_ms) < APP_REMAIN_LOG_MS) {
    return;
  }

  app.last_remain_log_ms = now_ms;
  UV_App_LogRemainingTime("Periodic");
}

static void UV_App_UpdateOutputs(uint32_t now_ms)
{
  uint8_t light_on = UV_App_ShouldLightOutput();
  uint8_t fan_on = light_on;

  UV_PWM_SetBrightness(&app.uv_pwm, app.brightness_percent);
  UV_PWM_Enable(&app.uv_pwm, light_on);

  if (light_on != 0U) {
    /* 灯亮时风扇立即运行，并取消历史延时关闭计时。 */
    app.fan_off_deadline_ms = 0U;
  } else if (app.output_light_on != 0U) {
    /* 检测到 UV 输出从亮变灭，启动 10s 风扇延时。 */
    app.fan_off_deadline_ms = now_ms + APP_FAN_OFF_DELAY_MS;
    fan_on = 1U;
  } else if ((app.fan_off_deadline_ms != 0U) &&
             ((int32_t)(now_ms - app.fan_off_deadline_ms) < 0)) {
    fan_on = 1U;
  }

  BoardIO_SetFan(fan_on);
  app.output_light_on = light_on;
}

/* 周期读取 LTR390 原始 UV 数据，失败时保留错误状态用于显示或保护逻辑。 */
static void UV_App_UpdateUvSample(uint32_t now_ms)
{
  if (LTR390_IsPresent(&app.uv_sensor) == 0U) {
    if ((now_ms - app.last_uv_init_retry_ms) >= APP_UV_INIT_RETRY_MS) {
      app.last_uv_init_retry_ms = now_ms;
      app.sensor_ok = UV_App_InitUvSensor();
    }
    return;
  }

  if ((now_ms - app.last_uv_sample_ms) < APP_UV_SAMPLE_MS) {
    return;
  }

  app.last_uv_sample_ms = now_ms;
  app.sensor_ok = (LTR390_ReadRawUv(&app.uv_sensor, &app.raw_uv) == HAL_OK);
  if ((app.sensor_ok != 0U) &&
      ((now_ms - app.last_uv_log_ms) >= APP_UV_LOG_MS)) {
    uint32_t uvi_x100 = LTR390_RawUvToUviX100(app.raw_uv);
    uint32_t uw_cm2_x100 = LTR390_RawUvToUwCm2X100(app.raw_uv);
    uint32_t w_m2_x1000 = LTR390_UwCm2X100ToWM2X1000(uw_cm2_x100);
    uint32_t mw_cm2_x1000000 = LTR390_UwCm2X100ToMwCm2X1000000(uw_cm2_x100);

    app.last_uv_log_ms = now_ms;
    printf("[UV] raw:%lu, UVI:%lu.%02lu, %lu.%02lu uW/cm2, %lu.%06lu mW/cm2, %lu.%03lu W/m2\r\n",
           (unsigned long)app.raw_uv,
           (unsigned long)(uvi_x100 / 100U),
           (unsigned long)(uvi_x100 % 100U),
           (unsigned long)(uw_cm2_x100 / 100U),
           (unsigned long)(uw_cm2_x100 % 100U),
           (unsigned long)(mw_cm2_x1000000 / 1000000U),
           (unsigned long)(mw_cm2_x1000000 % 1000000U),
           (unsigned long)(w_m2_x1000 / 1000U),
           (unsigned long)(w_m2_x1000 % 1000U));
  } else if (app.sensor_ok == 0U) {
    printf("[UV] LTR390 read failed: %s\r\n",
           LTR390_GetErrorString(app.uv_sensor.last_error));
  }
}

/* 将内部状态转换成 LCD 可显示的状态枚举。 */
static LcdStatusState UV_App_GetLcdState(void)
{
  if ((app.state == UV_APP_STATE_RUNNING) && (BoardIO_IsDoorOpen() != 0U)) {
    return LCD_STATUS_DOOR_OPEN;
  }

  switch (app.state) {
    case UV_APP_STATE_RUNNING:
      return LCD_STATUS_RUNNING;
    case UV_APP_STATE_PAUSED:
      return LCD_STATUS_PAUSED;
    case UV_APP_STATE_FINISHED:
      return LCD_STATUS_FINISHED;
    case UV_APP_STATE_OFF:
    default:
      return LCD_STATUS_OFF;
  }
}

/* LCD 周期刷新：显示 UV 原始值、剩余时间、亮度和运行状态。 */
static void UV_App_UpdateStatus(uint32_t now_ms)
{
  uint32_t display_uv;

  if ((now_ms - app.last_status_ms) < APP_STATUS_REFRESH_MS) {
    return;
  }

  app.last_status_ms = now_ms;
  display_uv = app.raw_uv / app.uv_unit;
  LCD_Status_Show(UV_App_GetLcdState(),
                  app.remain_seconds,
                  app.brightness_percent,
                  display_uv,
                  app.sensor_ok);
}

/* 应用初始化：初始化输出、PWM、UV 传感器、按键服务和 LCD 状态层。 */
void UV_App_Init(void)
{
  uint32_t now_ms = HAL_GetTick();

  app.state = UV_APP_STATE_OFF;
  app.adjust_mode = UV_ADJUST_TIME;
  app.configured_seconds = APP_TIME_DEFAULT_SECONDS;
  app.remain_seconds = APP_TIME_DEFAULT_SECONDS;
  app.last_tick_ms = now_ms;
  app.last_second_ms = now_ms;
  app.last_uv_sample_ms = now_ms;
  app.last_uv_log_ms = now_ms;
  app.last_uv_init_retry_ms = now_ms;
  app.last_status_ms = now_ms;
  app.last_remain_log_ms = now_ms;
  app.last_setting_ms = now_ms;
  app.fan_off_deadline_ms = 0U;
  app.raw_uv = 0U;
  app.uv_unit = 1U;
  app.brightness_percent = APP_DEFAULT_BRIGHTNESS_PERCENT;
  app.output_light_on = 0U;
  app.setting_active = 0U;

  BoardIO_SetFan(0U);
  BoardIO_SetLcdBacklight(1U);

  /* PA8 / TIM1_CH1 为唯一 UV LED PWM 调光输出。 */
  if (UV_PWM_Init(&app.uv_pwm, &htim1, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  UV_PWM_SetBrightness(&app.uv_pwm, app.brightness_percent);
  UV_PWM_Enable(&app.uv_pwm, 0U);

  /* LTR390 使用当前 CubeMX 配置好的 I2C2 句柄。 */
  app.sensor_ok = UV_App_InitUvSensor();

  /* SW2：控灯开关，短按开灯/暂停，长按关灯。 */
  Button_Init(&app.light_button,
              LIGHT_SWITCH_GPIO_Port,
              LIGHT_SWITCH_Pin,
              APP_BUTTON_ACTIVE_LEVEL,
              APP_BUTTON_DEBOUNCE_MS,
              APP_BUTTON_LONG_PRESS_MS,
              now_ms);
  /* SW3：定时增加。 */
  Button_Init(&app.function_button,
              FUNCTION_SWITCH_GPIO_Port,
              FUNCTION_SWITCH_Pin,
              APP_BUTTON_ACTIVE_LEVEL,
              APP_BUTTON_DEBOUNCE_MS,
              APP_BUTTON_LONG_PRESS_MS,
              now_ms);
  Button_Init(&app.time_add_button,
              TIME_ADD_GPIO_Port,
              TIME_ADD_Pin,
              APP_BUTTON_ACTIVE_LEVEL,
              APP_BUTTON_DEBOUNCE_MS,
              APP_BUTTON_LONG_PRESS_MS,
              now_ms);
  /* SW4：定时减少；长按用于亮度调整。 */
  Button_Init(&app.time_reduce_button,
              TIME_REDUCE_GPIO_Port,
              TIME_REDUCE_Pin,
              APP_BUTTON_ACTIVE_LEVEL,
              APP_BUTTON_DEBOUNCE_MS,
              APP_BUTTON_LONG_PRESS_MS,
              now_ms);

  LCD_Status_Init();
  LCD_Status_Show(LCD_STATUS_OFF,
                  app.remain_seconds,
                  app.brightness_percent,
                  app.raw_uv,
                  app.sensor_ok);
}

/* 主循环任务入口：非阻塞运行，适合直接放在 while(1) 中。 */
void UV_App_Task(void)
{
  uint32_t now_ms = HAL_GetTick();
  ButtonEvent light_event;
  ButtonEvent function_event;
  ButtonEvent add_event;
  ButtonEvent reduce_event;

  if ((now_ms - app.last_tick_ms) < APP_TICK_MS) {
    return;
  }
  app.last_tick_ms = now_ms;

  light_event = Button_Update(&app.light_button, now_ms);
  function_event = Button_Update(&app.function_button, now_ms);
  add_event = Button_Update(&app.time_add_button, now_ms);
  reduce_event = Button_Update(&app.time_reduce_button, now_ms);

  UV_App_LogButtonEvent("LIGHT", light_event, now_ms);
  UV_App_LogButtonEvent("FUNCTION", function_event, now_ms);
  UV_App_LogButtonEvent("TIME+", add_event, now_ms);
  UV_App_LogButtonEvent("TIME-", reduce_event, now_ms);

  UV_App_HandleLightButton(light_event, now_ms);
  UV_App_UpdateSettingTimeout(now_ms);
  UV_App_HandleFunctionButton(function_event, now_ms);
  UV_App_HandleTimeButtons(add_event, reduce_event, now_ms);
  UV_App_UpdateTimer(now_ms);
  UV_App_UpdateRemainLog(now_ms);
  UV_App_UpdateUvSample(now_ms);
  UV_App_UpdateOutputs(now_ms);
  UV_App_UpdateStatus(now_ms);
}
