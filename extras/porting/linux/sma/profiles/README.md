# SMA inverter profiles

Channel metadata for SMAData bulk reads (`CMD_GET_DATA` mask `0x090f`). Many
devices (e.g. WR33-008) match YASDI behaviour: metadata is loaded from a
profile file, not from `CMD_GET_CINFO` on every startup.

## YASDI export (recommended)

After `yasdishell` detection (`e` command), YASDI writes:

```text
yasdi/build/devices/<DeviceType>.bin
```

Copy next to your config:

```bash
mkdir -p sma-profiles
cp yasdi/build/devices/WR33-008.bin sma-profiles/
```

YAML:

```yaml
device:
  net_address: 1
  profile: WR33-008
```

Or use an absolute path: `profile: /opt/sma-profiles/WR33-008.bin`.

Built-in `WR33-008` is available without a file; prefer a YASDI-exported
`.bin` on production systems.

## Profiling workflow

```text
yasdishell> devices
yasdishell> channels <device_handle>
yasdishell> get <channel_handle>
```

Use YASDI property queries (or shell output) for:

| Property | YAML field |
|----------|------------|
| Net address | `device.net_address` |
| Channel type mask | `sma_channels.<key>.ctype` |
| Channel index | `sma_channels.<key>.cindex` |
| Numeric type | `sma_channels.<key>.ntype` |
| Gain | `sma_channels.<key>.gain` |
| Offset | `sma_channels.<key>.offset` |

Common `ntype` low nibble (from `chandef.h`):

| Value | Format |
|-------|--------|
| `0x0000` | byte |
| `0x0001` | word |
| `0x0002` | dword |
| `0x0004` | float32 |

Common spot channel `ctype` pattern: `CH_SPOT | CH_IN | CH_ANALOG` = `0x0801`
(verify per device).

## Example: Sunny Boy (placeholder)

Copy `sunny_boy_example.yaml` and adjust after profiling. Do not use
unverified ctype/cindex on production systems.
