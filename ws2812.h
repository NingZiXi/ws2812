/**
 * @file    ws2812.h
 * @brief   WS2812 LED 驱动头文件,定义 C API。
 */
#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>
#include "stm32g0xx_hal.h"

typedef struct {
    uint8_t g;       // 绿
    uint8_t r;       // 红
    uint8_t b;       // 蓝
} ws2812_color_t;

typedef enum {
    WS2812_OK       = 0,
    WS2812_ERR_NULL = 1,
    WS2812_ERR_BUSY = 2,
} ws2812_status_t;

typedef struct {
    SPI_HandleTypeDef *hspi;   // SPI 句柄,需 1LINE 方向、Mode 0、MSB 先发、≤6.4 MHz
    TIM_HandleTypeDef *htim;   // 翻相定时器
    uint16_t count;            // LED 数量
    uint8_t phase;             // 当前相位:0=亮,1=灭
    ws2812_color_t cur_color;  // 当前颜色
    uint8_t *tx_buf;           // 编码后缓冲
    volatile uint8_t dma_busy; // 1=正在 DMA 发送
    uint8_t installed;         // 1=已通过 ws2812_install 注册
} ws2812_t;

/**
 * @brief 初始化、分配缓冲、接管 TIM/SPI 中断回调
 *
 * 库内部覆盖 HAL_TIM_PeriodElapsedCallback 与 HAL_SPI_TxCpltCallback,
 * app 不需要再写 weak 回调。
 *
 * @param  self   句柄
 * @param  hspi   SPI 句柄
 * @param  htim   TIM 句柄
 * @param  count  LED 数量
 * @return WS2812_OK / WS2812_ERR_NULL
 */
ws2812_status_t ws2812_install(ws2812_t *self, SPI_HandleTypeDef *hspi, TIM_HandleTypeDef *htim, uint16_t count);

/**
 * @brief 反注册、释放缓冲、停定时器
 *
 * @param  self  句柄
 */
void ws2812_uninstall(ws2812_t *self);

/**
 * @brief 发送一次亮色并停止翻相(常亮)
 *
 * @param  self  句柄
 * @param  c     颜色
 * @return WS2812_OK / WS2812_ERR_BUSY / WS2812_ERR_NULL
 */
ws2812_status_t ws2812_effect_solid(ws2812_t *self, ws2812_color_t c);

/**
 * @brief 启动闪烁:立即发送一次亮色,之后 TIM 每 period_ms 翻相
 *
 * @param  self       句柄
 * @param  c          闪烁颜色
 * @param  period_ms  半周期(亮 / 灭 各持续这么久)
 * @return WS2812_OK / WS2812_ERR_BUSY / WS2812_ERR_NULL
 */
ws2812_status_t ws2812_effect_blink(ws2812_t *self, ws2812_color_t c, uint32_t period_ms);

#endif