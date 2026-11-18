#include "compass_app.h"
#include "lsm9ds_mag.h"
#include "serial.h"
#include "ssd1306.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN   48U
#define ARROW_CX       96
#define ARROW_CY       32
#define ARROW_RADIUS   30
#define ARROW_TIP      26
#define ARROW_TAIL     12
#define ARROW_HEAD     9
#define ARROW_WING     5
#define MIN_HORIZONTAL 1.0f
#define RAD_TO_DEG     57.29578f

typedef struct
{
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t direction;
  bool    valid;
} Reading;

static int32_t  scales[3] = { COMPASS_SCALE_DEFAULT, COMPASS_SCALE_DEFAULT, COMPASS_SCALE_DEFAULT };
static Reading  reading;
static bool     sensor_ok;
static bool     display_ok;
static uint32_t last_read_ms;
static uint32_t last_report_ms;
static uint32_t last_retry_ms;
static char     line[LINE_MAX_LEN + 1U];
static uint8_t  line_len;
static bool     line_overflow;
static float    arrow_right;
static float    arrow_up;

static void SendLine(const char *text)
{
  Serial_Print(text);
  Serial_Print("\r\n");
}

static void ComputeDirection(void)
{
  float mx = (float)reading.x;
  float my = (float)reading.y;
  float right = (float)COMPASS_SIGN_X * (COMPASS_SWAP_XY ? my : mx);
  float up = (float)COMPASS_SIGN_Y * (COMPASS_SWAP_XY ? mx : my);
  float len = sqrtf(right * right + up * up);

  if (len < MIN_HORIZONTAL)
  {
    arrow_right = 0.0f;
    arrow_up = 0.0f;
    reading.direction = -1;
    return;
  }
  arrow_right = right / len;
  arrow_up = up / len;
  reading.direction = (int32_t)lroundf(atan2f(right, up) * RAD_TO_DEG);
  if (reading.direction < 0)
  {
    reading.direction += 360;
  }
  if (reading.direction >= 360)
  {
    reading.direction -= 360;
  }
}

static void ReadSensor(void)
{
  LSM9DS_MagData raw;

  if (LSM9DS_Mag_Read(&raw) != HAL_OK)
  {
    sensor_ok = false;
    reading.valid = false;
    return;
  }
  reading.x = raw.x * scales[0] / 100;
  reading.y = raw.y * scales[1] / 100;
  reading.z = raw.z * scales[2] / 100;
  reading.valid = true;
  ComputeDirection();
}

static void DrawArrow(void)
{
  int tip_x = ARROW_CX + (int)lroundf(arrow_right * ARROW_TIP);
  int tip_y = ARROW_CY - (int)lroundf(arrow_up * ARROW_TIP);
  int tail_x = ARROW_CX - (int)lroundf(arrow_right * ARROW_TAIL);
  int tail_y = ARROW_CY + (int)lroundf(arrow_up * ARROW_TAIL);
  float base_x = (float)ARROW_CX + arrow_right * (ARROW_TIP - ARROW_HEAD);
  float base_y = (float)ARROW_CY - arrow_up * (ARROW_TIP - ARROW_HEAD);
  float px = arrow_up * ARROW_WING;
  float py = arrow_right * ARROW_WING;

  SSD1306_DrawCircle(ARROW_CX, ARROW_CY, ARROW_RADIUS, SSD1306_WHITE);
  if (!reading.valid || reading.direction < 0)
  {
    SSD1306_DrawCircle(ARROW_CX, ARROW_CY, 2, SSD1306_WHITE);
    return;
  }
  SSD1306_DrawLine(tail_x, tail_y, tip_x, tip_y, SSD1306_WHITE);
  SSD1306_FillTriangle(tip_x, tip_y,
                       (int)lroundf(base_x + px), (int)lroundf(base_y + py),
                       (int)lroundf(base_x - px), (int)lroundf(base_y - py),
                       SSD1306_WHITE);
  SSD1306_DrawCircle(ARROW_CX, ARROW_CY, 1, SSD1306_WHITE);
}

static void PrintRow(int row, const char *text)
{
  SSD1306_SetCursor(0, row * 8);
  SSD1306_WriteString(text, SSD1306_WHITE);
}

static void Render(void)
{
  char text[24];

  SSD1306_Fill(SSD1306_BLACK);
  PrintRow(0, sensor_ok ? LSM9DS_Mag_GetName() : "COMPASS");
  if (sensor_ok && reading.valid)
  {
    snprintf(text, sizeof(text), "X:%5ldmG", (long)reading.x);
    PrintRow(1, text);
    snprintf(text, sizeof(text), "Y:%5ldmG", (long)reading.y);
    PrintRow(2, text);
    snprintf(text, sizeof(text), "Z:%5ldmG", (long)reading.z);
    PrintRow(3, text);
    if (reading.direction >= 0)
    {
      snprintf(text, sizeof(text), "Dir:%3ld", (long)reading.direction);
    }
    else
    {
      snprintf(text, sizeof(text), "Dir: ---");
    }
    PrintRow(4, text);
  }
  else
  {
    PrintRow(2, "SENSOR");
    PrintRow(3, "ERROR");
  }
  snprintf(text, sizeof(text), "Kx:%3ld%%", (long)scales[0]);
  PrintRow(5, text);
  snprintf(text, sizeof(text), "Ky:%3ld%%", (long)scales[1]);
  PrintRow(6, text);
  snprintf(text, sizeof(text), "Kz:%3ld%%", (long)scales[2]);
  PrintRow(7, text);
  DrawArrow();
  display_ok = (SSD1306_Update() == HAL_OK);
}

