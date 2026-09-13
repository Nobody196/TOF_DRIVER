#ifndef VL53L5CX_PLATFORM_H
#define VL53L5CX_PLATFORM_H

#include "main.h"
#include <stdint.h>

/*
 * STM32H743 + VL53L5CX ULD platform layer.
 *
 * The files in VL53L5CX_ULD_API/inc and /src are ST files and are not
 * modified by this project. This header only supplies the platform object
 * and the mandatory functions expected by the ULD.
 */

typedef struct
{
    uint16_t address;                 /* ST ULD address format: 0x52 */
    I2C_HandleTypeDef *i2c;           /* I2C peripheral used by this sensor */
} VL53L5CX_Platform;

#define VL53L5CX_NB_TARGET_PER_ZONE   1U

/* Keep only what the NIDAR data path needs. */
#define VL53L5CX_DISABLE_AMBIENT_PER_SPAD
#define VL53L5CX_DISABLE_NB_SPADS_ENABLED
#define VL53L5CX_DISABLE_SIGNAL_PER_SPAD
#define VL53L5CX_DISABLE_RANGE_SIGMA_MM
#define VL53L5CX_DISABLE_REFLECTANCE_PERCENT
#define VL53L5CX_DISABLE_MOTION_INDICATOR

uint8_t VL53L5CX_RdByte(VL53L5CX_Platform *p_platform,
                        uint16_t RegisterAdress,
                        uint8_t *p_value);

uint8_t VL53L5CX_WrByte(VL53L5CX_Platform *p_platform,
                        uint16_t RegisterAdress,
                        uint8_t value);

uint8_t VL53L5CX_RdMulti(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t *p_values,
                         uint32_t size);

uint8_t VL53L5CX_WrMulti(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAdress,
                         uint8_t *p_values,
                         uint32_t size);

uint8_t VL53L5CX_Reset_Sensor(VL53L5CX_Platform *p_platform);

void VL53L5CX_SwapBuffer(uint8_t *buffer, uint16_t size);

uint8_t VL53L5CX_WaitMs(VL53L5CX_Platform *p_platform,
                        uint32_t TimeMs);

#endif /* VL53L5CX_PLATFORM_H */
