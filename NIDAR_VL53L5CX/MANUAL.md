# NIDAR-SENSE VL53L5CX Driver + USB CDC Telemetry Manual

## 1. Purpose

This package separates the VL53L5CX ranging driver from PC telemetry.

The intended data flow is:

    VL53L5CX
       |
       +-- GPIO1/INT --> STM32 EXTI --> ToF driver
       |
       +-- I2C1 ------> platform.c/.h --> ST VL53L5CX ULD
                                      |
                                      v
                                latest ToF frame
                                   /       \
                                  /         \
                         control code      serial.c/.h
                                             |
                                             v
                                        USB CDC
                                      PA11 / PA12
                                             |
                                             v
                                             PC

The ST ULD API source and headers are not modified by this package.

## 2. Design rules

### ToF driver

`tof.c/.h` is the application-facing VL53L5CX driver.

It owns:
- sensor initialization
- ST ULD calls
- interrupt/data-ready handling
- latest-frame publication

It does NOT own:
- USB
- UART
- PC telemetry formatting
- application control logic

### Platform layer

`platform.c/.h` adapts the ST ULD to STM32 HAL.

It owns:
- I2C read/write
- multi-byte I2C transfers
- delay
- byte swapping where required by the ULD
- optional reset handling

### Serial layer

`serial.c/.h` consumes the latest ToF frame and sends telemetry to the PC.

It uses USB Device CDC. It must never block the application waiting for USB transmission.

## 3. Hardware mapping

For the WeAct STM32H743VIT6 V1.2 board used by this project:

- I2C1 SCL: PB8
- I2C1 SDA: PB9
- USB FS D-: PA11
- USB FS D+: PA12

The VL53L5CX GPIO1/INT input must be connected to a suitable STM32 EXTI-capable GPIO. The project implementation uses the symbolic generated name `TOF_INT_Pin`; the actual CubeMX pin definition must match the physical wiring.

Do not invent a different ToF interrupt pin in application code. Keep the pin mapping in the generated GPIO configuration.

## 4. STM32CubeMX configuration

Configure:

### I2C1
- I2C1 enabled
- PB8 = I2C1_SCL
- PB9 = I2C1_SDA
- Fast-mode / 400 kHz is the intended starting configuration
- Keep the HAL-generated handle as `hi2c1` unless the project already uses another name

### USB
- USB_OTG_FS
- Device Only
- USB Device middleware
- Communication Device Class (CDC)

PA11 and PA12 are reserved for USB FS.

### ToF interrupt
- GPIO connected to VL53L5CX GPIO1
- EXTI enabled
- rising-edge trigger as appropriate for the sensor/carrier circuit
- NVIC for the selected EXTI line enabled

## 5. ULD integration

The directory:

    Drivers/BSP/Components/VL53L5CX_ULD_API/

contains the ST files.

Do not modify:
- `vl53l5cx_api.c`
- `vl53l5cx_api.h`
- `vl53l5cx_buffers.h`
- `vl53l5cx_plugin_detection_thresholds.*`
- `vl53l5cx_plugin_motion_indicator.*`
- `vl53l5cx_plugin_xtalk.*`

The platform layer is the intended integration point.

The normal initialization sequence is:

    platform setup
        |
    vl53l5cx_is_alive()
        |
    vl53l5cx_init()
        |
    configure resolution
        |
    configure ranging frequency
        |
    vl53l5cx_start_ranging()

`vl53l5cx_init()` performs the sensor firmware/configuration transfer through the ULD platform I2C functions.

## 6. Interrupt-driven ranging

The VL53L5CX GPIO1 data-ready signal causes an STM32 EXTI interrupt.

The ISR must remain short.

The callback only records:

    tof_irq_pending = 1

It must NOT:
- perform an I2C transaction
- call `vl53l5cx_get_ranging_data()`
- format telemetry
- transmit USB data

`TOF_Process()` runs in normal application context. When the interrupt flag is set, it retrieves the completed frame through the ST ULD and publishes the result.

This is intentional: a synchronous I2C ULD read should not execute inside an ISR.

## 7. Data API for time-critical code

The public type is:

    TOF_Data_t

It contains:
- `distance_mm[64]`
- `target_status[64]`
- `target_count[64]`
- frame identifier
- validity state

The fast control code can access:

    const TOF_Data_t *tof = TOF_GetData();

Then:

    tof->distance_mm[zone]

can be used directly.

The control code should not:
- call the ST ULD directly
- perform I2C itself
- wait for USB
- depend on `SERIAL_Process()`

## 8. Main loop

Only minimal calls belong in `main.c`.

Inside USER CODE sections:

    #include "tof.h"
    #include "serial.h"

After generated peripheral initialization:

    MX_USB_DEVICE_Init();

    if (TOF_Init() != HAL_OK)
    {
        Error_Handler();
    }

    if (SERIAL_Init() != HAL_OK)
    {
        Error_Handler();
    }

