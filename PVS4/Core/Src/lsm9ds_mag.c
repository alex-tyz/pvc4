#include "lsm9ds_mag.h"
#include "i2c.h"

#define LSM_TIMEOUT_MS      10U
#define LSM_REG_WHO_AM_I    0x0FU
#define LSM_AUTO_INCREMENT  0x80U

#define DS1_WHO_AM_I_M      0x3DU
#define DS1_CTRL_REG1_M     0x20U
#define DS1_CTRL_REG2_M     0x21U
#define DS1_CTRL_REG3_M     0x22U
#define DS1_CTRL_REG4_M     0x23U
#define DS1_CTRL_REG5_M     0x24U
#define DS1_OUT_X_L_M       0x28U
#define DS1_SENS_X100       14

#define DS0_WHO_AM_I_XM     0x49U
#define DS0_CTRL_REG5_XM    0x24U
#define DS0_CTRL_REG6_XM    0x25U
#define DS0_CTRL_REG7_XM    0x26U
#define DS0_OUT_X_L_M       0x08U
#define DS0_SENS_X100       16

static const uint16_t candidates[] = { 0x3CU, 0x38U, 0x3AU };

static LSM9DS_Type type = LSM9DS_NONE;
static uint16_t    address;

static HAL_StatusTypeDef WriteReg(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1, address, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U, LSM_TIMEOUT_MS);
}

static HAL_StatusTypeDef ReadRegs(uint8_t reg, uint8_t *data, uint16_t len)
{
  return HAL_I2C_Mem_Read(&hi2c1, address, reg, I2C_MEMADD_SIZE_8BIT, data, len, LSM_TIMEOUT_MS);
}

static bool ConfigureDs1(void)
{
  /* XY ultra-high performance, 80 Hz; continuous conversion, BDU, +/-4 gauss. */
  return WriteReg(DS1_CTRL_REG1_M, 0x7CU) == HAL_OK
      && WriteReg(DS1_CTRL_REG2_M, 0x00U) == HAL_OK
      && WriteReg(DS1_CTRL_REG3_M, 0x00U) == HAL_OK
      && WriteReg(DS1_CTRL_REG4_M, 0x0CU) == HAL_OK
      && WriteReg(DS1_CTRL_REG5_M, 0x40U) == HAL_OK;
}

static bool ConfigureDs0(void)
{
  /* High resolution, 50 Hz; continuous conversion, +/-4 gauss. */
  return WriteReg(DS0_CTRL_REG5_XM, 0x74U) == HAL_OK
      && WriteReg(DS0_CTRL_REG6_XM, 0x20U) == HAL_OK
      && WriteReg(DS0_CTRL_REG7_XM, 0x00U) == HAL_OK;
}

bool LSM9DS_Mag_Init(void)
{
  type = LSM9DS_NONE;
  for (uint32_t i = 0U; i < sizeof(candidates) / sizeof(candidates[0]); i++)
  {
    uint8_t id;
    address = candidates[i];
    if (ReadRegs(LSM_REG_WHO_AM_I, &id, 1U) != HAL_OK)
    {
      continue;
    }
    if (id == DS1_WHO_AM_I_M && ConfigureDs1())
    {
      type = LSM9DS_TYPE_DS1;
      return true;
    }
    if (id == DS0_WHO_AM_I_XM && ConfigureDs0())
    {
      type = LSM9DS_TYPE_DS0;
      return true;
    }
  }
  return false;
}

LSM9DS_Type LSM9DS_Mag_GetType(void)
{
  return type;
}

const char *LSM9DS_Mag_GetName(void)
{
  switch (type)
  {
    case LSM9DS_TYPE_DS1: return "LSM9DS1";
    case LSM9DS_TYPE_DS0: return "LSM9DS0";
    default:              return "NONE";
  }
}

HAL_StatusTypeDef LSM9DS_Mag_Read(LSM9DS_MagData *mgauss)
{
  uint8_t raw[6];
  uint8_t reg;
  int32_t sens;
  HAL_StatusTypeDef status;

  if (type == LSM9DS_NONE)
  {
    return HAL_ERROR;
  }
  reg = (type == LSM9DS_TYPE_DS1) ? DS1_OUT_X_L_M : DS0_OUT_X_L_M;
  sens = (type == LSM9DS_TYPE_DS1) ? DS1_SENS_X100 : DS0_SENS_X100;
  status = ReadRegs((uint8_t)(reg | LSM_AUTO_INCREMENT), raw, sizeof(raw));
  if (status != HAL_OK)
  {
    return status;
  }
  mgauss->x = (int32_t)(int16_t)(raw[0] | (raw[1] << 8)) * sens / 100;
  mgauss->y = (int32_t)(int16_t)(raw[2] | (raw[3] << 8)) * sens / 100;
  mgauss->z = (int32_t)(int16_t)(raw[4] | (raw[5] << 8)) * sens / 100;
  return HAL_OK;
}
