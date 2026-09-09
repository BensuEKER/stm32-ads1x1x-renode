# STM32 + Renode + ADS1x1x Sensor Driver

A small-scale embedded software development pipeline: a portable C driver
for the ADS1014 / ADS1015 / ADS1115 I2C ADC family, an STM32F407VG
firmware that uses it, a Renode C# model of the sensor so the whole
thing can run without real hardware, and automated unit + integration
tests wired into a GitHub Actions CI pipeline.

```
Datasheet → C Driver → STM32 Firmware → Renode → C# Sensor Model
          → Python Integration Test → Unit Tests → GitHub Actions
```

---

## Overview

The goal was not just to write a sensor driver, but to set up a small
version of a real embedded software workflow: a hardware-independent
driver, a firmware that uses it, a simulated version of the sensor so
the firmware can run without real hardware, and automated tests at two
levels (unit tests of the driver logic, and integration tests of the
whole firmware+simulator stack) that all run automatically in CI on
every push.

## Hardware / MCU

**MCU: STM32F407VG** (ARM Cortex-M4, 168 MHz, 1 MB Flash, 192 KB SRAM).

Chosen because:
- Enough performance and RAM headroom for a driver + test/mocking
  layers without constantly fighting resource limits.
- Multiple I2C, SPI, and USART peripherals available.
- One of the most common STM32 series, with extensive documentation
  and community resources.
- Officially and maturely supported by Renode — NVIC, SysTick, RCC,
  GPIO, UART, and I2C are all modeled and can be described in `.repl`
  platform files, and a compiled `.elf` can run directly on Renode
  without real hardware, which is exactly what this project needed.

**Communication peripherals:**
- **I2C1** — talks to the ADS1x1x sensor: writes configuration
  registers and reads 12/16-bit conversion data.
