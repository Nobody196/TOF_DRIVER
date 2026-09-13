# Integration Reference

## Files

- `tof.c/.h` — sensor driver and latest-frame provider
- `platform.c/.h` — STM32 HAL adapter required by the ST ULD
- `serial.c/.h` — USB CDC telemetry
- `main.c_additions.txt` — only the additions permitted in generated `main.c`
- `MANUAL.md` — full integration and operation manual

## Critical data path

The intended application interface is:

```c
const TOF_Data_t *tof = TOF_GetData();
```

No serial call is required to obtain ToF data.

## USB

The serial layer is USB CDC on the USB FS peripheral:

- PA11 = D-
- PA12 = D+

`SERIAL_Process()` must not contain a blocking wait for USB completion.

## ST ULD

All files under the ST ULD directory are treated as vendor files and are not modified.

The platform adapter supplies the functions required by the ULD.

## Interrupt

The sensor data-ready interrupt is handled by the STM32 EXTI callback. The callback only sets a flag. I2C data retrieval occurs in `TOF_Process()`.

## Generated code

Do not manually edit generated peripheral initialization. Configure the corresponding peripherals in CubeMX and regenerate. Keep application calls inside USER CODE sections.
