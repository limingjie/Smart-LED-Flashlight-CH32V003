# Smart LED Flashlight - CH32V003

**Smart LED Flashlight - CH32V003** is an open-source, compact, and energy-efficient flashlight solution based on the CH32V003 RISC-V microcontroller and SGM3732 LED driver.

It features multiple adjustable brightness lighting modes (Steady, Breathing, Blinking, SOS), battery monitoring, and zero standby current via a soft latching power circuit.

Designed for DIY enthusiasts, emergency kits, portable lighting, and educational projects.

- Low-cost, minimal component count
- USB or battery powered (3.0V–5.5V)
- Up to 10 LEDs in series
- Advanced features: PWM dimming, SOS mode, battery protection
- Open source hardware and firmware

---

- [Smart LED Flashlight - CH32V003](#smart-led-flashlight---ch32v003)
  - [Circuit on Breadboard](#circuit-on-breadboard)
  - [Schematic](#schematic)
  - [Features](#features)
  - [Components](#components)
    - [MCU - CH32V003](#mcu---ch32v003)
      - [Clock Selection](#clock-selection)
      - [Battery Monitoring](#battery-monitoring)
    - [LED Driver - SGM3732](#led-driver---sgm3732)
    - [Soft Latching Power Circuit](#soft-latching-power-circuit)
    - [LDO - ME6211](#ldo---me6211)
  - [Minimized ch32fun Library](#minimized-ch32fun-library)
  - [References](#references)
  - [License](#license)

## Circuit on Breadboard

![Circuit on Breadboard](./Images/Circuit_on_Breadboard.png)

## Schematic

![Schematic](./schematic/CH32V003_SGM3732_Portable_LED/CH32V003_SGM3732_Portable_LED.kicad_pcb.png)

## Features

- Low-cost
- PWM-controlled adjustable brightness
- Supports up to 10 LEDs in series
- Operates from `3.0V` to `5.5V` (suitable for single cell lithium battery)
- Soft latching power circuit for zero standby current
- Automatic power monitoring and low-voltage lockout to prevent battery drain
- 4 Modes - `Steady`, `Breathing`, `Blinking`, and `SOS`.
  - Click/Double click `Mode` button to move to previous/next mode.
  - Hold `Mode` button anytime to directly enter `SOS` mode.
- 8 Levels
  - Click/Double click `Level` button to increase/decrease.
  - Hold `Level` button to switch between min and max.

## Components

### MCU - CH32V003

The CH32V003 is a low-cost, high-performance 32-bit RISC-V microcontroller from WCH. It features a compact design, low power consumption, and a rich set of peripherals, making it ideal for embedded applications and cost-sensitive projects. The CH32V003 supports various interfaces such as UART, SPI, I2C, and PWM, and is well-suited for controlling LED drivers and other hardware components.

- [CH32V003 Datasheet](./Documents/CH32V003%20Datasheet%20-%20V1.7.PDF)
- [CH32V003 Reference Manual](./Documents/CH32V003%20Reference%20Manual%20-%20V1.7.PDF)

#### Clock Selection

With a `1.5MHz` clock (`1/16` of the internal high-speed clock), the CH32V003 draws around `1.53mA` (the power LED draws around `1.35mA`). Clocks lower than `1.5MHz` do not seem to work properly after enabling the ADC and may brick the chip. If this happens, try the unbrick command (`minichlink -u`) or flash a firmware with a higher clock; note that it may require more than 10 attempts. The chip is quite robust, but recovering it may require patience!

#### Battery Monitoring

When using a lithium battery as the power supply, and considering the cutoff voltages of the CH32V003 and SGM3732, the lockout voltage is set at `3.0V` if detected 3 times.

- CH32V003: `2.8V`-`5.5V` (With ADC).
- SGM3732: `2.7V`-`5.5V`.

Since the power supply can drop below `3.3V`, directly using `3.3V` as the ADC reference would lead to inaccurate results. Fortunately, the CH32V003 provides an internal voltage reference (`1.2V` on analog channel 8), which allows for accurate ADC measurements with an error within `±1%`.

$$
\begin{align}
\text{V}_\text{Reference} &= \text{V}_\text{DD} \times \frac{\text{ADC}_{\text{Reference}}}{1023}
\Longrightarrow \text{V}_\text{DD} = \frac{\text{V}_\text{Reference}}{\frac{\text{ADC}_{\text{Reference}}}{1023}} \\
\text{V}_\text{Battery} &= \text{V}_\text{DD} \times \frac{\text{ADC}_{\text{Battery}}}{1023} \\
&= \frac{\text{V}_\text{Reference}}{\frac{\text{ADC}_{\text{Reference}}}{1023}} \times \frac{\text{ADC}_{\text{Battery}}}{1023} \\
&= \text{V}_\text{Reference} \times \frac{\text{ADC}_{\text{Battery}}}{\text{ADC}_\text{Reference}}\\
&=  1.2\,\text{V} \times \frac{\text{ADC}_{\text{Battery}}}{\text{ADC}_\text{Reference}}
\end{align}
$$

### LED Driver - SGM3732

The [SGM3732](https://www.sg-micro.com/product/SGM3732) is a high-efficiency constant current LED driver with a 1.1MHz PWM boost converter, optimized for compact designs using small components. It can drive up to 10 LEDs in series (up to 38V output) or deliver up to 260mA with 3 LEDs per string, while maintaining high conversion efficiency. LED current is programmable via a digital PWM dimming interface (2kHz–60kHz). The device features very low shutdown current and includes protections such as over-voltage, cycle-by-cycle input current limit, and thermal shutdown. The SGM3732 is available in a TSOT-23-6 package and operates from -40℃ to +85℃.

- [SGM3732 Datasheet Rev A4](./Documents/SGM3732%20Datasheet%20-%20Rev%20A4.pdf)
- [SGM3732 Datasheet Rev A3](./Documents/SGM3732%20Datasheet%20-%20Rev%20A3.pdf) – The older version includes circuit design and component selection guidance.

### Soft Latching Power Circuit

Refer to the [CH32V003 Soft Latching Power Circuits](https://github.com/limingjie/CH32V003-Soft-Latching-Power-Circuits) project.

### LDO - ME6211

The CH32V003 operates from `2.7V` to `5.5V`, but shows noticeable current fluctuations when powered directly from USB. Using an LDO stabilizes the MCU’s power supply and reduces current consumption by a few milliamps.

- [ME6211 Datasheet - V24](./Documents/ME6211%20Datasheet%20-%20V26.pdf)

## Minimized ch32fun Library

- **`ch32fun.mk`** - Some modifications to accommodate library and tool paths.
  - The `LDFLAGS` path change is for locating the `ch32fun/libgcc.a`.

    ```diff
    <     LDFLAGS+=-L$(CH32FUN)/../misc -lgcc
    ---
    >     LDFLAGS+=-L$(CH32FUN) -lgcc
    ```

    - Otherwise, the compiler would use the default libgcc and throw an incompatible linking error:

      ```text
      .../riscv64-unknown-elf/bin/ld:
      .../riscv-gnu-toolchain/main/lib/gcc/riscv64-unknown-elf/15.1.0/libgcc.a(div.o):
      ABI is incompatible with that of the selected emulation:
      target emulation `elf64-littleriscv' does not match `elf32-littleriscv'
      ```

  - The other two changes are for `minichlink`:

    ```diff
    33c33
    < MINICHLINK?=$(CH32FUN)/../minichlink
    ---
    > MINICHLINK?=$(CH32FUN)
    51c51
    <     LDFLAGS+=-L$(CH32FUN)/../misc -lgcc
    ---
    >     LDFLAGS+=-L$(CH32FUN) -lgcc
    350c350
    <     make -C $(MINICHLINK) all
    ---
    >     # make -C $(MINICHLINK) all
    ```

- **`ch32fun.c`**
  - Changed line `1728` for `1.5MHz` HCLK, resulting in lower power consumption (<`1.6mA`).

    ```c
    1720: #elif defined(FUNCONF_USE_HSI) && FUNCONF_USE_HSI
    1721: #if defined(CH32V30x) || defined(CH32V20x) || defined(CH32V10x)
    1722:     EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;
    1723: #endif
    1724: #if defined(FUNCONF_USE_PLL) && FUNCONF_USE_PLL
    1725:     RCC->CFGR0 = BASE_CFGR0;
    1726:     RCC->CTLR  = BASE_CTLR | RCC_HSION | RCC_PLLON; // Use HSI, enable PLL.
    1727: #else
    1728:     RCC->CFGR0 = RCC_HPRE_DIV16;                    // HCLK = SYSCLK / 16
    1729:     RCC->CTLR  = BASE_CTLR | RCC_HSION;             // Use HSI only.
    1730: #endif
    ```

  - Set ADC sampling time to `241` cycles for better accuracy.

    ```c
    1808: // set sampling time for all channels to 0b111 (241 cycles).
    1809: ADC1->SAMPTR2 = (ADC_SMP0<<(3*0)) | (ADC_SMP0<<(3*1)) | (ADC_SMP0<<(3*2)) | (ADC_SMP0<<(3*3)) | (ADC_SMP0<<  (3*4)) | (ADC_SMP0<<(3*5)) | (ADC_SMP0<<(3*6)) | (ADC_SMP0<<(3*7)) | (ADC_SMP0<<(3*8)) | (ADC_SMP0<<(3*9));
    1810: ADC1->SAMPTR1 = (ADC_SMP0<<(3*0)) | (ADC_SMP0<<(3*1)) | (ADC_SMP0<<(3*2)) | (ADC_SMP0<<(3*3)) | (ADC_SMP0<<  (3*4)) | (ADC_SMP0<<(3*5));
    ```

## References

- [Andrew Levido: Soft Latching Power Circuits](https://circuitcellar.com/resources/quickbits/soft-latching-power-circuits/)
- [CNLohr: ch32fun](https://github.com/cnlohr/ch32fun)
- [A Guide to Debouncing](https://my.eng.utah.edu/~cs5780/debouncing.pdf)
- [The simplest button debounce solution](https://www.e-tinkers.com/2021/05/the-simplest-button-debounce-solution/)
- [Museum of the Game - Let's design some POKEY replacements](https://forums.arcade-museum.com/threads/lets-design-some-pokey-replacements.515774/post-4623716)

## License

![CC by-nc-sa](Images/by-nc-sa.svg)

This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/).

**You are free to:**

- **Share** — copy and redistribute the material in any medium or format
- **Adapt** — remix, transform, and build upon the material

The licensor cannot revoke these freedoms as long as you follow the license terms.

**Under the following terms:**

- **Attribution** - You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
- **NonCommercial** - You may not use the material for commercial purposes.
- **ShareAlike** - If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
- **No additional restrictions** — You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.

**Notices:**

You do not have to comply with the license for elements of the material in the public domain or where your use is permitted by an applicable exception or limitation.

No warranties are given. The license may not give you all of the permissions necessary for your intended use. For example, other rights such as publicity, privacy, or moral rights may limit how you use the material.