- **USART1** — reports firmware status and measurement results
  (`Sensor initialized`, `Sensor configuration OK`,
  `Voltage: ... mV`) to the outside world (Renode's UART terminal).

**Software layer: STM32 HAL.** Chosen because it hides register-level
detail behind a portable, readable API, while the sensor driver itself
is written to depend on **callback function pointers** rather than on
HAL directly (see [Architecture](#architecture)) — this is what makes
the driver testable on a host machine with no HAL and no hardware at
all.

## Sensor

**ADS1014 / ADS1015 / ADS1115** — a family of I2C analog-to-digital
converters from Texas Instruments. A single project driver
(`lib/ads1x1x/`) supports all three, selected via an enum passed to
`ads1x1x_init()`. The three chips share an identical register map
(Conversion / Config / Lo_thresh / Hi_thresh); the differences are
resolution (12-bit vs 16-bit) and whether channel selection (MUX) is
present at all (ADS1014 has none — a fixed differential input only).

Full register-level documentation (addresses, bit fields, reset
values, and the differences between the three variants) is in
[`docs/sensor.md`](docs/sensor.md).

### What was carried from the datasheet into the Renode model

The C# model (`renode-model/ADS1x1x.cs`) mirrors the following
datasheet facts directly:
- The four register addresses (`0x00`–`0x03`) and their read/write
  access (Conversion is read-only).
- The Config register's bit layout (OS, MUX, PGA, MODE, DR, COMP_*)
  at the exact bit positions given in the datasheet.
- The datasheet's power-on-reset values (`Config = 0x8583`,
  `Lo_thresh = 0x8000`, `Hi_thresh = 0x7FFF`).
- The PGA index → full-scale-range table (`±6.144V` … `±0.256V`) used
  to convert a simulated voltage into a realistic ADC code, rather
  than returning a fixed value.
- The 12-bit family's left-aligned result encoding (low 4 bits always
  zero) vs the 16-bit family's full-width result.
- The Address Pointer Register convention (first I2C byte selects the
  active register, and its value persists across transactions on real
  hardware — the model does not reset it after each transmission).

Not modeled: the comparator's actual ALERT/RDY pin behavior and
conversion timing (see [Limitations](#limitations)).

## Architecture

```
                 ┌───────────────────────────┐
                 │        main.c              │
                 │  (STM32 HAL I2C wrapper)   │
                 └─────────────┬───────────────┘
                                │ function pointers
                                │ (ads1x1x_i2c_read_fn / write_fn)
                 ┌─────────────▼───────────────┐
                 │      lib/ads1x1x/            │
                 │  hardware-independent driver │
                 └─────────────┬───────────────┘
              ┌─────────────────┼─────────────────┐
              │ (real I2C bus)  │                 │ (fake, in test/)
   ┌──────────▼─────────┐       │        ┌────────▼────────┐
   │ STM32 HAL / Renode  │       │        │  test/support/   │
   │  I2C1 peripheral     │      │        │  fake_i2c.c/.h   │
   └──────────┬──────────┘       │        └──────────────────┘
              │                  │
   ┌──────────▼──────────────────▼──────┐
   │   renode-model/ADS1x1x.cs           │
   │   (Renode C# sensor model)          │
   └──────────────────────────────────────┘
```

The key design decision is that `lib/ads1x1x/` never calls HAL (or
anything platform-specific) directly — it only calls the
`ads1x1x_i2c_read_fn` / `ads1x1x_i2c_write_fn` callbacks it was given
at `ads1x1x_init()`. This is what lets:
- the **firmware** (`Core/Src/main.c`) supply real `HAL_I2C_Mem_*`
  wrappers,
- the **unit tests** (`test/`) supply a hand-written fake I2C bus with
  zero hardware or simulator dependency, and
- the **same compiled driver code** run against either, with no
  `#ifdef`s or platform-specific branches inside the driver itself.

On the simulation side, `renode-model/ADS1x1x.cs` implements Renode's
`II2CPeripheral` interface, so the STM32F407 platform (`renode/platform.repl`)
can attach it to I2C1 exactly as if it were a physical chip. Python
(`python/renode_controller.py`) then drives Renode headlessly to run
the integration tests end-to-end.

## Repository Structure

```
.
├── Core/                    # STM32 application code (main.c, HAL config, etc.)
├── Drivers/                 # STM32 HAL/CMSIS drivers (from STM32CubeMX)
├── lib/ads1x1x/
│   ├── inc/ads1x1x.h        # driver public API
│   └── src/ads1x1x.c        # driver implementation (HAL-independent)
├── cubeide/                 # STM32CubeIDE project metadata (for reopening
│                             # the .ioc in the IDE if needed) — not used
│                             # by the CMake/CI build
├── renode/
│   ├── platform.repl        # STM32F407VG + ADS1x1x on I2C1
│   ├── platform_no_sensor.repl  # same, but sensor NOT attached (error-path tests)
│   ├── sensor_test.resc     # boots the firmware with the sensor attached
│   └── boot_test.resc       # plain boot test (no sensor)
├── renode-model/
│   └── ADS1x1x.cs           # Renode C# sensor model
├── test/                    # Ceedling/Unity unit tests (Görev 7)
│   ├── test_ads1x1x.c
│   └── support/fake_i2c.h/.c
├── python/                  # Renode integration tests (Görev 8)
│   ├── renode_controller.py
│   ├── test_integration.py
│   └── requirements.txt
├── test-results/            # captured output of the last local test run
├── docs/
│   └── sensor.md            # full ADS1x1x register-level documentation
├── scripts/
│   └── run_all_tests.sh     # runs everything, saves logs to test-results/
├── cmake/
│   └── arm-none-eabi-gcc.cmake  # ARM cross-compile toolchain file
├── CMakeLists.txt           # firmware build definition
├── CMakePresets.json
├── project.yml              # Ceedling configuration
├── .clang-format
├── .github/workflows/ci.yml # GitHub Actions pipeline
├── run_tests.sh             # thin wrapper -> scripts/run_all_tests.sh
└── README.md
```

This mirrors the assignment's suggested layout closely, with two
intentional adjustments: the driver lives under `lib/ads1x1x/` (with
its own `inc`/`src`, as needed for Ceedling), and the STM32CubeIDE
project's own metadata is kept in a separate `cubeide/` folder rather
than under `firmware/`, since `Core/`/`Drivers/` had to move to the
repo root for the CMake build.

## Building the Firmware

Requires `arm-none-eabi-gcc` and `cmake`.

```bash
cmake --preset debug
cmake --build --preset debug
```

Produces `build/STM32F407VG_ADS1x1x.elf`.

## Running Renode

Requires [Renode](https://renode.io) (the portable, self-contained
build is recommended — no system `.NET` dependency needed).

Build the firmware first (see above), then, from the repo root:

```bash
renode renode/sensor_test.resc
```

This compiles and registers the C# sensor model, loads the platform
with the sensor attached to I2C1, loads the `.elf`, sets a simulated
input voltage, and boots the firmware. A UART terminal window shows
the firmware's live output (`Sensor initialized`, `Sensor configuration
OK`, `Voltage: ... mV`).

## Running Unit Tests

Requires [Ceedling](https://www.throwtheswitch.org/ceedling) (Ruby
gem). No hardware or Renode dependency — the driver is tested against
a hand-written fake I2C bus (`test/support/fake_i2c.c`).

```bash
ceedling test:all
```

## Running Integration Tests

Requires Renode and Python (`pip install -r python/requirements.txt`).
Build the firmware first.

```bash
cd python
pytest -v test_integration.py
```

This drives a real Renode simulation headlessly (no GUI) and verifies
sensor initialization, configuration, register read/write, measurement
values, and the firmware's behavior when the sensor is not attached.

## Running Everything Locally

```bash
./run_tests.sh
```

Runs the unit tests, the firmware build, and the integration tests in
sequence, saving the full output of each to `test-results/*.log`. The
same combined output is also published as a
[public GitHub Gist](https://gist.github.com/BensuEKER/adbdbd26cdc26da2f985d9304a7b8808)
for quick viewing without cloning the repo.

## GitHub Actions

`.github/workflows/ci.yml` runs on every push and pull request, with
three jobs:

1. **C Unit Tests** — installs Ruby + Ceedling, runs `ceedling test:all`.
2. **Firmware Build** — installs the ARM toolchain + CMake, builds the
   `.elf`, uploads it as a build artifact.
3. **Renode Integration Tests** — depends on the build job; downloads
   the `.elf`, installs Renode (portable build) and the Python
   dependencies, runs the integration tests against it.

Any failing step fails its job (GitHub Actions' default behavior — no
special configuration needed), and a job that depends on a failed one
(via `needs:`) is skipped rather than run.

## Design Decisions

- **Callback-based hardware abstraction.** The driver never calls HAL
  directly; it's given `read`/`write` function pointers at init time.
  This single decision is what makes both the unit tests (fake I2C,
  no hardware) and the Renode integration tests (real simulated I2C)
  possible against the exact same driver binary/source.
- **A single library for all three chip variants**, selected via an
  enum, rather than three near-duplicate drivers — the register map
  is identical; only a few behaviors (MUX presence, resolution)
  differ, and the driver validates those differences at the API level
  (e.g., rejecting a non-default MUX value on ADS1014).
- **`fake_i2c` naming instead of `mock_i2c`** in the unit tests:
  Ceedling/CMock treats any included header named `mock_X.h` as an
  auto-generated mock of a real `X.h`, which broke for our
  hand-written fake — renaming it avoided that collision.
- **CMake instead of the STM32CubeIDE-generated Makefile** for the
  final build, since the IDE's Makefile embeds machine-specific
  absolute paths and IDE-specific quirks that don't survive outside
  the IDE or on a CI runner; a small, explicit `CMakeLists.txt` is
  more portable and easier to reason about.
- **`Write()` in the Renode sensor model supports two I2C write call
  patterns** (a combined 1-call write of pointer+data, and a 2-call
  split of pointer-then-data) because different HAL wrappers — this
  project's own, and a real-hardware wrapper used to cross-check the
  model — chunk I2C writes differently. Supporting both makes the
  model usable against either style without firmware changes.
- **Portable Renode build (bundled `.NET`)** instead of the `.deb`
  package, to sidestep a `dotnet` version mismatch between Ubuntu's
  package repositories and the version Renode expects — used
  consistently in local development and in CI.

## Limitations

- The comparator (Lo/Hi threshold ALERT/RDY behavior) is modeled only
  at the register read/write level — the model does not simulate the
  ALERT pin actually asserting when a threshold is crossed.
- Conversions complete instantly in the simulation; the model does not
  simulate the data-rate-dependent conversion delay a real chip has.
- Only a single sensor instance on I2C1 has been tested; the model has
  not been exercised with multiple I2C devices sharing a bus.
- The `cubeide/` folder (STM32CubeIDE project metadata) is kept only
  so the `.ioc` pin/peripheral configuration can be reopened and
  edited in the IDE if needed — it is not part of the CMake/CI build
  and can drift out of sync with `Core/`/`Drivers/` if the IDE
  project is regenerated without re-syncing.
