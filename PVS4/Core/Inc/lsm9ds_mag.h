#ifndef __LSM9DS_MAG_H__
#define __LSM9DS_MAG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  LSM9DS_NONE = 0U,
  LSM9DS_TYPE_DS0,
  LSM9DS_TYPE_DS1
} LSM9DS_Type;

typedef struct
{
  int32_t x;
  int32_t y;
  int32_t z;
} LSM9DS_MagData;

bool              LSM9DS_Mag_Init(void);
LSM9DS_Type       LSM9DS_Mag_GetType(void);
const char       *LSM9DS_Mag_GetName(void);
HAL_StatusTypeDef LSM9DS_Mag_Read(LSM9DS_MagData *mgauss);

#ifdef __cplusplus
}
#endif

#endif
