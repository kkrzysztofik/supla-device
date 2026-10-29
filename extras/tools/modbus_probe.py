#!/usr/bin/env python3
"""Modbus RTU discovery/read helper for Socomec COUNTIS RS-485 meters.

Important: an *exception* reply proves the slave is present, so it counts as a
discovery hit (the earlier version wrongly discarded it).

Usage:
  modbus_probe.py scan  <port>            # sweep bauds/parities/addresses, stop at first hit
  modbus_probe.py id    <port> <baud> <N|E|O> <addr>   # read COUNTIS identity block
  modbus_probe.py read  <port> <baud> <N|E|O> <addr> <func> <start> <count>
"""
import sys

import serial

# COUNTIS identity block starts at 0xC350 (50000) according to the
# communication tables; reading there is a safe "are you there" request.
IDENTITY_ADDR = 0xC350

BAUDS = (9600, 19200, 38400, 4800, 2400, 57600, 115200, 1200)
PARITIES = ("N", "E", "O")


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def frame_of(addr: int, func: int, start: int, count: int) -> bytes:
    payload = bytes([addr, func]) + start.to_bytes(2, "big") + count.to_bytes(2, "big")
    return payload + crc16(payload).to_bytes(2, "little")


def open_port(port, baud, parity, timeout=0.15):
    ser = serial.Serial(port, baud, bytesize=8, parity=parity, stopbits=1,
                        timeout=timeout, xonxoff=False, rtscts=False)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


def transaction(ser, addr, func, start, count, timeout=0.15):
    """Return (status, detail, words). status: ok|exception|silent|badframe."""
    ser.timeout = timeout
    ser.reset_input_buffer()
    ser.write(frame_of(addr, func, start, count))
    ser.flush()
    exp = 5 + 2 * count
    resp = ser.read(exp)
    if not resp:
        return "silent", "no bytes", None
    if len(resp) < 5:
        return "badframe", f"short({len(resp)}):{resp.hex()}", None
    if resp[0] != addr:
        return "badframe", f"addr_mismatch:{resp.hex()}", None
    if crc16(resp[:-2]) != int.from_bytes(resp[-2:], "little"):
        return "badframe", f"crc:{resp.hex()}", None
    if resp[1] & 0x80:
        return "exception", f"exception_code={resp[2]}", None
    words = [int.from_bytes(resp[3 + 2 * i:5 + 2 * i], "big") for i in range(count)]
    return "ok", f"words={words}", words


def scan(port, max_addr=247):
    for parity in PARITIES:
        for baud in BAUDS:
            try:
                ser = open_port(port, baud, parity)
            except Exception as exc:  # noqa: BLE001
                print(f"{port} {baud} 8{parity}1 OPEN-FAIL {exc}", flush=True)
                continue
            hit = None
            try:
                for addr in range(1, max_addr + 1):
                    status, detail, _ = transaction(ser, addr, 3, IDENTITY_ADDR, 1, timeout=0.07)
                    if status in ("ok", "exception"):
                        hit = (addr, status, detail)
                        break
                    if status == "badframe":
                        # a corrupted reply still means *something* answered
                        print(f"  ! {port} {baud} 8{parity}1 addr={addr} badframe {detail}",
                              flush=True)
            finally:
                ser.close()
            if hit:
                print(f"HIT {port} {baud} 8{parity}1 addr={hit[0]} {hit[1]} {hit[2]}", flush=True)
                return 0
            print(f"    {port} {baud} 8{parity}1: silent 1..{max_addr}", flush=True)
    print(f"{port}: NO DEVICE RESPONDED (all bauds/parities/addresses 1..{max_addr})", flush=True)
    return 0


def read_block(ser, addr, start, count):
    words = []
    # read in chunks of up to 100 registers
    left = count
    cur = start
    while left > 0:
        chunk = min(left, 100)
        status, detail, w = transaction(ser, addr, 3, cur, chunk, timeout=0.5)
        print(f"  read {cur}(0x{cur:04X}) x{chunk}: {status} {detail}", flush=True)
        if status != "ok":
            return words
        words.extend(w)
        cur += chunk
        left -= chunk
    return words


def decode_string(words, nwords):
    out = b""
    for w in words[:nwords]:
        out += w.to_bytes(2, "big")
    return out.decode("latin-1", "replace").strip("\x00 ").strip()


def identity(port, baud, parity, addr):
    ser = open_port(port, baud, parity, timeout=0.5)
    try:
        print(f"--- identity block @ 0x{IDENTITY_ADDR:04X} ({IDENTITY_ADDR}) ---")
        words = read_block(ser, addr, IDENTITY_ADDR, 66)
        if not words:
            return 1
        print(f"  vendor tag      : {decode_string(words[0:4], 4)!r}")
        print(f"  product order id: {words[4] if len(words) > 4 else '?'}")
        print(f"  product id      : {words[5] if len(words) > 5 else '?'}")
        print(f"  table version   : {words[6] if len(words) > 6 else '?'}")
        print(f"  sw version      : {words[7] if len(words) > 7 else '?'}")
        if len(words) >= 46:
            print(f"  vendor name     : {decode_string(words[42:50], 8)!r}")
            print(f"  product name    : {decode_string(words[50:58], 8)!r}")
            print(f"  extended name   : {decode_string(words[58:66], 8)!r}")
        return 0
    finally:
        ser.close()


def main(argv):
    if len(argv) >= 3 and argv[1] == "scan":
        return scan(argv[2], int(argv[3]) if len(argv) > 3 else 247)
    parity = {"N": serial.PARITY_NONE, "E": serial.PARITY_EVEN,
              "O": serial.PARITY_ODD}
    if len(argv) == 6 and argv[1] == "id":
        return identity(argv[2], int(argv[3]), parity[argv[4].upper()], int(argv[5]))
    if len(argv) == 9 and argv[1] == "read":
        ser = open_port(argv[2], int(argv[3]), parity[argv[4].upper()], timeout=0.5)
        try:
            status, detail, words = transaction(ser, int(argv[5]), int(argv[6]),
                                                int(argv[7]), int(argv[8]), timeout=0.5)
            print(f"{status} {detail} words={words}")
        finally:
            ser.close()
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
