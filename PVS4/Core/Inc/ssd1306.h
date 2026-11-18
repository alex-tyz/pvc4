#ifndef __SSD1306_H__
#define __SSD1306_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SSD1306_ADDR    0x78U
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

typedef enum
{
  SSD1306_BLACK = 0U,
  SSD1306_WHITE = 1U
} SSD1306_Color;

HAL_StatusTypeDef SSD1306_Init(void);
HAL_StatusTypeDef SSD1306_Update(void);
const uint8_t    *SSD1306_GetBuffer(void);

void SSD1306_Fill(SSD1306_Color color);
void SSD1306_DrawPixel(int x, int y, SSD1306_Color color);
void SSD1306_DrawLine(int x0, int y0, int x1, int y1, SSD1306_Color color);
void SSD1306_DrawCircle(int cx, int cy, int r, SSD1306_Color color);
void SSD1306_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, SSD1306_Color color);
void SSD1306_SetCursor(int x, int y);
void SSD1306_WriteChar(char c, SSD1306_Color color);
void SSD1306_WriteString(const char *s, SSD1306_Color color);

#ifdef __cplusplus
}
#endif

#endif
