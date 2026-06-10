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

1. Build YASDI with the serial driver:
   `cd yasdi/sdk/projects/generic-cmake && cmake -B build && cmake --build build`
2. Configure `yasdi-posix.ini` for your USB-RS485 adapter (port, baud, media).
3. Run `yasdishell` from the build directory and list devices/channels.
4. For each channel you need (e.g. `Pac`, `TotWh`), record:
   - `net_address` (device SMA net address, often `0x0001`)
   - `ctype` (`smadata1.ctype`)
   - `cindex` (`smadata1.cindex`)
   - `ntype` (numeric format)
   - `gain`, `offset`
5. Copy values into `supla-device.yaml` or use a profile under `profiles/`.

See [profiles/README.md](profiles/README.md) for an example profile template.

## YAML channel type

Enable the extension when building:

```bash
cmake -B build -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/sma
```

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
