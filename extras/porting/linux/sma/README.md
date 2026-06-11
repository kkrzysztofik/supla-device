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
   `Uac`, `E-Total`. Use those names in `supla-device.yaml`.
6. Many inverters (including **WR33-008**) do not answer standalone
   `CMD_GET_CINFO` the way YASDI uses during normal operation — YASDI loads
   channel metadata from `yasdi/build/devices/<type>.bin` after the first
   detection. Set `device.profile` in YAML (built-in `WR33-008` or a copied
   `.bin` file). See [Device profiles](#device-profiles) below.

### Simplified configuration (recommended)

```yaml
channels:
  - type: SmaInverter
    caption: SMA AC
    serial: &sma_serial
      device: /dev/ttyUSB0
      baud: 1200
      media: RS485
    device: &sma_device
      net_address: 1
      profile: WR33-008
    sma_channels:
      Pac: power_active
      Uac: voltage
      Fac: frequency
      "Iac-Ist": current
      "E-Total": fwd_act_energy

  - type: SmaDcMeter
    caption: SMA DC
    serial: *sma_serial
    device: *sma_device
    sma_channels:
      "Upv-Ist": voltage
      Ipv: current
```

`SmaInverter`, `SmaDcMeter`, `SmaThermometer`, and `SmaMeasurement` on the same
serial port share one RS485 poller (`SmaBus`). `SmaDcMeter` derives DC power
from voltage × current when both are mapped.

Additional channel types (one `sma_channels` entry each):

| YAML type | SUPLA channel | Example YASDI names |
|-----------|---------------|---------------------|
| `SmaThermometer` | Thermometer | `Tkk` |
| `SmaMeasurement` | General purpose measurement | `Zac`, `Riso` |

SUPLA mapping aliases: `pac`, `uac`, `fac`, `totwh`, `iac`, `upv`, `ipv`,
`tkk`, `zac`, `riso`, `gpm`.

### Device profiles

YASDI sequence for spot values (what works on WR33):

1. `CMD_SYN_ONLINE` (broadcast, 1 s wait)
2. `CMD_GET_DATA` to `net_address` with mask `0x090f` (bulk spot read)

Channel metadata (`ctype`, gain, order) comes from a **profile**, not from live
`CMD_GET_CINFO` on every poll.

| `device.profile` value | Source |
|------------------------|--------|
| `WR33-008` | Built-in catalog (verify against `yasdishell`) |
| `/path/to/WR33-008.bin` | YASDI cache file (recommended for production) |
| `SunnyBoy-5000` | Copy `yasdi/build/devices/<type>.bin` to `./sma-profiles/` |

Export from YASDI after successful detection:

```bash
mkdir -p sma-profiles
cp ~/yasdi/build/devices/WR33-008.bin sma-profiles/
```

Optional: `export SMA_PROFILES_DIR=/path/to/sma-profiles`.

If `device.profile` is omitted, the driver tries `CMD_GET_CINFO` (with retries),
then `CMD_GET_NET_START` detection and a profile lookup by device type.

### Advanced configuration (manual ctype/cindex)

You can specify protocol fields explicitly in YAML instead of names. See
[profiles/README.md](profiles/README.md).

## Build and test on Linux

### Dependencies (Debian/Ubuntu)

Same as sd4linux — see [extras/examples/linux/README.md](../../../../examples/linux/README.md#dependencies-debian--ubuntu).

Build:

```bash
sudo apt install build-essential cmake git libssl-dev libyaml-cpp-dev
```

Runtime (deployed binary only): `libssl3`, `libyaml-cpp0.8`, `ca-certificates`.
RS485 needs `dialout` group access, not an extra library.

### Build with the SMA extension

From the repository root:

```bash
cd extras/examples/linux
./build-sma.sh
```

Or manually:

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

- `supla.mail` and `supla.server` (SUPLA Cloud or your server)
- `serial.device` — RS485 adapter path, e.g. `/dev/ttyUSB0` (same as
  `yasdi-posix.ini` `[COM1] Device=`)
- On first run, GUID/AuthKey are written to `state_files_path/guid_auth.yaml`
  — register the device in SUPLA Cloud with those values

Hardware-specific template for WR33-008:
`extras/examples/linux/supla-device-wr33-008.yaml`
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
- `no channel catalog` — set `device.profile: WR33-008` (or copy YASDI
  `devices/<type>.bin`); WR33 often does not respond to `CMD_GET_CINFO`
- `bulk spot read failed` — stop `yasdishell`, check baud (1200 for WR33) and
  `net_address: 1`
- Only one process may use the RS485 port — stop `yasdishell` before starting
  `supla-device-linux`

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
