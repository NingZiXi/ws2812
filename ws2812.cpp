/**
 * @file    ws2812.cpp
 * @author  宁子希 (1589326497@qq.com)
 * @brief   TIM ISR 内翻相 + 启动 DMA；main loop 零 task()
 * @date    2026-07-24
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ws2812.h"
#include <string.h>
#include <new>

namespace ws2812 {

static const uint8_t k_pat[2] = { 0x18, 0x1C };   // 0b11000 / 0b11100, MSB 在 bit 4

static constexpr uint8_t kMaxInstances = 4;
static WS2812 *s_instances[kMaxInstances] = {nullptr};
static uint8_t  s_instance_count = 0;

static void hal_tim_period_elapsed(TIM_HandleTypeDef *htim) {
    for (uint8_t i = 0; i < s_instance_count; i++) {
        WS2812 *self = s_instances[i];
        if (self != nullptr && self->htim() == htim) {
            self->onTimTick();
            return;
        }
    }
}

WS2812::WS2812(SPI_HandleTypeDef *hspi, uint16_t led_count)
    : _hspi(hspi), _htim(nullptr), _count(0),
      _buf(nullptr), _tx_buf(nullptr),
      _mode(OFF), _color(), _phase(0)
{
    if (hspi == nullptr || led_count == 0) return;

    _buf    = new Color[led_count];
    _tx_buf = new uint8_t[15 * led_count];

    if (_buf == nullptr || _tx_buf == nullptr) {
        delete[] _buf;
        delete[] _tx_buf;
        _buf = nullptr;
        _tx_buf = nullptr;
        return;
    }

    _count = led_count;
    clear();
}

WS2812::~WS2812() {
    detachTimer();
    delete[] _buf;
    delete[] _tx_buf;
}

void WS2812::encodePixel(uint16_t i, uint8_t *out15) const {
    Color c = _buf[i];
    uint32_t bits = ((uint32_t)c.g << 16) | ((uint32_t)c.r << 8) | c.b;

    memset(out15, 0, 15);
    for (int b = 0; b < 24; b++) {
        uint8_t p = k_pat[(bits >> (23 - b)) & 1];
        int base = b * 5;
        for (int j = 0; j < 5; j++) {
            if (p & (uint8_t)(1u << (4 - j))) {
                int idx = base + j;
                out15[idx >> 3] |= (uint8_t)(1u << (7 - (idx & 7)));
            }
        }
    }
}

Status WS2812::sendFrameBlocking() {
    if (_hspi == nullptr) return Status::ERR_NULL;
    if (_count == 0)      return Status::ERR_COUNT;

    for (uint16_t i = 0; i < _count; i++) {
        encodePixel(i, _tx_buf + i * 15);
    }

    uint16_t total = _count * 15;
    if (HAL_SPI_Transmit_DMA(_hspi, _tx_buf, total) != HAL_OK) {
        return Status::ERR_BUSY;
    }
    while (_hspi->State != HAL_SPI_STATE_READY) {
        __WFI();
    }
    HAL_Delay(1);
    return Status::OK;
}

Status WS2812::setPixel(uint16_t i, Color c) {
    if (i >= _count) return Status::ERR_COUNT;
    _buf[i] = c;
    return Status::OK;
}

Status WS2812::setPixels(const Color *arr, uint16_t n) {
    if (arr == nullptr) return Status::ERR_NULL;
    if (n > _count) n = _count;
    memcpy(_buf, arr, n * sizeof(Color));
    return Status::OK;
}

Status WS2812::clear() {
    if (_buf == nullptr) return Status::ERR_NULL;
    for (uint16_t i = 0; i < _count; i++) _buf[i] = Color::Black();
    return Status::OK;
}

Status WS2812::show() {
    return sendFrameBlocking();
}

Status WS2812::attachTimer(TIM_HandleTypeDef *htim) {
    _htim = htim;
    if (s_instance_count < kMaxInstances) {
        s_instances[s_instance_count++] = this;
    }
    return Status::OK;
}

Status WS2812::detachTimer() {
    if (_htim != nullptr) {
        __HAL_TIM_DISABLE_IT(_htim, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(_htim);
    }
    _htim = nullptr;
    for (uint8_t i = 0; i < s_instance_count; i++) {
        if (s_instances[i] == this) {
            s_instances[i] = nullptr;
            break;
        }
    }
    return Status::OK;
}

Status WS2812::effectOff() {
    _mode = OFF;
    if (_htim != nullptr) {
        __HAL_TIM_DISABLE_IT(_htim, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(_htim);
    }
    for (uint16_t i = 0; i < _count; i++) _buf[i] = Color::Black();
    return sendFrameBlocking();
}

Status WS2812::effectSolid(Color c) {
    if (_htim != nullptr) {
        __HAL_TIM_DISABLE_IT(_htim, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(_htim);
    }
    _mode = SOLID;
    _color = c;
    for (uint16_t i = 0; i < _count; i++) _buf[i] = c;
    return sendFrameBlocking();
}

Status WS2812::effectBlink(Color c, uint32_t period_ms) {
    if (_htim == nullptr) return Status::ERR_NULL;

    _mode  = BLINK;
    _color = c;
    _phase = 0;

    for (uint16_t k = 0; k < _count; k++) _buf[k] = c;
    Status st = sendFrameBlocking();
    if (st != Status::OK) return st;

    __HAL_TIM_SET_AUTORELOAD(_htim, period_ms - 1);
    __HAL_TIM_SET_COUNTER(_htim, 0);
    __HAL_TIM_ENABLE_IT(_htim, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(_htim);

    if (s_instance_count < kMaxInstances) {
        s_instances[s_instance_count++] = this;
    }
    return Status::OK;
}

void WS2812::onTimTick() {
    if (_mode != Mode::BLINK) return;

    _phase ^= 1;
    Color c = _phase ? Color::Black() : _color;
    for (uint16_t i = 0; i < _count; i++) _buf[i] = c;

    for (uint16_t i = 0; i < _count; i++) {
        encodePixel(i, _tx_buf + i * 15);
    }
    HAL_SPI_Transmit_DMA(_hspi, _tx_buf, _count * 15);
}

} // namespace ws2812

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    ws2812::hal_tim_period_elapsed(htim);
}