# UV_Detector

基于 STM32G070CBTx 的 UV 检测与 UV 灯控制工程。项目使用 STM32 HAL、CMake 和 STM32 VS Code 扩展构建，当前没有 bootloader，固件直接烧录到主 Flash。

## 主要功能

- LTR390 UV 传感器采样
  - I2C 地址：`0x53`
  - 初始化时检查 `PART_ID = 0xB2`
  - 配置为 UVS Active 模式
  - 周期读取原始 UV 值，并通过 USART1 打印
- UV 灯 PWM 调光
  - TIM1 CH1 输出 PWM
  - 支持 0% 到 100% 亮度调整，步进 10%
- 检测倒计时
  - 上电默认检测时间为 `0:00`
  - 设置模式下通过 `TIME+` / `TIME-` 每次调整 5 分钟
  - 运行时每 10 秒通过串口打印一次剩余时间
- 门安全检测
  - 门打开时暂停 UV 输出
  - 门关闭且处于运行状态、剩余时间大于 0 时才允许 UV 输出
- 风扇联动
  - UV 输出开启时风扇开启
  - UV 输出关闭后风扇延时 10 秒关闭
- LCD 状态输出
  - 显示 UV 值、剩余时间、亮度、运行状态、传感器状态
- 串口调试
  - USART1 用作调试串口
  - 打印启动、按键、设置模式、剩余时间、UV 传感器状态和 I2C 诊断信息

## 外设与引脚

| 功能 | 外设/引脚 | 说明 |
| --- | --- | --- |
| 调试串口 | USART1 TX `PB6`, RX `PB7` | `115200 8N1` |
| UV 传感器 | I2C2 SCL `PA11`, SDA `PA12` | LTR390，7-bit 地址 `0x53` |
| UV PWM | TIM1 CH1 `PA8` | UV 灯亮度控制 |
| 风扇 | `PA1` | 普通 GPIO 输出 |
| LCD CS | `PA2` | GPIO 模拟 LCD 接口 |
| LCD WR | `PA3` | GPIO 模拟 LCD 接口 |
| LCD DATA | `PA4` | GPIO 模拟 LCD 接口 |
| LCD 背光 | `PA9` | 普通 GPIO 输出 |
| TIME+ | `PB10` | 按键，内部上拉，低电平有效 |
| FUNCTION | `PB11` | 设置模式/模式切换，内部上拉，低电平有效 |
| LIGHT | `PD0` | 启动/暂停/关闭，内部上拉，低电平有效 |
| TIME- | `PD2` | 按键，内部上拉，低电平有效 |
| 门安全输入 | `PB3` | 当前配置为高电平表示门打开 |
| UV_INPUT | `PA10` | 预留输入 |

## 按键使用

按键均为低电平有效，去抖时间 `30 ms`，长按时间 `2 s`。

### LIGHT_SWITCH

- 短按：
  - OFF / FINISHED 状态：开始检测
  - RUNNING 状态：暂停检测
  - PAUSED 状态：继续检测
- 长按 2 秒：
  - 关闭 UV 输出，回到 OFF 状态

注意：如果当前检测时间为 `0:00`，短按启动不会开始运行，串口会打印：

```text
[UV] Cannot start: remain time is 0:00
```

### FUNCTION_SWITCH

短按进入设置模式或切换设置项。设置模式超过 10 秒无操作会自动退出。

设置项循环顺序：

```text
TIME -> UV_UNIT -> BRIGHTNESS -> TIME
```

串口示例：

```text
[UV] Enter setting mode
[UV] Adjust mode: TIME
[UV] Adjust mode: UV_UNIT
[UV] Adjust mode: BRIGHTNESS
[UV] Exit setting mode
```

### TIME+ / TIME-

平时按 `TIME+` / `TIME-` 不调整参数。必须先按 `FUNCTION_SWITCH` 进入设置模式。

| 设置模式 | TIME+ | TIME- |
| --- | --- | --- |
| TIME | 增加 5 分钟 | 减少 5 分钟，最小 0 |
| UV_UNIT | UV 显示单位 +1 | UV 显示单位 -1，最小 1 |
| BRIGHTNESS | 亮度 +10% | 亮度 -10% |

## 串口日志

串口参数：

```text
115200 8N1
```

启动日志示例：

```text
[UV] Booting UV_Detector
[UV] HAL, clock, GPIO, I2C2, TIM1 and USART1 initialized
[UV] USART1 debug console ready: 115200 8N1
[UV] UV_App_Init begin
[UV] UV_App_Init done
[UV] Enter main loop
```

LTR390 正常日志：

```text
[UV] LTR390 init ok, PART_ID=0xB2, UVS active mode
[UV] LTR390 raw UV: 12345
```

LTR390/I2C 故障诊断日志：

```text
[UV] LTR390 init failed: no ack at 0x53, PART_ID=0x00
[UV] I2C2 scan begin, SCL=HIGH, SDA=HIGH
[UV] I2C2 scan found no devices
```

如果 I2C 扫描不到设备，优先检查：

- LTR390 `SCL` 是否接到 `PA11`
- LTR390 `SDA` 是否接到 `PA12`
- VCC/GND 是否接好
- 模块供电是否为 3.3V
- I2C 是否有上拉电阻，建议外接 `4.7k` 到 3.3V

## 构建与烧录

工程使用 CMake Presets：

- Debug 构建输出：`build/Debug/UV_Detector.elf`
- Release 构建输出：`build/Release/UV_Detector.elf`

VS Code 任务：

- `Build Debug`
- `Build Release`
- `Flash Debug`
- `Flash Release`

`Flash Debug` / `Flash Release` 会先构建，再通过 SWD 使用 STM32CubeProgrammer 烧录：

```text
port=SWD
erase all
download UV_Detector.elf
reset
```

## 代码结构

| 路径 | 说明 |
| --- | --- |
| `Core/App/uv_app.c` | 应用状态机、按键逻辑、倒计时、传感器采样、串口日志 |
| `Core/App/app_config.h` | 应用参数配置 |
| `Core/Bsp/ltr390.c` | LTR390 驱动 |
| `Core/Bsp/uv_pwm.c` | UV PWM 控制 |
| `Core/Bsp/lcd_status.c` | LCD 状态显示 |
| `Core/Bsp/board_io.c` | 风扇、背光、门检测等板级 IO |
| `Core/Services/button.c` | 按键去抖、短按、长按识别 |
| `Core/Src/usart.c` | USART1 初始化与 printf 重定向 |
| `Core/Src/i2c.c` | I2C2 初始化 |
| `.vscode/tasks.json` | VS Code 构建和烧录任务 |

## 关键配置

在 `Core/App/app_config.h` 中：

```c
#define APP_BUTTON_LONG_PRESS_MS            2000U
#define APP_TIME_STEP_SECONDS               (5U * 60U)
#define APP_TIME_DEFAULT_SECONDS            0U
#define APP_DEFAULT_BRIGHTNESS_PERCENT      80U
#define APP_BRIGHTNESS_STEP_PERCENT         10U
#define APP_SETTING_TIMEOUT_MS              10000U
```

