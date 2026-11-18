/* TIM6 supplies the HAL millisecond clock; SysTick belongs exclusively to FreeRTOS. */
#include "main.h"
static TIM_HandleTypeDef timer;
HAL_StatusTypeDef HAL_InitTick(uint32_t priority) {
  (void)priority;
  __HAL_RCC_TIM6_CLK_ENABLE();
  RCC_ClkInitTypeDef clocks; uint32_t latency;
  HAL_RCC_GetClockConfig(&clocks,&latency);
  uint32_t hz=HAL_RCC_GetPCLK1Freq();
  if (clocks.APB1CLKDivider!=RCC_HCLK_DIV1) hz*=2;
  timer.Instance=TIM6;
  timer.Init.Prescaler=hz/1000000U-1U;
  timer.Init.Period=999;
  timer.Init.CounterMode=TIM_COUNTERMODE_UP;
  timer.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&timer)!=HAL_OK) return HAL_ERROR;
  /* Above BASEPRI=5, so HAL timeout clock runs during short kernel critical sections. */
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn,4,0);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
  return HAL_TIM_Base_Start_IT(&timer);
}
void TIM6_DAC_IRQHandler(void) { HAL_TIM_IRQHandler(&timer); }
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance==TIM6) HAL_IncTick();
}
void HAL_SuspendTick(void) { __HAL_TIM_DISABLE_IT(&timer,TIM_IT_UPDATE); }
void HAL_ResumeTick(void) { __HAL_TIM_ENABLE_IT(&timer,TIM_IT_UPDATE); }
