# ws2812 — STM32 HAL WS2812 LED 驱动库

> C++ 类 API。SPI + DMA 硬件生成 800 kHz WS2812 时序，TIM ISR 内部完成翻相，main loop 零负担。🔥

## ✨ 特性

- ⚡ **SPI + DMA**，5-bit 编码把 WS2812 800 kbps 时序转给 SPI 硬件
- 🔁 **TIM ISR 内翻相**，main loop 完全不用 task()
- 🎨 **C++ 类 API**（`ws2812::WS2812`），Arduino 风格
- 🚫 **无 virtual / STL / exception / RTTI**（`-fno-rtti -fno-exceptions`）
- 🧩 **多实例支持**（多灯带 / 多 SPI 总线，HAL_TIM 区分）
- 💾 ~3 KB FLASH，单实例 ~100 B RAM

## 🔧 硬件

| 项 | 要求 |
|---|---|
| MCU | STM32G0/F4/H7 等（有 SPI + DMA + 基础 TIM 即可） |
| SPI | master 模式，MOSI 接 DIN，**baud ≥ 4 Mbps**（prescaler /16 for 64 MHz APB） |
| DMA | SPI TX 通道，Byte 宽 |
| TIM（可选） | 1 个基础定时器，1 kHz tick（PSC=63999 for 64 MHz），用于 blink |

## 📁 文件

```
ws2812/
├── CMakeLists.txt    ← CMake target
├── ws2812.h          ← 公开 API（namespace ws2812）
├── ws2812.cpp        ← 实现（5-bit 编码 + SPI/DMA + TIM trampoline）
├── LICENSE           ← MIT
└── README.md         ← 你正在看
```

## 🚀 用法

### 📦 CMake

```cmake
add_subdirectory(Lib/ws2812)
target_link_libraries(${CMAKE_PROJECT_NAME} ws2812)
```

或 `FetchContent`（从 GitHub 拉）：

```cmake
FetchContent_Declare(ws2812
    GIT_REPOSITORY https://github.com/NingZiXi/ws2812.git
    GIT_TAG        v1.0.0)
FetchContent_MakeAvailable(ws2812)
```

### 🔧 CubeMX

- **SPI**：master，Data 8，**Baud 4 Mbps**
- **SPI DMA**：TX，Byte，Normal
- **GPIO**：MOSI 设 `SPIx_MOSI` AF
- **TIM（可选）**：Internal Clock，PSC=63999，ARR=499，NVIC enable

> 💡 CubeMX 生成的 `MX_SPIx_Init()` / `MX_TIMy_Init()` 创建 handle，**库不创建硬件**。

### 💻 应用代码

```cpp
#include "ws2812.h"

extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim3;

static ws2812::WS2812 led(&hspi2, 1);    // 1 颗 LED

extern "C" void app_main(void) {
    HAL_TIM_Base_Start_IT(&htim3);        // CubeMX 已 init
    led.attachTimer(&htim3);
    led.effectBlink(ws2812::Color(0, 128, 0), 1000);   // 🟢 绿色 1Hz 闪烁

    for (;;) {
        HAL_Delay(10);   // 啥也不用做 😴
    }
}
```

## 📚 API

`namespace ws2812`

### 🎨 `struct Color`

```cpp
struct Color { uint8_t g, r, b; };      // WS2812 是 GRB 顺序
constexpr Color();                      // 默认黑
constexpr Color(uint8_t r, uint8_t g, uint8_t b);
static constexpr Color Black(), Red(), Green(), Blue();
```

### ⚠️ `enum class Status`

```cpp
enum class Status : uint8_t { OK, ERR_NULL, ERR_BUSY, ERR_COUNT };
```

### 🏗️ `class WS2812`

| 方法 | 说明 |
|---|---|
| `WS2812(SPI_HandleTypeDef*, uint16_t led_count=1)` | 构造 |
| `setPixel(i, c)` / `setPixels(arr, n)` / `clear()` / `show()` | 像素操作 |
| `attachTimer(htim)` / `detachTimer()` | 绑/解 TIM（bind 是 blink 的前提） |
| `effectOff()` / `effectSolid(c)` | 关 / 常亮 |
| `effectBlink(c, period_ms)` | 全 LED 同色闪烁（周期 = period_ms） |
| `count()` / `htim()` | 状态查询 |

> 💡 `effectBlink` 行为：立刻亮，TIM 每 `period_ms/2` 翻相（亮↔灭）。ISR 内部直接发 DMA，**main loop 啥也不用做**。

### 🧩 多实例

```cpp
static ws2812::WS2812 strip_a(&hspi2, 16);   // SPI2 + 16 颗
static ws2812::WS2812 strip_b(&hspi3, 8);    // SPI3 + 8 颗

strip_a.attachTimer(&htim3);                  // 用不同 TIM
strip_b.attachTimer(&htim14);

strip_a.effectBlink(ws2812::Color::Green(), 1000);
strip_b.effectBlink(ws2812::Color::Blue(),  500);
```

最多 4 个并发实例（改 `kMaxInstances` 可调）。

## ⚡ 工作原理

### 5-bit 编码

4 Mbps SPI × 5 bit = 800 kHz WS2812 bit：

| WS2812 bit | SPI bits (MSB first) | T_H / T_L |
|---|---|---|
| `0` | `11000` | 500 / 750 ns |
| `1` | `11100` | 750 / 500 ns |

24 bit GRB → 120 SPI bit → **15 字节**。

> 💡 **关键**：两种 5-bit 模式都以 `000` 结尾 → DMA 完成后 SPI 保持 MOSI 在低电平，距下次 TIM 触发 = `period_ms` 远 > 280 µs → **reset 自动满足**，不需要 HAL_Delay。

### ISR 数据流

```
TIM3 计数到 ARR
  ↓
HAL_TIM_PeriodElapsedCallback
  ↓
hal_tim_period_elapsed(htim)
  ↓
g_instance->onTimTick()
  ├─ _phase ^= 1
  ├─ _buf[*] = 亮 or 黑
  ├─ 编码 24 bit
  └─ HAL_SPI_Transmit_DMA()  ← 返回，DMA 完成 ISR 唤醒 CPU
```

## 📊 资源

| 项 | 1 LED | 16 LED |
|---|---|---|
| 库代码 (.text) | ~3 KB | ~3 KB |
| 堆（运行时） | 18 B | 288 B |
| 1 次全发 | 30 µs | 480 µs |
| Reset | 0 µs（自动） | 0 µs |

## 🎯 工程原则

| 项 | 做法 |
|---|---|
| 🚫 virtual | 不用（零 vtable） |
| 🚫 STL | 不用（仅 `<new>` 取 `nothrow`） |
| 🚫 exception | 关（`-fno-exceptions`） |
| 🚫 RTTI | 关（`-fno-rtti`） |
| 🚫 全局对象 | 不用（trampoline 用静态数组 `s_instances[4]`） |
| ✅ 堆 | 构造时 new，析构时 delete |
| ✅ HAL_Delay | 仅 main loop；ISR 内靠自然 reset |

## 🌐 兼容性

STM32G0/F4/H7/L0/L4 都行（任何有 SPI + DMA + TIM 的家族）。APB 时钟太低时调 SPI baud rate 分频，或改用 10-bit 编码（要重新算 timing）。

## 📜 License

MIT — 见 [LICENSE](LICENSE)

Copyright (c) 2026 宁子希 (1589326497@qq.com)