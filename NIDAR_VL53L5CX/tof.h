#ifndef TOF_H
#define TOF_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NIDAR-SENSE operating point, selected through the ST VL53L5CX ULD API. */
#define TOF_RESOLUTION             VL53L5CX_RESOLUTION_4X4
#define TOF_ZONE_COUNT             16U
#define TOF_RANGING_FREQUENCY_HZ   60U
#define TOF_RANGING_MODE           VL53L5CX_RANGING_MODE_CONTINUOUS

typedef struct
{
    int16_t distance_mm[TOF_ZONE_COUNT];
    uint8_t target_status[TOF_ZONE_COUNT];
    uint8_t target_count[TOF_ZONE_COUNT];
    uint8_t frame_id;
    uint8_t valid;
} TOF_Data_t;

HAL_StatusTypeDef TOF_Init(void);
void TOF_Process(void);

const TOF_Data_t *TOF_GetData(void);
uint8_t TOF_DataReady(void);
void TOF_AcknowledgeFrame(uint8_t frame_id);

uint8_t TOF_GetLastStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* TOF_H */
