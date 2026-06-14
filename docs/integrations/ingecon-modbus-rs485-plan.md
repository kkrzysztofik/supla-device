# INGECON Modbus/RS485 sd4linux Client Plan

## Summary

- Add a Linux-only `ingecon` extension for sd4linux.
- v1 is read-only Modbus RTU over RS485.
- Channel types: `IngeconInverter` and `IngeconMeasurement`.
- Do not implement holding-register inverter commands in v1; the provided register markdown lacks command/register details beyond the index.

## Key Changes

- Add extension build path:
  - `extras/porting/linux/extensions/ingecon/supla_linux_extension.cmake`
  - register factory types `IngeconInverter` and `IngeconMeasurement`
  - build via `-DSUPLA_LINUX_EXTENSION_DIRS=extras/porting/linux/extensions/ingecon`

- Add INGECON RTU stack under `extras/porting/linux/ingecon/`:
  - POSIX serial port, 8N1 defaults, configurable baud/timeout/retries
  - Modbus RTU function `0x04` only
  - CRC16 validation, slave-id/function/byte-count validation
  - shared `IngeconBus` per `(serial.device, baud, modbus_address)`

- Poll register block:
  - request input registers `30001-30027` as protocol address `0`, count `27`
  - parse 32-bit values as high word then low word
  - cache readings; after 3 failed polls mark stale

- `IngeconInverter` mapping:
  - extends `Supla::Sensor::ElectricityMeter`
  - phase 2/3 unsupported
  - `Total Energy` maps to `rvr_act_energy` by default, configurable with `energy_mapping: reverse|forward`
  - `Pac` maps to `setPowerActive(0, -Pac * 100000)`
  - `Vac` maps to `setVoltage(0, Vac * 100)`
  - `Iac` maps to `setCurrent(0, Iac * 1000)`
  - `Fac` maps to `setFreq(Fac)`
  - `Cos(M)` maps to `setPowerFactor(0, CosM)`
  - stale poll: zero instantaneous values, keep lifetime energy

- `IngeconMeasurement` mapping:
  - extends `GeneralPurposeMeasurement`
  - YAML key `ingecon_value`
  - supported keys: `vdc`, `idc`, `vbus`, `vac`, `iac`, `pac`, `fac`, `cos_phi`, `sin_sign`, `hours_running`, `grid_connections`, `status1`, `status2`, `alarms`, `year`, `month`, `day`, `hour`, `minute`, `second`, `display_fw_word_1..10`
  - stale poll: return `NAN` after 3 stale reads

- YAML interface:

```yaml
channels:
  - type: IngeconInverter
    caption: INGECON AC
    serial: &ingecon_serial
      device: /dev/ttyUSB0
      baud: 9600
    device: &ingecon_device
      modbus_address: 1
    poll_interval_sec: 15
    energy_mapping: reverse

  - type: IngeconMeasurement
    caption: INGECON alarms
    serial: *ingecon_serial
    device: *ingecon_device
    poll_interval_sec: 15
    ingecon_value: alarms
    default_unit_after_value: ""
    default_value_precision: 0
```

- Docs/examples:
  - add `extras/examples/linux/supla-device-ingecon.yaml`
  - add README supported-type entry
  - use the combined Linux helper: `build-sma-ingecon.sh`

## Test Plan

- Unit test Modbus RTU frame builder:
  - request slave `1`, function `0x04`, address `0`, count `27`
  - CRC appended little-endian
- Unit test response parser using markdown sample:
  - byte count `0x36`
  - `Vdc=317`, `Vbus=317`, `Cos=1000`, `Year=2013`
  - reject bad CRC, wrong slave, wrong function, wrong byte count, short frame
- Unit test `IngeconBus` cache:
  - shared bus reused for same serial config
  - separate bus for different `modbus_address`
  - stale state after 3 failed polls
- Channel tests:
  - `IngeconInverter` applies negative `Pac`
  - energy maps reverse by default and forward when configured
  - `IngeconMeasurement` returns configured key and `NAN` when stale
- Build checks:
  - `sd4linuxtests`
  - sd4linux CMake configure with INGECON extension enabled

## Assumptions

- Target is sd4linux only for v1.
- RS485 adapter is exposed as a POSIX serial device.
- Default serial config is `9600 8N1`, slave address `1`, poll interval `15s`.
- Commands under holding register `41000` stay out of scope until a complete command table is available.
