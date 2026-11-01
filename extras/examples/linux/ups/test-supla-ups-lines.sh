#!/usr/bin/env bash
# Copyright (C) 2026 Krzysztof Krzysztofik
# Checks supla-ups-lines against canned `upsc` output. Run: ./test-supla-ups-lines.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
filter="$here/supla-ups-lines"
fail=0

check() {  # name, expected stdout (every line incl. trailing blanks), expected exit, input
  local out
  # rc is appended inside $(...) so trailing blank lines survive
  out="$(printf '%s' "$4" | "$filter"; echo "rc=$?")"
  if [[ "$out" != "$2rc=$3" ]]; then
    echo "FAIL $1: $(printf '%q' "$out")"; fail=1
  else
    echo "ok   $1"
  fi
}

check "on mains" $'0\n0\n100\n12\n231.5\n1\n50.0\n0.48\n109.4\n' 0 \
'battery.charge: 100
battery.voltage: 27.50
input.voltage: 231.5
output.current.nominal: 4.0
output.frequency: 50.0
output.voltage: 228.0
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
check "on battery, low" $'1\n1\n18\n40\n0.0\n0\n50.0\n1.60\n368.0\n' 0 \
'battery.charge: 18
input.voltage: 0.0
output.current.nominal: 4.0
output.frequency: 50.0
output.voltage: 230.0
ups.beeper.status: disabled
ups.load: 40
ups.status: OB DISCHRG LB
'
check "charging is not on battery" $'0\n0\n85\n12\n230.1\n1\n\n\n\n' 0 \
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
check "missing charge" $'0\n0\n\n12\n231.5\n1\n\n\n\n' 0 \
'input.voltage: 231.5
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
# derived values stay blank when an input is missing, never 0
check "no rating: no current, no VA" $'0\n0\n100\n12\n231.5\n1\n50.0\n\n\n' 0 \
'battery.charge: 100
input.voltage: 231.5
output.frequency: 50.0
output.voltage: 228.0
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
check "no output voltage: current, no VA" $'0\n0\n100\n12\n231.5\n1\n50.0\n0.48\n\n' 0 \
'battery.charge: 100
input.voltage: 231.5
output.current.nominal: 4.0
output.frequency: 50.0
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
exit $fail
