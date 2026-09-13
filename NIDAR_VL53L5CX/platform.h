#ifndef VL53L5CX_PLATFORM_H
#define VL53L5CX_PLATFORM_H

#include "main.h"
#include <stdint.h>

/*
 * ST ULD 2.0.1 customer platform structure.
 * Only the application-specific communication descriptor is added here.
 */
typedef struct
{
    uint16_t address;               /* ST ULD uses 0x52 for the default address */
    I2C_HandleTypeDef *i2c;         /* STM32 HAL I2C handle */
} VL53L5CX_Platform;

/*
 * ST ULD compile-time result configuration.
 * Valid values: 1..4 targets per zone.
 */
#define VL53L5CX_NB_TARGET_PER_ZONE 1U

/*
 * Keep only data required by the NIDAR path:
 *   nb_target_detected, distance_mm, target_status.
 *
 * These macros are consumed by the ST ULD; do not modify ST source files.
 */
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
