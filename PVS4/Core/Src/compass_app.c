#include "compass_app.h"
#include "compass_logic.h"
#include "lsm9ds_mag.h"
#include "serial.h"
#include "ssd1306.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ARROW_CX 96
#define ARROW_CY 30
#define ARROW_RADIUS 28
#define ARROW_TIP 24
#define ARROW_TAIL 12
#define ARROW_HEAD 9
#define ARROW_WING 5
static SemaphoreHandle_t i2c_mutex;
static QueueHandle_t config_queue, sensor_queue, displayed_queue;
/* Rendering state belongs exclusively to DisplayTask. */
static CompassReading reading;
static int32_t scales[3]={100,100,100};
static bool sensor_ok, display_ok;
static char frame_name[8]="NONE";
static float arrow_right, arrow_up;
static unsigned frame_counter;
typedef struct { uint32_t frames, max_ms, late, errors, max_gap_ms; } DisplayStats;
static DisplayStats stats;

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
  PrintRow(0, sensor_ok ? frame_name : "COMPASS");
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
  snprintf(text, sizeof(text), "%02u", frame_counter);
  SSD1306_SetCursor(114, 56);
  SSD1306_WriteString(text, SSD1306_WHITE);
  display_ok = (SSD1306_Update() == HAL_OK);
}

static void SensorTask(void *argument)
{
  (void)argument;
  bool ok=false;
  uint32_t retry=HAL_GetTick()-COMPASS_RETRY_PERIOD_MS;
  TickType_t wake=xTaskGetTickCount();
  for (;;) {
    CompassReading sample={0};
    sample.direction=-1;
    xQueuePeek(config_queue,&sample.scales,0);
    if (xSemaphoreTake(i2c_mutex,pdMS_TO_TICKS(32))==pdTRUE) {
      if (!ok && (uint32_t)(HAL_GetTick()-retry)>=COMPASS_RETRY_PERIOD_MS) {
        retry=HAL_GetTick();
        ok=LSM9DS_Mag_Init();
      }
      LSM9DS_MagData raw;
      if (ok) {
        ok=LSM9DS_Mag_Read(&raw)==HAL_OK;
        sample.valid=ok;
        if (ok) {
          const int32_t values[]={raw.x,raw.y,raw.z};
          Compass_Compute(&sample,values,sample.scales);
        }
      }
      snprintf(sample.name,sizeof(sample.name),"%s",LSM9DS_Mag_GetName());
      xSemaphoreGive(i2c_mutex);
    }
    sample.sampled_ms=HAL_GetTick();
    xQueueOverwrite(sensor_queue,&sample);
    if (xTaskDelayUntil(&wake,pdMS_TO_TICKS(COMPASS_READ_PERIOD_MS))==pdFALSE)
      wake=xTaskGetTickCount();
  }
}

static void DisplayTask(void *argument)
{
  (void)argument;
  /* Initial hardware reset/blank frame is outside the periodic rendering budget. */
  xSemaphoreTake(i2c_mutex,portMAX_DELAY);
  display_ok=SSD1306_Init()==HAL_OK;
  xSemaphoreGive(i2c_mutex);
  TickType_t wake=xTaskGetTickCount();
  uint32_t retry=HAL_GetTick(), last_frame=0;
  for (;;) {
    uint32_t start=HAL_GetTick();
    xQueuePeek(sensor_queue,&reading,0);
    if ((uint32_t)(start-reading.sampled_ms)>200U) reading.valid=false;
    sensor_ok=reading.valid;
    memcpy(scales,reading.scales.k,sizeof(scales));
    memcpy(frame_name,reading.name,sizeof(frame_name));
    arrow_right=reading.right; arrow_up=reading.up;
    if (xSemaphoreTake(i2c_mutex,pdMS_TO_TICKS(5))==pdTRUE) {
      if (!display_ok && (uint32_t)(start-retry)>=COMPASS_RETRY_PERIOD_MS) {
        retry=start;
        display_ok=SSD1306_Init()==HAL_OK;
      }
      if (display_ok) {
        Render();
        if (display_ok) {
          uint32_t completed=HAL_GetTick();
          uint32_t gap=completed-last_frame;
          if (stats.frames && gap>stats.max_gap_ms) stats.max_gap_ms=gap;
          last_frame=completed;
          ++stats.frames;
          frame_counter=(frame_counter+1U)%100U;
        } else ++stats.errors;
      }
      xSemaphoreGive(i2c_mutex);
    } else ++stats.errors;
    /* One immutable snapshot for DATA, including the exact scales of this sample. */
    xQueueOverwrite(displayed_queue,&reading);
    uint32_t elapsed=HAL_GetTick()-start;
    if (elapsed>stats.max_ms) stats.max_ms=elapsed;
    if (xTaskDelayUntil(&wake,pdMS_TO_TICKS(COMPASS_DISPLAY_PERIOD_MS))==pdFALSE) {
      ++stats.late;
      wake=xTaskGetTickCount();
    }
  }
}