static void Report(void)
{
  char text[64];

  if (!sensor_ok || !reading.valid)
  {
    SendLine("ERR SENSOR");
    return;
  }
  snprintf(text, sizeof(text), "DATA %ld %ld %ld %ld %ld %ld %ld",
           (long)reading.x, (long)reading.y, (long)reading.z, (long)reading.direction,
           (long)scales[0], (long)scales[1], (long)scales[2]);
  SendLine(text);
}

static bool ParseInt(const char **cursor, int32_t *value)
{
  char *end;
  long parsed;

  while (**cursor == ' ')
  {
    (*cursor)++;
  }
  if (**cursor == '\0')
  {
    return false;
  }
  parsed = strtol(*cursor, &end, 10);
  if (end == *cursor || (*end != ' ' && *end != '\0'))
  {
    return false;
  }
  *value = (int32_t)parsed;
  *cursor = end;
  return true;
}

static void CommandScale(const char *args)
{
  int32_t values[3];
  char text[40];

  for (int i = 0; i < 3; i++)
  {
    if (!ParseInt(&args, &values[i]))
    {
      SendLine("ERR SYNTAX");
      return;
    }
  }
  while (*args == ' ')
  {
    args++;
  }
  if (*args != '\0')
  {
    SendLine("ERR SYNTAX");
    return;
  }
  for (int i = 0; i < 3; i++)
  {
    if (values[i] < COMPASS_SCALE_MIN || values[i] > COMPASS_SCALE_MAX)
    {
      SendLine("ERR RANGE");
      return;
    }
  }
  for (int i = 0; i < 3; i++)
  {
    scales[i] = values[i];
  }
  snprintf(text, sizeof(text), "OK SCALE %ld %ld %ld", (long)scales[0], (long)scales[1], (long)scales[2]);
  SendLine(text);
}

static void HandleLine(void)
{
  char text[40];

  line[line_len] = '\0';
  for (uint8_t i = 0U; i < line_len; i++)
  {
    if (line[i] >= 'a' && line[i] <= 'z')
    {
      line[i] = (char)(line[i] - 'a' + 'A');
    }
  }
  if (line_overflow)
  {
    SendLine("ERR SYNTAX");
  }
  else if (line_len == 0U)
  {
    return;
  }
  else if (strcmp(line, "PING") == 0)
  {
    snprintf(text, sizeof(text), "PONG COMPASS %s", LSM9DS_Mag_GetName());
    SendLine(text);
  }
  else if (strncmp(line, "SCALE ", 6U) == 0)
  {
    CommandScale(line + 6);
  }
  else if (strcmp(line, "GET") == 0)
  {
    Report();
  }
  else
  {
    SendLine("ERR UNKNOWN");
  }
}

static void ProcessInput(void)
{
  uint8_t c;

  while (Serial_ReadByte(&c))
  {
    if (c == '\r' || c == '\n')
    {
      HandleLine();
      line_len = 0U;
      line_overflow = false;
    }
    else if (line_len < LINE_MAX_LEN)
    {
      line[line_len++] = (char)c;
    }
    else
    {
      line_overflow = true;
    }
  }
}

void CompassApp_Init(void)
{
  char text[40];

  Serial_Init();
  display_ok = (SSD1306_Init() == HAL_OK);
  sensor_ok = LSM9DS_Mag_Init();
  if (sensor_ok)
  {
    ReadSensor();
  }
  if (display_ok)
  {
    Render();
  }
  snprintf(text, sizeof(text), "HELLO COMPASS %s", LSM9DS_Mag_GetName());
  SendLine(text);
  last_read_ms = HAL_GetTick();
  last_report_ms = last_read_ms;
  last_retry_ms = last_read_ms;
}

void CompassApp_Process(void)
{
  uint32_t now = HAL_GetTick();

  ProcessInput();

  if ((!sensor_ok || !display_ok) && (uint32_t)(now - last_retry_ms) >= COMPASS_RETRY_PERIOD_MS)
  {
    last_retry_ms = now;
    if (!sensor_ok)
    {
      sensor_ok = LSM9DS_Mag_Init();
    }
    if (!display_ok)
    {
      display_ok = (SSD1306_Init() == HAL_OK);
    }
  }
  if (sensor_ok && (uint32_t)(now - last_read_ms) >= COMPASS_READ_PERIOD_MS)
  {
    last_read_ms = now;
    ReadSensor();
  }
  if ((uint32_t)(now - last_report_ms) >= COMPASS_REPORT_PERIOD_MS)
  {
    last_report_ms = now;
    if (display_ok)
    {
      Render();
    }
    Report();
  }
}
