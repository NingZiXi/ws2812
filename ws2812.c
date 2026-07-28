/**
 * @file    ws2812.c
 * @brief   WS2812 驱动实现。
 */
#include "ws2812.h"
#include <stdlib.h>
#include <string.h>

#define WS2812_CODE_0    (0b100u)   // 0 码
#define WS2812_CODE_1    (0b110u)   // 1 码
#define WS2812_LED_BYTES  24        // 1 颗 LED 24 字节
#define WS2812_RESET_BYTES 50       // reset 缓冲:50 字节低电平 ≈ 100μs

#define WS2812_MAX_INSTANCES 4

typedef struct {
    ws2812_t *self;
    SPI_HandleTypeDef *hspi;
    TIM_HandleTypeDef *htim;
} ws2812_slot_t;

static ws2812_slot_t s_slots[WS2812_MAX_INSTANCES];
static uint8_t s_slot_count;

// 把 GRB 颜色展开为 24 字节 3-bit 编码
static void encode_led(uint8_t *out, ws2812_color_t c)
{
    uint32_t color = ((uint32_t)c.g << 16) | ((uint32_t)c.r << 8) | c.b;
    for (int i = 0; i < 24; i++) {
        int bit = (color >> (23 - i)) & 1;
        out[i] = (uint8_t)(bit ? WS2812_CODE_1 : WS2812_CODE_0);
    }
}

// 把 self 颜色编码后通过 DMA 发出
static ws2812_status_t send_color(ws2812_t *self, ws2812_color_t c)
{
    for (uint16_t i = 0; i < self->count; i++) {
        encode_led(self->tx_buf + i * WS2812_LED_BYTES, c);
    }
    uint16_t total = (uint16_t)(self->count * WS2812_LED_BYTES + WS2812_RESET_BYTES);
    self->dma_busy = 1;
    if (HAL_SPI_Transmit_DMA(self->hspi, self->tx_buf, total) != HAL_OK) {
        self->dma_busy = 0;
        return WS2812_ERR_BUSY;
    }
    return WS2812_OK;
}

// 安装到分发表
static ws2812_status_t register_slot(ws2812_t *self)
{
    for (uint8_t i = 0; i < s_slot_count; i++) {
        if (s_slots[i].self == self) return WS2812_OK;
    }
    if (s_slot_count >= WS2812_MAX_INSTANCES) return WS2812_ERR_NULL;
    s_slots[s_slot_count].self = self;
    s_slots[s_slot_count].hspi = self->hspi;
    s_slots[s_slot_count].htim = self->htim;
    s_slot_count++;
    return WS2812_OK;
}

// 从分发表反注册
static void unregister_slot(ws2812_t *self)
{
    for (uint8_t i = 0; i < s_slot_count; i++) {
        if (s_slots[i].self == self) {
            s_slots[i].self = NULL;
            s_slots[i].hspi = NULL;
            s_slots[i].htim = NULL;
            return;
        }
    }
}

// 在分发表里找匹配 hspi/htim 的实例
static ws2812_t *find_by_hspi(SPI_HandleTypeDef *hspi)
{
    for (uint8_t i = 0; i < s_slot_count; i++) {
        if (s_slots[i].self != NULL && s_slots[i].hspi == hspi) return s_slots[i].self;
    }
    return NULL;
}

static ws2812_t *find_by_htim(TIM_HandleTypeDef *htim)
{
    for (uint8_t i = 0; i < s_slot_count; i++) {
        if (s_slots[i].self != NULL && s_slots[i].htim == htim) return s_slots[i].self;
    }
    return NULL;
}

// 初始化句柄、分配缓冲、注册到分发表
ws2812_status_t ws2812_install(ws2812_t *self, SPI_HandleTypeDef *hspi, TIM_HandleTypeDef *htim, uint16_t count)
{
    if (self == NULL || hspi == NULL || htim == NULL || count == 0) return WS2812_ERR_NULL;

    self->hspi = hspi;
    self->htim = htim;
    self->count = count;
    self->phase = 0;
    self->dma_busy = 0;
    self->installed = 1;

    uint16_t len = (uint16_t)(count * WS2812_LED_BYTES + WS2812_RESET_BYTES);
    self->tx_buf = (uint8_t *)malloc(len);
    if (self->tx_buf == NULL) return WS2812_ERR_NULL;
    memset(self->tx_buf, 0, len);

    return register_slot(self);
}

// 停 TIM、释放缓冲、反注册
void ws2812_uninstall(ws2812_t *self)
{
    if (self == NULL) return;
    if (self->htim != NULL) {
        __HAL_TIM_DISABLE_IT(self->htim, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(self->htim);
    }
    if (self->tx_buf != NULL) {
        free(self->tx_buf);
        self->tx_buf = NULL;
    }
    self->installed = 0;
    unregister_slot(self);
}

// 停止翻相并发送一次亮色
ws2812_status_t ws2812_effect_solid(ws2812_t *self, ws2812_color_t c)
{
    if (self == NULL || self->tx_buf == NULL) return WS2812_ERR_NULL;
    if (self->htim != NULL) {
        __HAL_TIM_DISABLE_IT(self->htim, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(self->htim);
    }

    self->cur_color = c;
    self->phase = 0;
    return send_color(self, c);
}

// 启动闪烁:立即发送亮色帧,然后 TIM 接管翻相
ws2812_status_t ws2812_effect_blink(ws2812_t *self, ws2812_color_t c, uint32_t period_ms)
{
    if (self == NULL || self->htim == NULL || self->tx_buf == NULL) return WS2812_ERR_NULL;

    self->cur_color = c;
    self->phase = 0;

    ws2812_status_t st = send_color(self, c);
    if (st != WS2812_OK) return st;

    __HAL_TIM_SET_AUTORELOAD(self->htim, period_ms - 1);
    __HAL_TIM_SET_COUNTER(self->htim, 0);
    __HAL_TIM_ENABLE_IT(self->htim, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(self->htim);
    return WS2812_OK;
}

// 库内 weak 回调:覆盖 HAL 的默认空实现,app 不需要再写
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    ws2812_t *self = find_by_htim(htim);
    if (self == NULL) return;
    if (self->dma_busy) return;  // 上一帧还没发完

    self->phase ^= 1;
    ws2812_color_t c = self->phase ? (ws2812_color_t){0, 0, 0} : self->cur_color;
    send_color(self, c);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    ws2812_t *self = find_by_hspi(hspi);
    if (self != NULL) self->dma_busy = 0;
}