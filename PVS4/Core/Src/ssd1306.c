#include "ssd1306.h"
#include "font5x7.h"
#include "i2c.h"
#include "pca9538.h"
#include <stdlib.h>

#define SSD1306_RESET_PIN    0U
#define SSD1306_TIMEOUT_MS   30U
#define SSD1306_BUFFER_SIZE  (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

static uint8_t frame[1U + SSD1306_BUFFER_SIZE] = { 0x40U };
static int     cursor_x;
static int     cursor_y;

static const uint8_t init_commands[] = {
  0x00U,
  0xAEU,
  0x20U, 0x00U,
  0xC8U,
  0x40U,
  0x81U, 0xFFU,
  0xA1U,
  0xA6U,
  0xA8U, 0x3FU,
  0xA4U,
  0xD3U, 0x00U,
  0xD5U, 0xF0U,
  0xD9U, 0x22U,
  0xDAU, 0x12U,
  0xDBU, 0x20U,
  0x8DU, 0x14U,
  0xAFU
};

static const uint8_t window_commands[] = {
  0x00U,
  0x21U, 0x00U, 0x7FU,
  0x22U, 0x00U, 0x07U
};

static HAL_StatusTypeDef Send(const uint8_t *data, uint16_t len)
{
  return HAL_I2C_Master_Transmit(&hi2c1, SSD1306_ADDR, (uint8_t *)data, len, SSD1306_TIMEOUT_MS);
}

static HAL_StatusTypeDef HardwareReset(void)
{
  HAL_StatusTypeDef status = PCA9538_SetPin(PCA9538_ADDR_DEVICES, SSD1306_RESET_PIN, 1U);
  if (status == HAL_OK)
  {
    status = PCA9538_MakeOutput(PCA9538_ADDR_DEVICES, SSD1306_RESET_PIN);
  }
  if (status == HAL_OK)
  {
    status = PCA9538_SetPin(PCA9538_ADDR_DEVICES, SSD1306_RESET_PIN, 0U);
  }
  HAL_Delay(1U);
  if (status == HAL_OK)
  {
    status = PCA9538_SetPin(PCA9538_ADDR_DEVICES, SSD1306_RESET_PIN, 1U);
  }
  HAL_Delay(1U);
  return status;
}

HAL_StatusTypeDef SSD1306_Init(void)
{
  HAL_StatusTypeDef status = HardwareReset();
  if (status != HAL_OK)
  {
    return status;
  }
  status = Send(init_commands, sizeof(init_commands));
  if (status != HAL_OK)
  {
    return status;
  }
  SSD1306_Fill(SSD1306_BLACK);
  return SSD1306_Update();
}

HAL_StatusTypeDef SSD1306_Update(void)
{
  HAL_StatusTypeDef status = Send(window_commands, sizeof(window_commands));
  if (status != HAL_OK)
  {
    return status;
  }
  frame[0] = 0x40U;
  return Send(frame, sizeof(frame));
}

const uint8_t *SSD1306_GetBuffer(void)
{
  return &frame[1];
}

void SSD1306_Fill(SSD1306_Color color)
{
  uint8_t value = (color == SSD1306_WHITE) ? 0xFFU : 0x00U;
  for (int i = 1; i <= SSD1306_BUFFER_SIZE; i++)
  {
    frame[i] = value;
  }
}

void SSD1306_DrawPixel(int x, int y, SSD1306_Color color)
{
  uint8_t *cell;
  if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
  {
    return;
  }
  cell = &frame[1 + x + (y / 8) * SSD1306_WIDTH];
  if (color == SSD1306_WHITE)
  {
    *cell |= (uint8_t)(1U << (y % 8));
  }
  else
  {
    *cell &= (uint8_t)~(1U << (y % 8));
  }
}

void SSD1306_DrawLine(int x0, int y0, int x1, int y1, SSD1306_Color color)
{
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;

  for (;;)
  {
    SSD1306_DrawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }
    int e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void SSD1306_DrawCircle(int cx, int cy, int r, SSD1306_Color color)
{
  int x = r;
  int y = 0;
  int err = 1 - r;

  while (x >= y)
  {
    SSD1306_DrawPixel(cx + x, cy + y, color);
    SSD1306_DrawPixel(cx + y, cy + x, color);
    SSD1306_DrawPixel(cx - y, cy + x, color);
    SSD1306_DrawPixel(cx - x, cy + y, color);
    SSD1306_DrawPixel(cx - x, cy - y, color);
    SSD1306_DrawPixel(cx - y, cy - x, color);
    SSD1306_DrawPixel(cx + y, cy - x, color);
    SSD1306_DrawPixel(cx + x, cy - y, color);
    y++;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

static int Edge(int ax, int ay, int bx, int by, int px, int py)
{
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void SSD1306_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, SSD1306_Color color)
{
  int min_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
  int max_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

  for (int y = min_y; y <= max_y; y++)
  {
    for (int x = min_x; x <= max_x; x++)
    {
      int w0 = Edge(x1, y1, x2, y2, x, y);
      int w1 = Edge(x2, y2, x0, y0, x, y);
      int w2 = Edge(x0, y0, x1, y1, x, y);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
      {
        SSD1306_DrawPixel(x, y, color);
      }
    }
  }
}

void SSD1306_SetCursor(int x, int y)
{
  cursor_x = x;
  cursor_y = y;
}

void SSD1306_WriteChar(char c, SSD1306_Color color)
{
  const uint8_t *glyph;
  SSD1306_Color back = (color == SSD1306_WHITE) ? SSD1306_BLACK : SSD1306_WHITE;

  if ((uint8_t)c < FONT5X7_FIRST || (uint8_t)c > FONT5X7_LAST)
  {
    c = '?';
  }
  glyph = Font5x7[(uint8_t)c - FONT5X7_FIRST];
  for (int col = 0; col <= (int)FONT5X7_WIDTH; col++)
  {
    uint8_t bits = (col < (int)FONT5X7_WIDTH) ? glyph[col] : 0U;
    for (int row = 0; row < 8; row++)
    {
      SSD1306_DrawPixel(cursor_x + col, cursor_y + row, (bits & (1U << row)) ? color : back);
    }
  }
  cursor_x += (int)FONT5X7_WIDTH + 1;
}

void SSD1306_WriteString(const char *s, SSD1306_Color color)
{
  while (*s != '\0')
  {
    SSD1306_WriteChar(*s++, color);
  }
}
