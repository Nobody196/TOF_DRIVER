#include "platform.h"

#include <string.h>

#define VL53L5CX_I2C_TIMEOUT_MS  100U
#define VL53L5CX_I2C_MEM_TIMEOUT 1000U

static uint8_t status_from_hal(HAL_StatusTypeDef status)
{
    return (status == HAL_OK) ? 0U : 1U;
}

uint8_t VL53L5CX_RdByte(VL53L5CX_Platform *p_platform,
                        uint16_t RegisterAdress,
                        uint8_t *p_value)
{
    uint8_t reg[2];

    if ((p_platform == NULL) ||
        (p_platform->i2c == NULL) ||
        (p_value == NULL))
    {
        return 1U;
    }

    reg[0] = (uint8_t)(RegisterAdress >> 8);
    reg[1] = (uint8_t)(RegisterAdress & 0xFFU);

    if (HAL_I2C_Master_Transmit(p_platform->i2c,
                                p_platform->address,
                                reg, 2U,
                                VL53L5CX_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1U;
    }

    return status_from_hal(
        HAL_I2C_Master_Receive(p_platform->i2c,
                               p_platform->address,
                               p_value, 1U,
                               VL53L5CX_I2C_TIMEOUT_MS));
}

uint8_t VL53L5CX_WrByte(VL53L5CX_Platform *p_platform,
                        uint16_t RegisterAdress,
                        uint8_t value)
{
    uint8_t data[3];

    if ((p_platform == NULL) || (p_platform->i2c == NULL))
    {
        return 1U;
    }

    data[0] = (uint8_t)(RegisterAdress >> 8);
    data[1] = (uint8_t)(RegisterAdress & 0xFFU);
    data[2] = value;

    return status_from_hal(
        HAL_I2C_Master_Transmit(p_platform->i2c,
                                p_platform->address,
                                data, 3U,
                                VL53L5CX_I2C_TIMEOUT_MS));
}

uint8_t VL53L5CX_WrMulti(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t *p_values,
                         uint32_t size)
{
    if ((p_platform == NULL) ||
        (p_platform->i2c == NULL) ||
        (p_values == NULL) ||
        (size > 0xFFFFU))
    {
        return 1U;
    }

    return status_from_hal(
        HAL_I2C_Mem_Write(p_platform->i2c,
                          p_platform->address,
                          RegisterAdress,
                          I2C_MEMADD_SIZE_16BIT,
                          p_values,
                          (uint16_t)size,
                          VL53L5CX_I2C_MEM_TIMEOUT));
}

uint8_t VL53L5CX_RdMulti(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t *p_values,
                         uint32_t size)
{
    uint8_t reg[2];

    if ((p_platform == NULL) ||
        (p_platform->i2c == NULL) ||
        (p_values == NULL) ||
        (size > 0xFFFFU))
    {
        return 1U;
    }

    /*
     * This matches the ST supplied STM32 example: transmit the 16-bit
     * register address, then receive the requested payload.
     */
    reg[0] = (uint8_t)(RegisterAdress >> 8);
    reg[1] = (uint8_t)(RegisterAdress & 0xFFU);

    if (HAL_I2C_Master_Transmit(p_platform->i2c,
                                p_platform->address,
                                reg, 2U,
                                VL53L5CX_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1U;
    }

    return status_from_hal(
        HAL_I2C_Master_Receive(p_platform->i2c,
                               p_platform->address,
                               p_values,
                               (uint16_t)size,
                               VL53L5CX_I2C_MEM_TIMEOUT));
}

uint8_t VL53L5CX_Reset_Sensor(VL53L5CX_Platform *p_platform)
{
    /*
     * No reset/power GPIO is currently part of the NIDAR-SENSE board
     * configuration. ST marks this platform callback optional.
     */
    (void)p_platform;
    return 0U;
}

void VL53L5CX_SwapBuffer(uint8_t *buffer, uint16_t size)
{
    uint32_t i;
    uint32_t tmp;

    /*
     * ST ULD contract: size is always a multiple of four.
     * Preserve the ST supplied implementation semantics.
     */
    for (i = 0U; i < size; i += 4U)
    {
        tmp = ((uint32_t)buffer[i] << 24) |
              ((uint32_t)buffer[i + 1U] << 16) |
              ((uint32_t)buffer[i + 2U] << 8) |
              ((uint32_t)buffer[i + 3U]);

        memcpy(&buffer[i], &tmp, 4U);
    }
}

uint8_t VL53L5CX_WaitMs(VL53L5CX_Platform *p_platform,
                        uint32_t TimeMs)
{
    (void)p_platform;
    HAL_Delay(TimeMs);
    return 0U;
}
