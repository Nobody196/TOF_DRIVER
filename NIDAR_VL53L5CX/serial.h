#ifndef SERIAL_H
#define SERIAL_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packet:
 *   0..1    sync 0xA5 0x5A
 *   2       protocol version
 *   3       ToF frame ID
 *   4..35   16 x int16 distance_mm, little endian
 *   36..51  16 x uint8 target_status
 *   52..67  16 x uint8 target_count
 *   68..69  CRC16-CCITT, little endian
 */
#define SERIAL_PACKET_BYTES 70U

HAL_StatusTypeDef SERIAL_Init(void);
void SERIAL_Process(void);

uint32_t SERIAL_GetDroppedFrames(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
