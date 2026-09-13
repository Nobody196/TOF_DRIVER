#include "serial.h"
#include "tof.h"

#include <string.h>

/*
 * CubeMX-generated USB CDC interface.
 *
 * After enabling USB Device -> CDC in CubeMX, the generated
 * usbd_cdc_if.c normally provides CDC_Transmit_FS().
 */
#include "usbd_cdc_if.h"

static uint8_t serial_tx_buffer[SERIAL_PACKET_BYTES];
static uint8_t serial_tx_in_flight;
static uint32_t serial_dropped_frames;

#define SERIAL_SOF0             0xA5U
#define SERIAL_SOF1             0x5AU
#define SERIAL_PROTOCOL_VERSION 1U

static uint16_t serial_crc16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;

    for (uint32_t i = 0U; i < length; ++i)
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

HAL_StatusTypeDef SERIAL_Init(void)
{
    serial_tx_in_flight = 0U;
    serial_dropped_frames = 0U;
    memset(serial_tx_buffer, 0, sizeof(serial_tx_buffer));

    return HAL_OK;
}

void SERIAL_Process(void)
{
    const TOF_Data_t *tof;
    uint8_t frame_id;
    uint16_t crc;

    /*
     * CDC_Transmit_FS() is the generated USB interface. Its normal contract
     * is non-blocking and returns USBD_BUSY when the previous transfer is
     * still active.
     */
    if (serial_tx_in_flight != 0U)
    {
        /*
         * We do not have to poll the private CDC TxState here. The generated
         * CDC transmit function owns the USB state. A new frame is attempted
         * on a later loop pass.
         *
         * The in-flight flag is cleared when the next transmit attempt is
         * accepted; if the USB stack is still busy, no new packet is built.
         */
        if (TOF_DataReady() == 0U)
        {
            return;
        }
    }

    if (TOF_DataReady() == 0U)
    {
        return;
    }

    tof = TOF_GetData();
    frame_id = tof->frame_id;

    serial_tx_buffer[0] = SERIAL_SOF0;
    serial_tx_buffer[1] = SERIAL_SOF1;
    serial_tx_buffer[2] = SERIAL_PROTOCOL_VERSION;
    serial_tx_buffer[3] = frame_id;

    for (uint32_t i = 0U; i < TOF_ZONE_COUNT; ++i)
    {
        uint16_t d = (uint16_t)tof->distance_mm[i];

        serial_tx_buffer[4U + (2U * i)] = (uint8_t)(d & 0xFFU);
        serial_tx_buffer[5U + (2U * i)] = (uint8_t)(d >> 8);
    }

    memcpy(&serial_tx_buffer[36U],
           tof->target_status,
           TOF_ZONE_COUNT);

    memcpy(&serial_tx_buffer[52U],
           tof->target_count,
           TOF_ZONE_COUNT);

    crc = serial_crc16(serial_tx_buffer, 68U);
    serial_tx_buffer[68U] = (uint8_t)(crc & 0xFFU);
    serial_tx_buffer[69U] = (uint8_t)(crc >> 8);

    if (CDC_Transmit_FS(serial_tx_buffer, SERIAL_PACKET_BYTES) == USBD_OK)
    {
        serial_tx_in_flight = 1U;
        TOF_AcknowledgeFrame(frame_id);
    }
    else
    {
        /*
         * Keep the frame ready. USB can be slower than the sensor and may
         * legitimately reject an attempt with USBD_BUSY.
         */
        serial_tx_in_flight = 0U;
        ++serial_dropped_frames;
    }
}

uint32_t SERIAL_GetDroppedFrames(void)
{
    return serial_dropped_frames;
}
