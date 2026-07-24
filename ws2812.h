/**
 * @file    ws2812.h
 * @author  宁子希 (1589326497@qq.com)
 * @brief   WS2812 LED 驱动，C++ 类 API。SPI+DMA 时序，TIM ISR 翻相。
 * @date    2026-07-24
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <stdint.h>
#include "stm32g0xx_hal.h"

namespace ws2812 {

enum class Status : uint8_t {
    OK      = 0,
    ERR_NULL,
    ERR_BUSY,
    ERR_COUNT,
};

struct Color {
    uint8_t g, r, b;

    constexpr Color() : g(0), r(0), b(0) {}
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_) : g(g_), r(r_), b(b_) {}

    static constexpr Color Black() { return Color(0, 0, 0); }
    static constexpr Color Red()   { return Color(255, 0, 0); }
    static constexpr Color Green() { return Color(0, 255, 0); }
    static constexpr Color Blue()  { return Color(0, 0, 255); }
};

class WS2812 {
public:
    WS2812(SPI_HandleTypeDef *hspi, uint16_t led_count = 1);
    ~WS2812();

    WS2812(const WS2812&) = delete;
    WS2812& operator=(const WS2812&) = delete;

    Status setPixel(uint16_t i, Color c);
    Status setPixels(const Color *arr, uint16_t n);
    Status clear();
    Status show();

    Status attachTimer(TIM_HandleTypeDef *htim);
    Status detachTimer();
    Status effectOff();
    Status effectSolid(Color c);
    Status effectBlink(Color c, uint32_t period_ms);

    uint16_t count()          const { return _count; }
    TIM_HandleTypeDef *htim() const { return _htim; }

    void onTimTick();

private:
    SPI_HandleTypeDef *_hspi;
    TIM_HandleTypeDef *_htim;
    uint16_t _count;
    Color  *_buf;
    uint8_t *_tx_buf;

    enum Mode : uint8_t { OFF = 0, SOLID, BLINK };
    Mode _mode;
    Color _color;
    uint8_t _phase;

    void encodePixel(uint16_t i, uint8_t *out15) const;
    Status sendFrameBlocking();
};

} // namespace ws2812