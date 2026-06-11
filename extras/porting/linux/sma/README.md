# Minimal SMA RS485/SMANet client (sd4linux)

Read-only SMAData client for Linux `supla-device`. Protocol behavior is
reimplemented in C++ and validated against YASDI 1.8.3 (reference only — no
runtime dependency on `libyasdi`).

## License and YASDI attribution

**supla-device SMA module** — Copyright (C) Krzysztof Krzysztofik, licensed
under the GNU General Public License v2 or later (same as supla-device).

**YASDI reference library** — Protocol algorithms and constants in this module
follow the behavior of [YASDI](https://www.sma.de/) (*Yet Another SMA Data
Implementation*), Copyright (C) 2001-2008 SMA Solar Technology AG, which is
distributed under the **GNU Lesser General Public License v2.1 or later
(LGPL-2.1+)**. A copy of the LGPL is typically shipped as `COPYING.LIB` with
YASDI; the full license text is also available at
<https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.

This directory contains **original C++ reimplementations** for supla-device.
No YASDI source files are linked or vendored into the build. YASDI is used only
as an interoperability reference and for validation (e.g. `yasdishell` on
hardware). Each ported file documents its YASDI reference paths in the file
header.

| Module | YASDI reference sources |
|--------|-------------------------|
| `smanet_framer.*` | `sdk/protocol/smanet.c`, `smanet.h` |
| `sma_serial_port.*` | `sdk/driver/serial_posix.c` |
| `smadata_client.*` | `sdk/core/smadata_layer.c`, `smadata_cmd.h`, `statereadchan.c` |
| `sma_channel_codec.*` | `sdk/master/netchannel.c`, `statereadchan.c`, `tools.c`, `chandef.h` |
| `sma_types.h` | `chandef.h`, `smadata_cmd.h`, `smanet.h`, `smadata_layer.h` |

`sma_inverter.*` and the sd4linux extension are SUPLA integration layers and do
not incorporate YASDI protocol code.

## Hardware profiling (one-time per inverter)

1. Build YASDI with the serial driver (debug output is on by default):
   `cd yasdi/sdk/projects/generic-cmake && cmake -B build && cmake --build build`
   For a quiet library build: `cmake -B build -DYASDI_DEBUG_OUTPUT=OFF`
2. Configure `yasdi-posix.ini` for your USB-RS485 adapter (port, baud, media).
   Ensure `[Misc] DebugOutput=stderr` is set so protocol traces are visible.
3. Run `yasdishell` from the build directory and list devices/channels.

### Debugging device discovery

`yasdishell` emits two log layers during discovery (command `e` sync, `b` async,
or `./yasdishell yasdi-posix.ini autodetect`):

| Layer | Prefix / format | Content |
|-------|-----------------|---------|
| Shell | `[detect] ...` | Target count, devices found (handle, name, type, serial), timing, error codes |
| Library | `[timestamp] TStateDetect::...` | SMAData state machine, transport protocol, packet-level traces |

Rebuild with debug if needed:

```bash
cd yasdi/sdk/projects/generic-cmake
cmake -B build -DYASDI_DEBUG_OUTPUT=ON
cmake --build build
./build/yasdishell yasdi-posix.ini autodetect
```

See also [YASDI architecture — interactive exploration](../../../../docs/integrations/yasdi-architecture.md).
4. Note `net_address`, serial port, and baud from `yasdi-posix.ini`.
5. List spot channel **names** from `yasdishell` (`a` command), e.g. `Pac`,
   `Uac`, `E-Total`. Use those names in `supla-device.yaml`; the driver fetches
   `ctype`, `cindex`, `ntype`, `gain`, and `offset` from the inverter via
   `CMD_GET_CINFO` at startup.

### Simplified configuration (recommended)

```yaml
channels:
  - type: SmaInverter
    serial:
      device: /dev/ttyUSB0
      baud: 1200
      media: RS485
    device:
      net_address: 1
    sma_channels:
      Pac: power_active
      Uac: voltage
      Fac: frequency
      "E-Total": fwd_act_energy
```

SUPLA mapping aliases: `pac`, `uac`, `fac`, `totwh`, `iac` (see extension).

### Advanced configuration (manual ctype/cindex)

If CINFO is unavailable, you can still specify protocol fields explicitly
(profiled with YASDI property dumps). See `profiles/README.md`.

See [profiles/README.md](profiles/README.md) for an example profile template.

## Build and test on Linux

### Dependencies (Debian/Ubuntu)

```bash
sudo apt install git libssl-dev build-essential libyaml-cpp-dev cmake
```

### Build with the SMA extension

From the repository root:

```bash
cd extras/examples/linux
cmake -B build -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/sma
cmake --build build -j$(nproc)
```

The binary is produced at:

```text
extras/examples/linux/build/supla-device-linux
```

Verify:

```bash
./build/supla-device-linux --version
```

Without `-DSUPLA_LINUX_EXTENSION_DIRS=.../sma`, the binary builds but YAML type
`SmaInverter` is rejected as unknown.

### Prepare configuration

Copy and edit the example config:

```bash
cp supla-device-sma.yaml my-sma.yaml
```

Set at least:

- `network.email` and `network.server` (SUPLA Cloud or your server)
- `device.guid` and `device.auth_key` (from SUPLA Cloud device registration)
- `serial.device` — RS485 adapter path, e.g. `/dev/ttyUSB0`
- `serial.baud` and `serial.media`
- `device.net_address` and `sma_channels.*` — from `yasdishell` profiling
  (placeholder values in the example may not match your inverter)

Serial port access (add your user to the `dialout` group, then log out/in):

```bash
sudo usermod -aG dialout $USER
```

### Run

```bash
cd extras/examples/linux
./build/supla-device-linux -c my-sma.yaml
```

Verbose logging:

```bash
./build/supla-device-linux -c my-sma.yaml --verbose
```

If `-c` is omitted, sd4linux looks for `./etc/supla-device.yaml` or
`/etc/supla-device.yaml`.

### What to expect

- Log line similar to: `adding SmaInverter on /dev/ttyUSB0...`
- SMA polling runs on a worker thread; the main `SuplaDevice.iterate()` loop is
  not blocked by serial I/O
- Wrong serial path or channel metadata shows read failures or
  `CMD_GET_CINFO failed` in logs

### Rebuild after code changes

```bash
cmake --build build -j$(nproc)
```

### One-liner (from repository root)

```bash
cd extras/examples/linux && \
cmake -B build -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/sma && \
cmake --build build -j$(nproc) && \
./build/supla-device-linux -c supla-device-sma.yaml --verbose
```

## YAML channel type

Example channel (see `extras/examples/linux/supla-device-sma.yaml`):

```yaml
channels:
  - type: SmaInverter
    serial:
      device: /dev/ttyUSB0
      baud: 9600
      media: RS485
    device:
      net_address: 1
    poll_interval_sec: 15
    sma_channels:
      pac:
        ctype: 0x0801
        cindex: 0
        ntype: 0x0004
        gain: 1.0
        offset: 0.0
        supla: power_active
```

## Module layout

| File | Role |
|------|------|
| `sma_serial_port.*` | POSIX serial, RS485 RTS/DTR |
| `smanet_framer.*` | HDLC SMANet framing and FCS16 |
| `smadata_client.*` | Blocking SMAData (sync online, GET_DATA) |
| `sma_channel_codec.*` | Parse channel values, gain/offset |
| `sma_inverter.*` | `Supla::PV::SmaInverter` electricity meter |
