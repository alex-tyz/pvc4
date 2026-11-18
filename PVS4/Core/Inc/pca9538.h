#ifndef __PCA9538_H__
#define __PCA9538_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define PCA9538_ADDR_DEVICES   0xE0U
#define PCA9538_ADDR_KEYBOARD  0xE2U

#define PCA9538_REG_INPUT      0x00U
#define PCA9538_REG_OUTPUT     0x01U
#define PCA9538_REG_POLARITY   0x02U
#define PCA9538_REG_CONFIG     0x03U

HAL_StatusTypeDef PCA9538_Write(uint16_t addr, uint8_t reg, uint8_t value);
HAL_StatusTypeDef PCA9538_Read(uint16_t addr, uint8_t reg, uint8_t *value);
HAL_StatusTypeDef PCA9538_SetPin(uint16_t addr, uint8_t pin, uint8_t level);
HAL_StatusTypeDef PCA9538_MakeOutput(uint16_t addr, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif
