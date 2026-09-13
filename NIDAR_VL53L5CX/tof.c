#include "tof.h"
#include "platform.h"
#include "i2c.h"
#include "vl53l5cx_api.h"

#include <string.h>

/*
 * NIDAR-SENSE sensor connection selected from the WeAct STM32H7xx V12
 * schematic:
 *   I2C1_SCL -> PB8
 *   I2C1_SDA -> PB9
 *
 * GPIO1/INT is assigned to PB0. PB0 is a free GPIO on the board header and
 * supports EXTI0 on STM32H743VIT6. Wire VL53L5CX GPIO1 -> PB0.
 */
#define TOF_I2C_ADDRESS       VL53L5CX_DEFAULT_I2C_ADDRESS
#define TOF_INT_GPIO_PORT     GPIOB
#define TOF_INT_GPIO_PIN      GPIO_PIN_0
#define TOF_INT_IRQn          EXTI0_IRQn
#define TOF_INT_IRQ_PRIORITY  6U
#define TOF_RANGING_HZ        10U

static VL53L5CX_Configuration tof_dev;
static VL53L5CX_ResultsData tof_results;

/* Double buffer: producer writes inactive buffer, then atomically publishes. */
static TOF_Data_t tof_buffers[2];
static volatile uint8_t tof_active_buffer = 0U;
static volatile uint8_t tof_irq_pending = 0U;
static volatile uint8_t tof_frame_ready = 0U;

static void TOF_INT_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = TOF_INT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOF_INT_GPIO_PORT, &gpio);

    HAL_NVIC_SetPriority(TOF_INT_IRQn, TOF_INT_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(TOF_INT_IRQn);
}

HAL_StatusTypeDef TOF_Init(void)
{
    uint8_t alive = 0U;
    uint8_t status;

    memset(&tof_dev, 0, sizeof(tof_dev));
    memset(&tof_results, 0, sizeof(tof_results));
    memset(tof_buffers, 0, sizeof(tof_buffers));

    tof_dev.platform.address = TOF_I2C_ADDRESS;
    tof_dev.platform.i2c = &hi2c1;

    /* Configure GPIO1 -> EXTI0 before ranging starts. */
    TOF_INT_Init();

    status = vl53l5cx_is_alive(&tof_dev, &alive);
    if ((status != VL53L5CX_STATUS_OK) || (alive == 0U))
    {
        return HAL_ERROR;
    }

    status = vl53l5cx_init(&tof_dev);
    if (status != VL53L5CX_STATUS_OK)
    {
        return HAL_ERROR;
    }

    status = vl53l5cx_set_resolution(&tof_dev,
                                     VL53L5CX_RESOLUTION_8X8);
    if (status != VL53L5CX_STATUS_OK)
    {
        return HAL_ERROR;
    }

    status = vl53l5cx_set_ranging_frequency_hz(&tof_dev,
                                               TOF_RANGING_HZ);
    if (status != VL53L5CX_STATUS_OK)
    {
        return HAL_ERROR;
    }

    status = vl53l5cx_start_ranging(&tof_dev);
    if (status != VL53L5CX_STATUS_OK)
    {
        return HAL_ERROR;
    }

    tof_irq_pending = 0U;
    tof_frame_ready = 0U;

    return HAL_OK;
}

/*
 * EXTI callback is intentionally tiny. Never perform I2C in an ISR.
 * stm32h7xx_it.c remains generated; Cube's EXTI handler calls this callback.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TOF_INT_GPIO_PIN)
    {
        tof_irq_pending = 1U;
    }
}

void TOF_Process(void)
{
    TOF_Data_t *next;
    uint8_t status;
    uint8_t next_index;
    uint32_t zone;

    if (tof_irq_pending == 0U)
    {
        return;
    }

    /* Claim the pending event. A later interrupt can set it again. */
    tof_irq_pending = 0U;

    /* ULD acquisition is synchronous; it is deliberately NOT in the ISR. */
    status = vl53l5cx_get_ranging_data(&tof_dev, &tof_results);
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
        const uint32_t target = zone * VL53L5CX_NB_TARGET_PER_ZONE;

        next->distance_mm[zone] = tof_results.distance_mm[target];
        next->target_status[zone] = tof_results.target_status[target];
        next->target_count[zone] = tof_results.nb_target_detected[zone];
    }

    /* Publish only after the entire frame has been filled. */
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

void TOF_ClearDataReady(void)
{
    tof_frame_ready = 0U;
}
