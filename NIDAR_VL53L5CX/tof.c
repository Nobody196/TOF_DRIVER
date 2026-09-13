#include "tof.h"
#include "platform.h"
#include "i2c.h"
#include "vl53l5cx_api.h"

#include <string.h>

/*
 * GPIO/EXTI ownership:
 *
 * The physical VL53L5CX GPIO1/INT connection is configured by CubeMX.
 * Give that pin the CubeMX label "TOF_INT", with GPIO_EXTI + rising edge
 * and the corresponding EXTI IRQ enabled.
 *
 * CubeMX then provides:
 *     TOF_INT_Pin
 *     TOF_INT_GPIO_Port
 *
 * No GPIO/NVIC initialization is performed by this driver.
 */
#ifndef TOF_INT_Pin
#error "Configure the VL53L5CX GPIO1 connection in CubeMX with label TOF_INT"
#endif

#define TOF_I2C_ADDRESS VL53L5CX_DEFAULT_I2C_ADDRESS

static VL53L5CX_Configuration tof_dev;
static VL53L5CX_ResultsData tof_results;

/* Producer fills the inactive buffer and publishes only a complete frame. */
static TOF_Data_t tof_buffers[2];
static volatile uint8_t tof_active_buffer;
static volatile uint8_t tof_irq_pending;
static volatile uint8_t tof_frame_ready;
static volatile uint8_t tof_last_status = VL53L5CX_STATUS_ERROR;

HAL_StatusTypeDef TOF_Init(void)
{
    uint8_t status;
    uint8_t alive = 0U;

    memset(&tof_dev, 0, sizeof(tof_dev));
    memset(&tof_results, 0, sizeof(tof_results));
    memset(tof_buffers, 0, sizeof(tof_buffers));

    tof_active_buffer = 0U;
    tof_irq_pending = 0U;
    tof_frame_ready = 0U;

    tof_dev.platform.address = TOF_I2C_ADDRESS;
    tof_dev.platform.i2c = &hi2c1;

    /*
     * Optional presence check. The mandatory ULD initialization below is
     * what loads the sensor firmware after power-on.
     */
    status = vl53l5cx_is_alive(&tof_dev, &alive);
    if ((status != VL53L5CX_STATUS_OK) || (alive == 0U))
    {
        tof_last_status = (status != VL53L5CX_STATUS_OK) ?
                          status : VL53L5CX_STATUS_ERROR;
        return HAL_ERROR;
    }

    /*
     * ST ULD initialization:
     * reboot/boot sequence, firmware download, FW check, NVM/offset handling,
     * default configuration and related initialization are performed by the ULD.
     */
    status = vl53l5cx_init(&tof_dev);
    if (status != VL53L5CX_STATUS_OK)
    {
        tof_last_status = status;
        return HAL_ERROR;
    }

    /*
     * ST requires resolution to be selected before changing ranging frequency.
     * 4x4 = 16 zones and permits the requested 60 Hz.
     */
    status = vl53l5cx_set_resolution(&tof_dev, TOF_RESOLUTION);
    if (status != VL53L5CX_STATUS_OK)
    {
        tof_last_status = status;
        return HAL_ERROR;
    }

    status = vl53l5cx_set_ranging_frequency_hz(
                 &tof_dev, TOF_RANGING_FREQUENCY_HZ);
    if (status != VL53L5CX_STATUS_OK)
    {
        tof_last_status = status;
        return HAL_ERROR;
    }

    status = vl53l5cx_set_ranging_mode(&tof_dev, TOF_RANGING_MODE);
    if (status != VL53L5CX_STATUS_OK)
    {
        tof_last_status = status;
        return HAL_ERROR;
    }

    status = vl53l5cx_start_ranging(&tof_dev);
    if (status != VL53L5CX_STATUS_OK)
    {
        tof_last_status = status;
        return HAL_ERROR;
    }

    tof_last_status = VL53L5CX_STATUS_OK;
    return HAL_OK;
}

/*
 * Called by the HAL EXTI mechanism. Do not perform I2C here.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TOF_INT_Pin)
    {
        tof_irq_pending = 1U;
    }
}

void TOF_Process(void)
{
    uint8_t status;
    uint8_t next_index;
    uint32_t zone;
    TOF_Data_t *next;

    if (tof_irq_pending == 0U)
    {
        return;
    }

    /* Claim this event before the synchronous ULD read. */
    tof_irq_pending = 0U;

    status = vl53l5cx_get_ranging_data(&tof_dev, &tof_results);
    tof_last_status = status;

    if (status != VL53L5CX_STATUS_OK)
    {
        return;
    }

    next_index = (uint8_t)(tof_active_buffer ^ 1U);
    next = &tof_buffers[next_index];

    next->frame_id = tof_dev.streamcount;
    next->valid = 1U;

    for (zone = 0U; zone < TOF_ZONE_COUNT; ++zone)
    {
        const uint32_t target =
            zone * VL53L5CX_NB_TARGET_PER_ZONE;

        next->distance_mm[zone] =
            tof_results.distance_mm[target];

        next->target_status[zone] =
            tof_results.target_status[target];

        next->target_count[zone] =
            tof_results.nb_target_detected[zone];
    }

    /* Publish after all 16 zones have been copied. */
    tof_active_buffer = next_index;
    tof_frame_ready = 1U;
}

const TOF_Data_t *TOF_GetData(void)
{
    return &tof_buffers[tof_active_buffer];
}

uint8_t TOF_DataReady(void)
{
    return tof_frame_ready;
}

void TOF_AcknowledgeFrame(uint8_t frame_id)
{
    /*
     * Only clear the ready flag if the frame being acknowledged is still
     * the currently published frame. A newer frame remains ready.
     */
    if ((tof_frame_ready != 0U) &&
        (tof_buffers[tof_active_buffer].frame_id == frame_id))
    {
        tof_frame_ready = 0U;
    }
}

uint8_t TOF_GetLastStatus(void)
{
    return tof_last_status;
}
