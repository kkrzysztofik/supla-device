# SMA inverter profiles

Fixed channel metadata for known inverter families. Values must be confirmed
on your hardware with `yasdishell` — the examples below are placeholders.

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
