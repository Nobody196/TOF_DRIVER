#include "serial.h"
#include "tof.h"

/* These are supplied by STM32Cube USB Device middleware after CDC is enabled. */
#include "usb_device.h"
#include "usbd_cdc.h"
#include "usbd_core.h"

#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

#define SERIAL_SOF0             0xA5U
#define SERIAL_SOF1             0x5AU
#define SERIAL_PROTOCOL_VERSION 1U

/*
 * One complete binary packet:
 *   0-1   : 0xA5 0x5A
 *   2     : protocol version
 *   3     : ToF frame ID
 *   4-131 : 64 x int16 distance_mm, little endian
 *   132-195: 64 x uint8 target status
 *   196-259: 64 x uint8 target count
 *   260-261: CRC16-CCITT
 *
 * This is intentionally binary rather than printf/ASCII: much less CPU work
 * and much less USB bandwidth for a fast telemetry path.
 */
#define SERIAL_PACKET_BYTES 262U

static uint8_t serial_tx_buffer[SERIAL_PACKET_BYTES];
static uint8_t serial_tx_in_flight = 0U;
static uint32_t serial_dropped_frames = 0U;

static uint16_t SERIAL_CRC16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;

    for (i = 0U; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 0x8000U) ?
                  (uint16_t)((crc << 1) ^ 0x1021U) :
                  (uint16_t)(crc << 1);
        }
    }

    return crc;
}

static uint8_t SERIAL_USB_Configured(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

static uint8_t SERIAL_USB_TxBusy(void)
{
    USBD_CDC_HandleTypeDef *cdc;

    if (hUsbDeviceFS.pClassData == NULL)
    {
        return 0U;
    }

    cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    return (cdc->TxState != 0U) ? 1U : 0U;
}

HAL_StatusTypeDef SERIAL_Init(void)
{
    serial_tx_in_flight = 0U;
    serial_dropped_frames = 0U;
    return HAL_OK;
}

void SERIAL_Process(void)
{
    const TOF_Data_t *tof;
    uint32_t i;
    uint16_t crc;
    uint16_t length = SERIAL_PACKET_BYTES;

    if (!SERIAL_USB_Configured())
    {
        return;
    }

    /* Never wait for a previous USB transfer. */
    if (serial_tx_in_flight != 0U)
    {
        if (SERIAL_USB_TxBusy() != 0U)
        {
            return;
        }

        serial_tx_in_flight = 0U;
    }

    if (TOF_DataReady() == 0U)
    {
        return;
    }

    tof = TOF_GetData();

    serial_tx_buffer[0] = SERIAL_SOF0;
    serial_tx_buffer[1] = SERIAL_SOF1;
    serial_tx_buffer[2] = SERIAL_PROTOCOL_VERSION;
    serial_tx_buffer[3] = tof->frame_id;

    /* Distances: 64 x little-endian int16. */
    for (i = 0U; i < TOF_ZONE_COUNT; ++i)
    {
        const uint16_t d = (uint16_t)tof->distance_mm[i];
        serial_tx_buffer[4U + (2U * i)] = (uint8_t)(d & 0xFFU);
        serial_tx_buffer[5U + (2U * i)] = (uint8_t)(d >> 8);
    }

    /* Status and target-count arrays. */
    memcpy(&serial_tx_buffer[132U],
           tof->target_status,
           TOF_ZONE_COUNT);

    memcpy(&serial_tx_buffer[196U],
           tof->target_count,
           TOF_ZONE_COUNT);

    crc = SERIAL_CRC16(serial_tx_buffer, 260U);
    serial_tx_buffer[260U] = (uint8_t)(crc & 0xFFU);
    serial_tx_buffer[261U] = (uint8_t)(crc >> 8);

    /*
     * Directly use the ST USB CDC class API. It only starts the USB transfer;
     * it does not busy-wait for completion.
     */
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS,
                         serial_tx_buffer,
                         length);

    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
    {
        serial_tx_in_flight = 1U;
        TOF_ClearDataReady();
    }
    else
    {
        /* USB was busy. Keep the ToF frame; try again next pass. */
        ++serial_dropped_frames;
    }
}

uint32_t SERIAL_GetDroppedFrames(void)
{
    return serial_dropped_frames;
}
