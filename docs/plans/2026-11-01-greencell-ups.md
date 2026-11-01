# Green Cell UPS → SUPLA Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Show the Green Cell PowerProof 1000VA UPS (USB on `supla-sma`,
`root@192.168.3.125`) in SUPLA: on-battery and low-battery alarms, battery %,
load %, input voltage and power, and a beeper relay.

**Architecture:** NUT (`nutdrv_qx`, standalone, localhost) talks to the UPS.
A separate sd4linux instance (`supla-device-ups.service`) runs
`upsc greencell@localhost | supla-ups-lines` through the existing `Cmd`
source. The awk filter turns `upsc` output into fixed numeric lines, and one
shared `Simple` parser feeds 6 channels. No C++. Design:
`docs/plans/2026-11-01-greencell-ups-design.md`.

**Tech Stack:** NUT 2.8.1 (Debian 13), POSIX awk, sd4linux YAML, systemd.

**Ground rules:**
- Commands marked **VM** run as `ssh root@192.168.3.125 '…'`. Everything else
  runs in the worktree `.worktrees/greencell-ups`.
- New files carry `Copyright (C) 2026 Krzysztof Krzysztofik` (AGENTS.md).
- Do NOT touch `supla-device.service` or `supla-device-ingecon.service` on
  the VM. Both are live (SMA inverter and Ingecon).

---

### Task 0: Fix the conflict markers already on `main` (separate commit)

`4da1c339` left `<<<<<<<`/`>>>>>>>` in `extras/test/CMakeLists.txt` and
`docs/index.md`, so the test suite fails to configure. Both sides are kept
(Socomec and Display HAT Mini are independent). This is already applied in the
worktree.

**Step 1:** Configure and build the tests:
`cd extras/test && cmake -S . -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build -j$(nproc)`
(The policy flag is needed because the bundled cJSON requires CMake < 3.5.)

**Step 2:** `cd build && ctest --output-on-failure -R sd4linux`. Expected: all pass.

**Step 3: Commit**
```bash
git add extras/test/CMakeLists.txt docs/index.md
git commit -m "fix: drop leftover conflict markers from the Socomec/HAT merge"
```

---

### Task 1: `supla-ups-lines` awk filter (TDD)

**Files:**
- Create: `extras/examples/linux/ups/supla-ups-lines`
- Test: `extras/examples/linux/ups/test-supla-ups-lines.sh`

**Step 1: Write the failing test**

```bash
#!/usr/bin/env bash
# Copyright (C) 2026 Krzysztof Krzysztofik
# Checks supla-ups-lines against canned `upsc` output. Run: ./test-supla-ups-lines.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
filter="$here/supla-ups-lines"
fail=0

check() {  # name, expected stdout, expected exit, input
  local out rc
  out="$(printf '%s' "$4" | "$filter")"; rc=$?
  if [[ "$out" != "$2" || "$rc" != "$3" ]]; then
    echo "FAIL $1: rc=$rc out=$(printf '%q' "$out")"; fail=1
  else
    echo "ok   $1"
  fi
}

check "on mains" $'0\n0\n100\n12\n231.5\n1' 0 \
'battery.charge: 100
battery.voltage: 27.50
input.voltage: 231.5
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
check "on battery, low" $'1\n1\n18\n40\n0.0\n0' 0 \
'battery.charge: 18
input.voltage: 0.0
ups.beeper.status: disabled
ups.load: 40
ups.status: OB DISCHRG LB
'
check "charging is not on battery" $'0\n0\n85\n12\n230.1\n1' 0 \
'battery.charge: 85
input.voltage: 230.1
ups.beeper.status: enabled
ups.load: 12
ups.status: OL CHRG
'
# upsd down / data stale: upsc prints nothing on stdout
check "no data" '' 1 ''
# never report "on mains" when status itself is missing
check "no status" '' 1 'battery.charge: 100
'
# a missing variable leaves its line blank (parser marks that channel invalid)
check "missing charge" $'0\n0\n\n12\n231.5\n1' 0 \
'input.voltage: 231.5
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
exit $fail
```

**Step 2: Run it to make sure it fails**

