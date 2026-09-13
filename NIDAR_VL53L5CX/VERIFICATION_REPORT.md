# VL53L5CX / NIDAR-SENSE cross-verification

## Source-of-truth set

This review cross-checks:

1. NIDAR-SENSE project archive and `NIDAR-SENSE.ioc`
2. WeAct STM32H7 V12 schematic supplied with the project
3. STSW-IMG023, VL53L5CX ULD driver 2.0.1
4. ST UM2884 VL53L5CX ULD user manual
5. Current `Nobody196/TOF_DRIVER` repository files

## Result

The requested sensor operating point is valid:

```text
4x4 = 16 zones
60 Hz maximum at 4x4
continuous ranging
1 target per zone
```

ST's ULD defines `VL53L5CX_RESOLUTION_4X4` as 16 and `VL53L5CX_RESOLUTION_8X8` as 64. UM2884 specifies 1–60 Hz for 4x4 and 1–15 Hz for 8x8.

## Important corrections discovered

### A. NIDAR `.ioc` does not currently match the board schematic

The supplied NIDAR-SENSE `.ioc` currently configures:

```text
PB6 = I2C1_SCL
PB7 = I2C1_SDA
```

The supplied WeAct schematic labels:

```text
PB8 = I2C1_SCL
PB9 = I2C1_SDA
```

The repository's own `integration_notes.txt` also explicitly identifies this mismatch and instructs changing I2C1 to PB8/PB9.

Therefore this is a **CubeMX project configuration correction**, not something `platform.c` should hide.

### B. The repository hard-coded the ToF interrupt

The current repository's `tof.c` hard-codes:

```c
GPIOB / GPIO_PIN_0 / EXTI0
```

and initializes the GPIO/NVIC itself.

That conflicts with the desired generated-code ownership model.

The revised driver instead requires CubeMX to create:

```c
TOF_INT_Pin
TOF_INT_GPIO_Port
```

and configures no GPIO/NVIC itself.

### C. The current repository is 8x8 / 10 Hz

The current `tof.c` requests:

```c
VL53L5CX_RESOLUTION_8X8
10 Hz
```

and `tof.h` allocates 64 zones.

The revised driver requests:

```c
VL53L5CX_RESOLUTION_4X4
60 Hz
```

and exposes 16 zones.

### D. ST initialization already performs the sensor firmware load

In ULD 2.0.1, `vl53l5cx_init()` contains the boot sequence and firmware transfer. The source writes the firmware in three blocks:

```text
0x8000
0x8000
0x5000
```

for a total of approximately 84 Kbytes.

NIDAR does not need to duplicate this.

### E. Platform layer responsibility is confirmed

ST's supplied `Platform/platform.c` marks the following functions for customer implementation:

```text
RdByte
WrByte
RdMulti
WrMulti
Reset_Sensor (optional)
SwapBuffer
WaitMs
```

The revised `platform.c` supplies these using STM32 HAL.

The resolution, frequency and ranging-mode settings remain in `tof.c` through ST's public API.

### F. `VL53L5CX_NB_TARGET_PER_ZONE` is a compile-time ULD setting

ST's platform header documents this as a user-adjustable value from 1 to 4.

The NIDAR configuration keeps:

```c
#define VL53L5CX_NB_TARGET_PER_ZONE 1U
```

This is appropriate for the current 16-zone/one-target output.

### G. Output reduction is done through ST-supported macros

The ULD `start_ranging()` implementation checks the `VL53L5CX_DISABLE_*` macros from `platform.h` when building the output list and calculating `data_read_size`.

The revised platform header keeps:

```text
nb_target_detected
distance_mm
target_status
```

and disables currently unused large outputs.

## Data-ready flow

The ST example documents two ways to determine readiness:

1. Poll with `vl53l5cx_check_data_ready()`
2. Use the hardware interrupt on GPIO1

The NIDAR implementation uses the second method.

The EXTI callback only sets a flag.
`TOF_Process()` performs `vl53l5cx_get_ranging_data()` in normal application context.

## One important hardware/software boundary

The current NIDAR-SENSE archive does not yet contain the USB Device CDC middleware generated files.

The current `.ioc` has USB OTG FS Device_Only configured and PA11/PA12 assigned to USB FS, but `usb_device.c`, `usbd_cdc_if.c`, and related CDC middleware are not present in the archive.

Therefore:

```text
ToF driver:
    can be integrated once the ULD files are copied in.

USB telemetry:
    requires the CubeMX USB Device CDC middleware to be enabled/generated.
```

This is an integration prerequisite, not an ST ULD problem.

## Final approval state

| Item | State |
|---|---|
| VL53L5CX ULD 2.0.1 API compatibility | Verified against supplied source |
| 4x4 / 16 zones | Verified |
| 60 Hz at 4x4 | Verified |
| Continuous mode | Verified |
| 1 target/zone | Verified |
| Firmware initialization | Verified in ST source |
| Platform function set | Verified |
| I2C transaction style | Matches ST STM32 example |
| Board I2C mapping | **CubeMX must change PB6/PB7 -> PB8/PB9** |
| ToF interrupt ownership | Revised to CubeMX-generated EXTI symbols |
| USB CDC | Requires CubeMX middleware generation |
| Current hardware operation | Not physically tested |
| Current code build | Must be performed after integration/CubeMX changes |

## Bottom line

The sensor configuration requested by NIDAR is valid and supported by ST.

The two hardware configuration facts we must preserve are:

```text
WeAct V12 board:
    I2C1 = PB8/PB9

NIDAR current .ioc:
    I2C1 = PB6/PB7   <-- must be corrected in CubeMX
```

and:

```text
VL53L5CX GPIO1/INT
    -> any correctly wired EXTI-capable STM32 GPIO
    -> CubeMX label TOF_INT
    -> generated TOF_INT_Pin / TOF_INT_GPIO_Port
    -> tof.c callback
```

No hard-coded PB0/EXTI0 remains in the revised application driver.
