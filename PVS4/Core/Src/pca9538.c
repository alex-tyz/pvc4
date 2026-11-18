#include "pca9538.h"
#include "i2c.h"

#define PCA9538_TIMEOUT_MS  10U

HAL_StatusTypeDef PCA9538_Write(uint16_t addr, uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U, PCA9538_TIMEOUT_MS);
}

HAL_StatusTypeDef PCA9538_Read(uint16_t addr, uint8_t reg, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT, value, 1U, PCA9538_TIMEOUT_MS);
}

HAL_StatusTypeDef PCA9538_SetPin(uint16_t addr, uint8_t pin, uint8_t level)
{
  uint8_t out;
  HAL_StatusTypeDef status = PCA9538_Read(addr, PCA9538_REG_OUTPUT, &out);
  if (status != HAL_OK)
  {
    return status;
  }
  out = level ? (uint8_t)(out | (1U << pin)) : (uint8_t)(out & ~(1U << pin));
  return PCA9538_Write(addr, PCA9538_REG_OUTPUT, out);
}

HAL_StatusTypeDef PCA9538_MakeOutput(uint16_t addr, uint8_t pin)
{
  uint8_t cfg;
  HAL_StatusTypeDef status = PCA9538_Read(addr, PCA9538_REG_CONFIG, &cfg);
  if (status != HAL_OK)
  {
    return status;
  }
  return PCA9538_Write(addr, PCA9538_REG_CONFIG, (uint8_t)(cfg & ~(1U << pin)));
}