`chmod +x extras/examples/linux/ups/test-supla-ups-lines.sh && extras/examples/linux/ups/test-supla-ups-lines.sh`
Expected: every check FAILs (the filter doesn't exist yet).

**Step 3: Write the minimal implementation**

```awk
#!/usr/bin/awk -f
# Copyright (C) 2026 Krzysztof Krzysztofik
#
# Turns `upsc <ups>` output into one value per line for the sd4linux Simple
# parser (supla-device-ups.yaml uses these indexes):
#   0 on battery (ups.status has OB)  1 battery low (LB)  2 battery.charge
#   3 ups.load  4 input.voltage  5 beeper enabled
# No ups.status (upsd down, stale data) -> no output, exit 1, so SUPLA channels
# go invalid instead of showing "on mains, 0 %".
{
  i = index($0, ": ")
  if (i) v[substr($0, 1, i - 1)] = substr($0, i + 2)
}
END {
  if (v["ups.status"] == "") exit 1
  s = " " v["ups.status"] " "
  print (s ~ / OB /)
  print (s ~ / LB /)
  print v["battery.charge"]
  print v["ups.load"]
  print v["input.voltage"]
  print (v["ups.beeper.status"] == "enabled")
}
```

**Step 4: Run tests and make sure they pass**

`chmod +x extras/examples/linux/ups/supla-ups-lines && extras/examples/linux/ups/test-supla-ups-lines.sh`
Expected: 6 × `ok`, exit 0. Also run it under `mawk` if it's installed
(`awk` on Debian may be mawk): `ls -l /etc/alternatives/awk`.

**Step 5: Commit**
```bash
git add extras/examples/linux/ups/
git commit -m "feat(ups): awk filter mapping upsc output to Simple parser lines"
```

---

### Task 2: NUT on the VM

**Step 1 (VM):** `apt-get install -y nut-server nut-client`

**Step 2 (VM):** Write the configs. The password is generated on the VM and
never committed:
```bash
cat > /etc/nut/ups.conf <<'EOF'
[greencell]
  driver = nutdrv_qx
  port = auto
  vendorid = 0665
  productid = 5161
  subdriver = cypress
  desc = "Green Cell PowerProof 1000VA"
  # voltage->% estimate knobs, tune after a real discharge:
  # default.battery.voltage.low = 21.4
  # default.battery.voltage.high = 26.0
EOF
sed -i 's/^MODE=.*/MODE=standalone/' /etc/nut/nut.conf
echo 'LISTEN 127.0.0.1 3493' > /etc/nut/upsd.conf
pw=$(head -c 18 /dev/urandom | base64)
cat > /etc/nut/upsd.users <<EOF
[supla]
  password = $pw
  instcmds = beeper.enable beeper.disable beeper.toggle
EOF
chown root:nut /etc/nut/*.conf /etc/nut/upsd.users; chmod 640 /etc/nut/upsd.users
udevadm trigger --subsystem-match=usb --action=add
systemctl restart nut-driver-enumerator nut-server
```

**Step 3 (VM): Verify against the raw Q1 reading** (in about 231 V, load
about 12, battery about 27.5 V, `OL`):
`upsc greencell@localhost` and `upscmd -l greencell@localhost`

Note which beeper commands `upscmd -l` lists. If there's no
`beeper.enable`/`beeper.disable`, use the guarded-toggle variant in Task 3.
Also note `battery.charge`: if it's missing, the battery % channel will stay
invalid until `default.battery.voltage.*` is set (see Step 2 comments).

**Step 4 (VM):** `/dev/hidraw1` is now owned by NUT. Check that
`supla-device` and `supla-device-ingecon` are still `active`.

No commit (VM-only state). The config is documented in Task 5.

---

### Task 3: `supla-device-ups.yaml`

**Files:**
- Create: `extras/examples/linux/supla-device-ups.yaml`

```yaml
# Copyright (C) 2026 Krzysztof Krzysztofik
#
# Green Cell PowerProof 1000VA UPS via NUT -> sd4linux, as its own SUPLA device.
# Setup: docs/integrations/greencell-ups-supla.md
# Keep line indexes in sync with ups/supla-ups-lines.

name: "UPS Green Cell 1000VA"
log_level: info
state_files_path: "/var/lib/supla-device-ups"

supla:
  server: svr1.supla.org
  mail: user@example.com

channels:
  - type: BinaryParsed
    caption: "UPS on battery"
    state: 0
    source:
      name: ups_src
      type: Cmd
      command: "upsc greencell@localhost 2>/dev/null | /usr/local/bin/supla-ups-lines"
    parser:
      name: ups
      type: Simple
      refresh_time_ms: 5000

  - type: BinaryParsed
    caption: "UPS battery low"
    state: 1
    parser:
      use: ups

  - type: GeneralPurposeMeasurementParsed
    caption: "UPS battery"
    value: 2
    default_value_precision: 0
    default_unit_after_value: "%"
    parser:
      use: ups

  - type: GeneralPurposeMeasurementParsed
    caption: "UPS load"
    value: 3
    default_value_precision: 0
    default_unit_after_value: "%"
    parser:
      use: ups

  # power is estimated: load % x 7 W (700 W rated). Calibrate the multiplier
  # against a plug-in meter if it matters.
  - type: ElectricityMeterParsed
    caption: "UPS input"
    parser:
      use: ups
    phase_1:
      - voltage: 4
      - power_active: 3
        multiplier: 7

  - type: CmdRelay
    caption: "UPS beeper"
    cmd_on: "upscmd -u supla -p PASSWORD greencell@localhost beeper.enable"
    cmd_off: "upscmd -u supla -p PASSWORD greencell@localhost beeper.disable"
    state: 5
    parser:
      use: ups
```

If Task 2 found only `beeper.toggle`, use guarded commands instead:
`cmd_on: "[ \"$(upsc greencell@localhost ups.beeper.status 2>/dev/null)\" = enabled ] || upscmd -u supla -p PASSWORD greencell@localhost beeper.toggle"`
`cmd_off` is the same with `= disabled`.

**Step 1:** Check keys against `linux_yaml_config.cpp`
(`addBinaryParsed`, `addGeneralPurposeMeasurementParsed`:1662,
`addElectricityMeterParsed`:1900, `addCmdRelay`:1052) and confirm `caption` is
accepted for all four. Any unknown key is logged at startup, and Step 3
catches it.

**Step 2:** Copy the filter and a scratch config to the VM:
```bash
scp extras/examples/linux/ups/supla-ups-lines root@192.168.3.125:/usr/local/bin/
scp extras/examples/linux/supla-device-ups.yaml root@192.168.3.125:/tmp/ups-test.yaml
```
**VM:** `chmod 755 /usr/local/bin/supla-ups-lines; upsc greencell@localhost 2>/dev/null | supla-ups-lines`
Expected: 6 lines, e.g. `0 0 100 12 231.5 1`.

**Step 3 (VM): Dry run** without registering. Point it at a scratch state dir
and an unresolvable server:
```bash
sed -i 's#/var/lib/supla-device-ups#/tmp/ups-state#; s#svr1.supla.org#invalid.invalid#' /tmp/ups-test.yaml
sudo -u supla-device timeout 15 supla-device-linux -D -c /tmp/ups-test.yaml 2>&1 | grep -iE "channel|error|unknown|parser" | head -40
rm -rf /tmp/ups-test.yaml /tmp/ups-state
```
Expected: 6 × `Channel[n] config: adding …`, no `error`/`unknown parameter`,
and parsed values in debug output.

**Step 4: Commit**
```bash
git add extras/examples/linux/supla-device-ups.yaml
git commit -m "feat(ups): sd4linux example config for Green Cell UPS via NUT"
```

---

### Task 4: Deploy on the VM

**Step 1 (VM):**
```bash
pw=$(awk '/password/{print $3}' /etc/nut/upsd.users)
install -m 0640 -o root -g supla-device /dev/null /etc/supla-device-ups.yaml
# copy the yaml content, set real supla.server/mail (svr45.supla.org, same as
# /etc/supla-device.yaml), replace PASSWORD with $pw
sed 's#/etc/supla-device-ingecon.yaml#/etc/supla-device-ups.yaml#;
     s#/var/lib/supla-device-ingecon#/var/lib/supla-device-ups#;
     s#^Description=.*#Description=SUPLA Device Linux (UPS)#' \
  /usr/lib/systemd/system/supla-device-ingecon.service \
  > /etc/systemd/system/supla-device-ups.service
install -d -o supla-device -g supla-device /var/lib/supla-device-ups
systemctl daemon-reload && systemctl enable --now supla-device-ups
journalctl -u supla-device-ups -n 50 --no-pager
```

**Step 2 (user):** Enable "add new devices" in SUPLA Cloud, then restart the
service. Expected: the device "UPS Green Cell 1000VA" registers with 6
channels.

Note: `upscmd -p` shows the password in `ps` on this VM. That's acceptable
for a single-purpose VM, since the user can only toggle the beeper.

---

### Task 5: Docs

**Files:**
- Create: `docs/integrations/greencell-ups-supla.md` (hardware IDs, Q1 probe
  results, the Task 2 NUT config, the Task 4 deploy steps, calibration knobs,
  and troubleshooting: channels invalid → `upsc greencell@localhost`)
- Modify: `docs/index.md` (one bullet under integrations, next to Socomec)

**Commit:** `docs(ups): Green Cell UPS setup guide`

---

### Task 6: Live verification (needs the user)

1. Values in SUPLA match `upsc greencell@localhost`.
2. Unplug the UPS from the mains for about 10 s: "UPS on battery" goes ON
   within about 5 s, input voltage drops, and it returns to OFF after
   replugging.
3. Toggle "UPS beeper": `upsc greencell@localhost ups.beeper.status` follows.
4. `systemctl stop nut-server` → channels go invalid/offline within about 5 s,
   then `systemctl start nut-server`.
5. Low battery: not tested (deep discharge).
