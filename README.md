# ws2812 — STM32 HAL WS2812 LED 驱动库

> C API。SPI + DMA 硬件生成 WS2812 时序，TIM ISR 翻相，main loop 零负担。

## ✨ 特性

- ⚡ **SPI + DMA**，3-bit 编码把 WS2812 时序转给 SPI 硬件
- 🔁 **TIM ISR 内翻相 + 启动 DMA**，main loop 完全空转
- 🎨 **C API**（`ws2812_t` 结构体 + 函数）
- 🧩 **单实例**，最简结构
- 💾 ~1 KB FLASH，单实例 ~80 B RAM + 编码缓冲

## 🔧 硬件

| 项 | 要求 |
|---|---|
| MCU | STM32G0/F4/H7 等（有 SPI + DMA + 基础 TIM 即可） |
| SPI | master，**Direction = 1LINE**，MOSI 接 DIN，Mode 0，MSB first，2~4 MHz |
| DMA | SPI TX 通道，Byte 宽 |
| TIM（可选） | 1 个基础定时器，用于 blink |

## 🚀 用法

### 📦 CMake

```cmake
add_subdirectory(Lib/ws2812)
target_link_libraries(${CMAKE_PROJECT_NAME} ws2812)
```

或 `FetchContent`：

```cmake
FetchContent_Declare(ws2812
    GIT_REPOSITORY https://github.com/NingZiXi/ws2812.git
    GIT_TAG        v2.0.0)
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
    ws2812_init(&led, &hspi2, 1);
    ws2812_attach_timer(&led, &htim3);

    ws2812_color_t green = { .g = 128, .r = 0, .b = 0 };
    ws2812_effect_blink(&led, green, 500);  // 🟢 绿 1Hz 闪烁

    for (;;) {
        // 主循环空，WS2812 由 TIM3 ISR + DMA 完成 ISR 驱动
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim3) ws2812_on_tim_tick(&led);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi2) ws2812_on_dma_complete(&led);
}
```

## 📜 License

MIT — 见 [LICENSE](LICENSE)

Copyright (c) 2026 宁子希 (1589326497@qq.com)