#ifndef TOF_H
#define TOF_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOF_ZONE_COUNT 64U

typedef struct
{
    int16_t  distance_mm[TOF_ZONE_COUNT];
    uint8_t  target_status[TOF_ZONE_COUNT];
    uint8_t  target_count[TOF_ZONE_COUNT];
    uint8_t  frame_id;
    uint8_t  valid;
} TOF_Data_t;

/* Sensor setup + continuous ranging start. */
HAL_StatusTypeDef TOF_Init(void);

/*
 * Fast, non-blocking service entry. Call from the normal application loop.
 * It returns immediately unless the sensor has asserted its data-ready IRQ.
 */
void TOF_Process(void);

/*
 * Lock-free access to the most recently published complete frame.
 * The returned pointer is read-only and remains valid until the next publish.
 */
const TOF_Data_t *TOF_GetData(void);

/* True after the first complete frame has been published. */
uint8_t TOF_DataReady(void);

/* Clears the software new-frame indication after the application consumes it. */
void TOF_ClearDataReady(void);

#ifdef __cplusplus
}
#endif

#endif /* TOF_H */
