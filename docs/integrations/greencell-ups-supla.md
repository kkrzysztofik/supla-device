# Green Cell PowerProof 1000VA UPS — SUPLA setup (NUT + sd4linux)

NUT talks to the UPS. sd4linux's `Cmd` source and `Simple` parser publish the
values as a separate SUPLA device. There is no C++ code involved:

* `extras/examples/linux/ups/supla-ups-lines` is an awk filter that turns `upsc`
  output into one value per line. Its test is `test-supla-ups-lines.sh` in the
  same directory.
* `extras/examples/linux/supla-device-ups.yaml` is the sd4linux config.

Design notes: [design](../plans/2026-11-01-greencell-ups-design.md),
[implementation plan](../plans/2026-11-01-greencell-ups.md).

## Hardware

* USB `0665:5161` "INNO TECH USB to Serial" (Cypress HID), Megatec/Q1 protocol.
* Raw probe: `Q1` → `(231.5 174.7 226.7 012 50.0 27.5 --.- 00001001`,
  `I` → `1000VA`, `F` → `230.0 004 24.00 50.0` (24 V pack, 2×12 V).
* The UPS does not report a battery percentage or runtime. NUT estimates the
  charge from the battery voltage (see [Calibration](#calibration)).

## What gets published

| Line | Source | SUPLA channel |
| --- | --- | --- |
| 0 | `ups.status` contains `OB` | `BinaryParsed` "UPS on battery" |
| 1 | `ups.status` contains `LB` | `BinaryParsed` "UPS battery low" |
| 2 | `battery.charge` | `GeneralPurposeMeasurementParsed` "UPS battery" % |
| 3 | `ups.load` | `GeneralPurposeMeasurementParsed` "UPS load" % |
| 4, 3 | `input.voltage`, `ups.load` × 7 | `ElectricityMeterParsed` "UPS input": voltage, estimated power (700 W at 100 %) |
| 5 | `ups.beeper.status == enabled` | `CmdRelay` "UPS beeper" |

One `upsc` call every 5 s feeds all channels. Not published: `upsmon`
auto-shutdown, runtime (needs `runtimecal`), and output voltage/frequency
(`ElectricityMeterParsed` has no frequency key).

## 1. NUT (Debian 13, NUT 2.8.1)

```bash
apt-get install -y nut-server nut-client
cat > /etc/nut/ups.conf <<'EOF'
[greencell]
  driver = nutdrv_qx
  port = auto
  vendorid = 0665
  productid = 5161
  subdriver = cypress
  desc = "Green Cell PowerProof 1000VA"
  # voltage->% estimate, tune after a real discharge (NUT default 20.8 / 26.0):
  # default.battery.voltage.low = 20.8
  # default.battery.voltage.high = 26.0
EOF
sed -i 's/^MODE=.*/MODE=standalone/' /etc/nut/nut.conf
echo 'LISTEN 127.0.0.1 3493' > /etc/nut/upsd.conf
pw=$(head -c 18 /dev/urandom | base64 | tr -dc 'A-Za-z0-9')
cat > /etc/nut/upsd.users <<EOF
[supla]
  password = $pw
  instcmds = beeper.toggle
EOF
chown root:nut /etc/nut/*.conf /etc/nut/upsd.users; chmod 640 /etc/nut/upsd.users
udevadm trigger --action=add --attr-match=idVendor=0665   # only the UPS
systemctl restart nut-driver-enumerator nut-server
```

The password must stay alphanumeric because the relay commands use it unquoted
in `sh`. It is also visible in `ps`. That is acceptable on a single-purpose VM,
since the `supla` user can only toggle the beeper.

Check the result:

```bash
upsc greencell@localhost          # input.voltage ~231, ups.load ~12, battery.voltage ~27.5, ups.status OL
upscmd -l greencell@localhost     # beeper.toggle, load.on/off, shutdown.*, test.battery.start.quick
```

`nutdrv_qx` auto-selects the "Voltronic-QS 0.09" protocol, and its values match
the raw `Q1` reading. This UPS has no `beeper.enable`/`beeper.disable`, only
`beeper.toggle`. `upsc` prints "Init SSL without certificate database" on
stderr. That is harmless, and the config discards it with `2>/dev/null`.

## 2. Filter and SUPLA device

Run these commands from a repository checkout. The UPS runs as its own SUPLA
device next to the existing ones, with its own config, state directory and unit:

```bash
install -m 0755 extras/examples/linux/ups/supla-ups-lines /usr/local/bin/
upsc greencell@localhost 2>/dev/null | supla-ups-lines   # 6 lines, e.g. 0 0 100 12 231.5 1

pw=$(awk '/password/{print $3}' /etc/nut/upsd.users)
install -m 0640 -o root -g supla-device /dev/null /etc/supla-device-ups.yaml
sed "s/PASSWORD/$pw/g" extras/examples/linux/supla-device-ups.yaml > /etc/supla-device-ups.yaml
# now set supla.server and supla.mail in /etc/supla-device-ups.yaml

sed 's#/etc/supla-device-ingecon.yaml#/etc/supla-device-ups.yaml#;
     s#/var/lib/supla-device-ingecon#/var/lib/supla-device-ups#;
     s#^Description=.*#Description=SUPLA Device Linux (UPS)#;
     s#^SyslogIdentifier=.*#SyslogIdentifier=supla-device-ups#' \
  /usr/lib/systemd/system/supla-device-ingecon.service \
  > /etc/systemd/system/supla-device-ups.service
install -d -o supla-device -g supla-device /var/lib/supla-device-ups
```

Before the first start, enable new-device registration in SUPLA Cloud.
Otherwise the log shows `Registration disabled!` and you have to restart the
service after enabling it.

```bash
systemctl daemon-reload && systemctl enable --now supla-device-ups
journalctl -u supla-device-ups -n 50 --no-pager
```

The device "UPS Green Cell 1000VA" should register with 6 channels.

## Beeper relay behaviour

sd4linux relays apply `initial_state` at every start. On Linux the init path
calls it twice. With the default `off`, a start would run `cmd_off` and silence
the alarm. The UPS only has `beeper.toggle`, so the config handles this in
three ways:

* `initial_state: on`, so a restart never mutes the alarm.
* A fail-closed guard. The commands toggle only when `ups.beeper.status` is
  known to be the opposite state. If the state is unknown, they do nothing.
* `sleep 3` after a toggle. NUT re-polls about every 2 s, and without the wait
  the second init call would toggle the beeper back.

As a result, muting the beeper in SUPLA lasts only until the next service
restart. That is intended for an alarm device. The relay state in SUPLA can
lag by up to one refresh (5 s).

## Calibration

* **Battery %** is NUT's voltage-based estimate (`battery.charge`), with
  defaults of low 20.8 V and high 26.0 V. On float charge (27.5 V) it reads
  100. After a real discharge, set `default.battery.voltage.low/high` in
  `ups.conf` and restart `nut-driver@greencell`.
* **Power** is `ups.load` × 7 W. Adjust `multiplier` in the yaml against a
  plug-in meter if accuracy matters.

## Error handling and troubleshooting

If `upsd` is down or its data is stale, `upsc` prints nothing. The filter then
emits no lines and exits 1, the `Simple` parser treats the result as invalid,
and the channels go invalid instead of showing "on mains, 0 %". A single
missing variable leaves only that channel invalid.

| Symptom | Check |
| --- | --- |
| All channels invalid | `upsc greencell@localhost`, then `upsc greencell@localhost \| supla-ups-lines` |
| `upsc` fails / no data | `journalctl -u nut-driver@greencell`, `systemctl status nut-server` |
| Device not registering, config errors | `journalctl -u supla-device-ups` (look for `Registration disabled!`) |
| Beeper relay does nothing | `upsc greencell@localhost ups.beeper.status`; test `upscmd -u supla -p … greencell@localhost beeper.toggle` by hand |
| Filter changed | `extras/examples/linux/ups/test-supla-ups-lines.sh` |

`nut-monitor` is enabled but inactive because `upsmon` is not configured. Add
`upsmon` if the host should shut down on low battery.
