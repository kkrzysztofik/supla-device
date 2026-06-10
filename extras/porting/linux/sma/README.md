# Minimal SMA RS485/SMANet client (sd4linux)

Read-only SMAData client for Linux `supla-device`. Protocol behavior is
reimplemented in C++ and validated against YASDI 1.8.3 in `yasdi/` (reference
only — no runtime dependency on `libyasdi`).

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
