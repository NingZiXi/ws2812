# ws2812 — STM32 HAL WS2812 LED 驱动库

> C API。SPI + DMA 硬件生成 WS2812 时序，TIM ISR 翻相，main loop 零负担，库自带中断接管。

## ✨ 特性

- ⚡ **SPI + DMA**，3-bit 编码把 WS2812 时序转给 SPI 硬件
- 🔁 **TIM ISR 内翻相 + 启动 DMA**，main loop 完全空转
- 🎨 **C API**（`ws2812_t` 结构体 + 函数）
- 🧩 **多实例支持**（最多 4 个 `ws2812_t`，按 htim/hspi 路由）
- 🪝 **自动接管 HAL weak 回调**（app 不写 `HAL_TIM_PeriodElapsedCallback` / `HAL_SPI_TxCpltCallback`）
- 💾 ~1 KB FLASH，单实例 ~80 B RAM + 编码缓冲

## 🔧 硬件

| 项 | 要求 |
|---|---|
| MCU | STM32G0/F4/H7 等（有 SPI + DMA + 基础 TIM 即可） |
| SPI | master，**Direction = 1LINE**，MOSI 接 DIN，Mode 0，MSB first，2~4 MHz |
| DMA | SPI TX 通道，Byte 宽 |
| TIM | 1 个基础定时器，用于 blink |

## 🚀 用法

### 📦 FetchContent（推荐）

```cmake
FetchContent_Declare(
    ws2812
    # 默认走 Gitee 镜像（国内访问快）；如需 GitHub，把下一行注释掉、放开下一行
    GIT_REPOSITORY https://gitee.com/nzxhg/ws2812.git
    #GIT_REPOSITORY https://github.com/NingZiXi/ws2812.git     # 备选：境外 / GitHub 直连
    GIT_TAG        v2.0.0
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Lib/ws2812
)
FetchContent_MakeAvailable(ws2812)
```

### 🔧 CubeMX

- **SPI**：master，Data 8，**Direction = 1LINE**（必须，否则 MOSI 不驱动），Mode 0，MSB first，Baud 4 Mbps（prescaler /16）
- **SPI DMA**：TX，Byte，Normal，NVIC enable
- **GPIO**：MOSI 设 `SPIx_MOSI` AF
- **TIM**：Internal Clock，NVIC enable update 中断

> 💡 CubeMX 生成的 `MX_SPIx_Init()` / `MX_TIMy_Init()` 创建 handle，**库不创建硬件**。

### 💻 应用代码

```c
#include "ws2812.h"

extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim3;

static ws2812_t led;

void app_main(void) {
    ws2812_install(&led, &hspi2, &htim3, 1);   // 一步完成 init + 注册

    ws2812_color_t green = { .g = 128, .r = 0, .b = 0 };
    ws2812_effect_blink(&led, green, 500);     // 🟢 绿 1Hz 闪烁

    for (;;) {
        // 主循环空，WS2812 由库内置的 weak 回调驱动
    }
}
```

**应用层不需要写 `HAL_TIM_PeriodElapsedCallback` / `HAL_SPI_TxCpltCallback`**——库内部已经覆盖（强定义）。如果工程里也有其他模块需要这两个回调，请把 ws2812 的分发合并到对应模块，或评估是否会冲突。

## 📜 License

MIT — 见 [LICENSE](LICENSE)

Copyright (c) 2026 宁子希 (1589326497@qq.com)