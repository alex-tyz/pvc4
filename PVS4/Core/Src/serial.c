#include "serial.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <string.h>
static QueueHandle_t rx;
static TaskHandle_t owner;
static uint8_t rx_byte;
static volatile bool lost;
static volatile uint32_t errors;

void Serial_Init(void) {
  rx=xQueueCreate(512,sizeof(uint8_t));
  configASSERT(rx);
}
void Serial_Start(void) {
  owner=xTaskGetCurrentTaskHandle();
  configASSERT(HAL_UART_Receive_IT(&huart6,&rx_byte,1)==HAL_OK);
}
bool Serial_ReadByte(uint8_t *byte) { return xQueueReceive(rx,byte,0)==pdPASS; }
bool Serial_RecoverOverflow(void) {
  taskENTER_CRITICAL();
  bool result=lost;
  if (result) { xQueueReset(rx); lost=false; }
  taskEXIT_CRITICAL();
  return result;
}
uint32_t Serial_GetRxErrors(void) { return errors; }
void Serial_SendLine(const char *text) {
  /* Single writer: CommsTask. Buffer remains alive until completion or abort. */
  char buffer[160];
  size_t len=strlen(text);
  configASSERT(len+2<sizeof(buffer));
  memcpy(buffer,text,len); buffer[len++]='\r'; buffer[len++]='\n';
  ulTaskNotifyTake(pdTRUE,0);
  if (HAL_UART_Transmit_IT(&huart6,(uint8_t *)buffer,(uint16_t)len)!=HAL_OK) return;
  if (!ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(60))) HAL_UART_AbortTransmit(&huart6);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart) {
  if (uart!=&huart6 || !rx) return;
  BaseType_t wake=pdFALSE;
  if (xQueueSendFromISR(rx,&rx_byte,&wake)!=pdPASS) { lost=true; ++errors; }
  if (HAL_UART_Receive_IT(uart,&rx_byte,1)!=HAL_OK) { lost=true; ++errors; }
  portYIELD_FROM_ISR(wake);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart) {
  if (uart!=&huart6 || !owner) return;
  BaseType_t wake=pdFALSE;
  vTaskNotifyGiveFromISR(owner,&wake);
  portYIELD_FROM_ISR(wake);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
  if (uart!=&huart6) return;
  lost=true; ++errors;
  if (uart->RxState==HAL_UART_STATE_READY) (void)HAL_UART_Receive_IT(uart,&rx_byte,1);
}
