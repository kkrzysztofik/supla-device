# SMA branch refactor baseline

This checklist scopes refactors on the `sma-integration` branch only. It is
intended to keep the native sd4linux SMA/SMANet behavior stable while reducing
review risk in the new branch code.

## Scope

- sd4linux SMA extension registration and YAML parsing.
- Native SMA RS485/SMANet protocol code under `extras/porting/linux/sma/`.
- SMA Linux examples and profile documentation.
- SMA unit tests under `extras/test/SmaTests/`.

Out of scope:

- Runtime linking to YASDI.
- ESP/Arduino SMA support.
- Public YAML schema changes.
- Protocol behavior changes not backed by hardware traces.
- Dependency or toolchain upgrades.

## Refactor passes

| Pass | Current behavior | Structural improvement | Validation |
| ---- | ---------------- | ---------------------- | ---------- |
| Protocol core | `SmaDataClient` performs frame IO, detection, handshakes, retry/backoff, CINFO, and spot reads. | Extract private helpers for timeouts and response parsing without changing command order or timeouts. | `extras/test/SmaTests/*`, `extras/examples/linux/build-sma-ingecon.sh`. |
| Bus worker | `SmaBus` owns a shared poller and copies subscriber cache state between threads. | Keep public API stable while isolating retry delay, subscriber snapshots, cache writes, and mapped read logic. | SMA build, targeted tests, hardware poll log parity. |
| Channel wrappers | `SmaInverter`, `SmaDcMeter`, `SmaThermometer`, and `SmaMeasurement` repeat poll interval and mapping checks. | Share small header-only helpers for poll interval normalization, bus config, and mapping comparison. | Channel construction from both SMA YAML examples. |
| YAML extension | `sma_extension.cpp` parses config and performs ownership handoff for each channel type. | Consolidate ownership handoff and keep channel factory registration unchanged. | sd4linux build with `-DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/sma`. |
| Serial IO | `SmaSerialPort` manages termios, direction control, blocking waits, and full writes. | Extract writable-fd wait helper while preserving `EINTR`, timeout, and fatal-error handling. | SMA build; serial write timeout/error logs remain equivalent. |
| Docs parity | SMA behavior is documented in the integration docs, SMA README, and Linux examples. | Keep behavior claims tied to runnable commands and YAML examples. | Commands and YAML types referenced in docs match registered factories. |

## Required checks

```bash
cmake -S extras/test -B extras/test/build
cmake --build extras/test/build
ctest --test-dir extras/test/build -R "Sma|sd4linux|LinuxPort" --output-on-failure
extras/examples/linux/build-sma-ingecon.sh
```

Hardware parity, when an SMA inverter is available:

- `CMD_GET_NET_START` then `CMD_CFG_NETADR` must occur before spot reads.
- `CMD_GET_DATA` bulk spot parsing must keep per-channel fallback for missing
  values.
- Multiple SMA YAML channels on one serial port must share one `SmaBus` worker.
- SUPLA active power sign convention remains unchanged for inverter export.
