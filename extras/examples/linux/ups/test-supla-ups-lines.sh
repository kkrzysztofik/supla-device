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

# all variables the YAML uses; tests drop or change one at a time
full='battery.charge: 100
input.voltage: 231.5
output.current.nominal: 4.0
output.frequency: 50.0
output.voltage: 228.0
ups.beeper.status: enabled
ups.load: 12
ups.status: OL
'
without() { printf '%s' "$full" | grep -v "^$1: "; }

check "on mains" $'0\n0\n100\n12\n231.5\n1\n50.0\n0.48\n109.4\n' 0 "$full"
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
check "charging is not on battery" $'0\n0\n100\n12\n231.5\n1\n50.0\n0.48\n109.4\n' 0 \
"$(without ups.status)
ups.status: OL CHRG
"
# upsd down / data stale: upsc prints nothing on stdout
check "no data" '' 1 ''
# A blank line would invalidate the whole shared Simple parser and reach the
# meter as 0, so any missing value means no output at all.
for var in ups.status battery.charge ups.load input.voltage ups.beeper.status \
           output.frequency output.current.nominal output.voltage; do
  check "missing $var" '' 1 "$(without "$var")
"
done
# beeper state other than enabled/disabled is unknown, not "disabled"
check "beeper muted" '' 1 "$(without ups.beeper.status)
ups.beeper.status: muted
"
# a non-numeric value (e.g. N/A) would reach awk arithmetic as 0 and the
# shared parser as a string, so it counts as missing
for var in battery.charge ups.load input.voltage output.frequency \
           output.current.nominal output.voltage; do
  check "non-numeric $var" '' 1 "$(without "$var")
$var: N/A
"
done
exit $fail
