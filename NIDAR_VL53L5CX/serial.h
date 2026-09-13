#ifndef SERIAL_H
#define SERIAL_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB CDC telemetry packet size. */
#define SERIAL_FRAME_BYTES  204U

HAL_StatusTypeDef SERIAL_Init(void);

/*
 * Non-blocking USB CDC service. Never waits for USB completion.
 * Call from the normal loop.
 */
void SERIAL_Process(void);

/* Number of completed ToF frames dropped because USB was busy. */
uint32_t SERIAL_GetDroppedFrames(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
