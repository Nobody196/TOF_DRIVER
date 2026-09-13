# CubeMX changes required before build

This file is deliberately separate from the generated project files.

## 1. Correct I2C1 pin assignment

The **NIDAR-SENSE.ioc currently has**:

```text
PB6 -> I2C1_SCL
PB7 -> I2C1_SDA
```

The supplied **WeAct STM32H7 V1.2 schematic** labels the board I2C1 connection as:

```text
PB8 -> I2C1_SCL
PB9 -> I2C1_SDA
```

Therefore, in CubeMX change I2C1 to:

```text
PB8 = I2C1_SCL
PB9 = I2C1_SDA
```

Do not hand-edit the generated `.c` files to accomplish this. Change it in CubeMX and regenerate.

## 2. ToF interrupt

Choose the physical STM32 GPIO wired to VL53L5CX GPIO1/INT.

Configure it in CubeMX as:

```text
Label: TOF_INT
Mode: GPIO_EXTI
Trigger: Rising edge
NVIC: Enabled for its EXTI IRQ
```

CubeMX must generate:

```c
TOF_INT_Pin
TOF_INT_GPIO_Port
```

The application driver then uses those symbols; it contains no hard-coded PB0/EXTI0 assumption.

## 3. USB CDC

The current NIDAR-SENSE project contains the USB OTG FS peripheral configuration but does **not** yet contain the USB Device CDC middleware files.

Enable:

```text
USB_OTG_FS -> Device Only
USB Device middleware -> CDC
```

This should generate the USB device/CDC files, including the `CDC_Transmit_FS()` interface used by `serial.c`.

PA11 and PA12 are already the USB FS data pins in the NIDAR project.

## 4. I2C peripheral mode

Keep the generated I2C handle as:

```c
hi2c1
```

The ST ULD platform structure stores a pointer to that HAL handle.

## 5. Do not modify generated code manually

After making the CubeMX changes, regenerate the project.

Only the application calls from `main.c_additions.txt` belong in USER CODE sections.