static void Report(void)
{
  CompassReading sample={0};
  char text[96];
  xQueuePeek(displayed_queue,&sample,0);
  Compass_Format(&sample,text,sizeof(text));
  Serial_SendLine(text);
}

static void CommsTask(void *argument)
{
  (void)argument;
  CompassLine line={0};
  CompassScales next={{100,100,100}};
  Serial_Start();
  Serial_SendLine("HELLO COMPASS FREERTOS");
  uint32_t last=HAL_GetTick();
  for (;;) {
    uint8_t byte;
    /* Bound the work per activation even under continuous UART traffic. */
    for (unsigned i=0; i<64; ++i) {
      if (Serial_RecoverOverflow()) { line.used=0; line.discard=true; }
      if (!Serial_ReadByte(&byte)) break;
      CompassCommand command;
      if (!Compass_Feed(&line,byte,&command,&next)) continue;
      char text[160];
      switch (command) {
        case CMD_PING: {
          CompassReading sample={0};
          xQueuePeek(sensor_queue,&sample,0);
          snprintf(text,sizeof(text),"PONG COMPASS %s",sample.name[0]?sample.name:"NONE");
          break;
        }
        case CMD_GET: Report(); continue;
        case CMD_SCALE:
          xQueueOverwrite(config_queue,&next);
          snprintf(text,sizeof(text),"OK SCALE %ld %ld %ld",(long)next.k[0],(long)next.k[1],(long)next.k[2]);
          break;
        case CMD_STATS: {
          /* DisplayTask has higher priority; copy all counters atomically. */
          taskENTER_CRITICAL();
          DisplayStats snapshot=stats;
          taskEXIT_CRITICAL();
          snprintf(text,sizeof(text),"STATS %lu %lu %lu %lu %lu %lu %lu",(unsigned long)HAL_GetTick(),
            (unsigned long)snapshot.frames,(unsigned long)snapshot.max_ms,(unsigned long)snapshot.late,
            (unsigned long)snapshot.errors,(unsigned long)Serial_GetRxErrors(),(unsigned long)snapshot.max_gap_ms);
          break;
        }
        case CMD_RANGE: strcpy(text,"ERR RANGE"); break;
        case CMD_SYNTAX: strcpy(text,"ERR SYNTAX"); break;
        default: strcpy(text,"ERR UNKNOWN"); break;
      }
      Serial_SendLine(text);
    }
    uint32_t now=HAL_GetTick();
    if ((uint32_t)(now-last)>=COMPASS_REPORT_PERIOD_MS) { last=now; Report(); }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void CompassApp_Init(void)
{
  i2c_mutex=xSemaphoreCreateMutex();
  config_queue=xQueueCreate(1,sizeof(CompassScales));
  sensor_queue=xQueueCreate(1,sizeof(CompassReading));
  displayed_queue=xQueueCreate(1,sizeof(CompassReading));
  configASSERT(i2c_mutex && config_queue && sensor_queue && displayed_queue);
  CompassScales defaults={{100,100,100}};
  xQueueOverwrite(config_queue,&defaults);
  CompassReading initial={.direction=-1,.scales={{100,100,100}},.name="NONE"};
  xQueueOverwrite(sensor_queue,&initial);
  xQueueOverwrite(displayed_queue,&initial);
  Serial_Init();
  configASSERT(xTaskCreate(DisplayTask,"Display",768,NULL,3,NULL)==pdPASS);
  configASSERT(xTaskCreate(SensorTask,"Sensor",512,NULL,2,NULL)==pdPASS);
  configASSERT(xTaskCreate(CommsTask,"Comms",768,NULL,1,NULL)==pdPASS);
}
void vApplicationMallocFailedHook(void) { Error_Handler(); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
  (void)task; (void)name; Error_Handler();
}