In the main loop:

    TOF_Process();
    SERIAL_Process();

Do not add the ULD implementation to `main.c`.

## 9. USB CDC telemetry

`serial.c` uses USB CDC rather than a UART.

PA11/PA12 are therefore the physical USB FS D-/D+ connection.

The serial layer starts a CDC transfer and returns. It does not sit in a loop waiting for the PC.

The intended behavior is:

    SERIAL_Process()
        |
        +-- previous USB transfer busy?
        |       |
        |       +-- yes -> return immediately
        |
        +-- obtain newest ToF frame
        |
        +-- construct packet
        |
        +-- start CDC transfer
        |
        +-- return

If USB is busy, telemetry is allowed to lag/drop a frame. The ToF/control path must remain independent.

## 10. Telemetry packet

The package uses a compact binary packet rather than `printf()` CSV for the runtime telemetry path.

Conceptually:

    HEADER
    VERSION
    FRAME_ID
    64 x distance_mm
    64 x target_status
    64 x target_count
    CRC

The exact packet definition is documented in `serial.h`/`serial.c`.

A PC program can decode the packet into an 8x8 array.

## 11. Why serial is separate

The critical application loop should not be coupled to PC communications.

Bad architecture:

    control loop -> ToF -> USB transmit -> wait

Preferred architecture:

    ToF interrupt -> update data
                       |
                 control loop
                       |
                  latest frame

and independently:

    latest frame -> serial.c -> USB CDC

The PC link can therefore be slow or temporarily unavailable without making the control loop wait for USB.

## 12. Timing considerations

There are two different timing domains:

### Sensor acquisition

The sensor produces a ranging frame according to its configured ranging frequency.

### Application control loop

The control loop may run much faster than the ToF frame rate.

That is expected.

For example:

    control: 1 kHz
    ToF:      10 Hz

The control loop will read the same latest ToF frame between new sensor frames. It does not need to wait.

If a new frame arrives, the ToF driver replaces the published frame.

## 13. Build order

Recommended order:

1. Confirm I2C1 PB8/PB9 in CubeMX.
2. Confirm USB FS Device Only.
3. Enable USB Device CDC middleware.
4. Configure the actual VL53L5CX GPIO1 pin as EXTI.
5. Add `platform.c/.h`.
6. Add `tof.c/.h`.
7. Add `serial.c/.h`.
8. Add only the documented calls to `main.c`.
9. Build.
10. Connect the board by USB.
11. Confirm a USB CDC COM port appears.
12. Confirm ToF frames are arriving.
13. Only then connect the fast control application to `TOF_GetData()`.

## 14. Debugging checklist

### Sensor not detected
Check:
- sensor power
- I2C1 PB8/PB9
- I2C address
- pull-ups
- sensor reset/LPn wiring if present
- logic analyzer on SCL/SDA

### `TOF_Init()` fails
Check the return status from:
- `vl53l5cx_is_alive()`
- `vl53l5cx_init()`
- resolution configuration
- frequency configuration
- `vl53l5cx_start_ranging()`

The firmware initialization is a comparatively large I2C operation, so the platform multi-transfer implementation must be correct.

### Interrupt never arrives
Check:
- physical GPIO1 connection
- EXTI pin mapping
- EXTI edge
- NVIC
- sensor ranging actually started

### USB COM port does not appear
Check:
- USB Device middleware is enabled
- CDC class is selected
- `MX_USB_DEVICE_Init()` is called
- USB D-/D+ are PA11/PA12
- USB cable supports data

### PC telemetry stops
Check whether CDC is busy. The telemetry layer deliberately does not block. A busy USB endpoint should never stall the control loop.

## 15. Files and ownership

    tof.c/.h
        NIDAR application ToF driver

    platform.c/.h
        STM32 HAL adapter for ST ULD

    serial.c/.h
        USB CDC telemetry consumer

    main.c
        initialization/service calls only

    VL53L5CX_ULD_API/inc
    VL53L5CX_ULD_API/src
        ST supplied ULD -- leave untouched

## 16. Important modification policy

System-generated STM32 files should not be manually polluted.

If CubeMX regenerates:
- GPIO
- I2C
- USB
- clocks
- interrupt setup

let CubeMX own those sections.

Application logic belongs in:
- `tof.c/.h`
- `platform.c/.h`
- `serial.c/.h`

and only their calls belong in the designated USER CODE sections of `main.c`.

## 17. Current assumptions

This implementation assumes:
- STM32H743VIT6
- STM32 HAL
- ST VL53L5CX ULD
- I2C1 handle named `hi2c1`
- USB CDC generated by CubeMX
- USB CDC transmit interface available to `serial.c`
- 8x8 ranging
- 10 Hz ranging
- one target retained per zone

If the generated project uses different symbol names, change the integration symbols in the application wrapper, not the ST ULD API.
