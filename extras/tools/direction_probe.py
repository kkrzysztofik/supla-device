#!/usr/bin/env python3
"""Probe RS-485 adapters that may need manual direction control.

Some cheap FT232 + RS-485 builds tie the transceiver DE/RE pin to RTS or DTR
instead of using auto-direction. This tries each driving mode while scanning a
few slave addresses and baud rates.

Usage: direction_probe.py <port> [max_addr]
"""
import sys
import time

import serial


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


def attempt(ser, addr, func, start, count, timeout=0.15):
    ser.timeout = timeout
    ser.reset_input_buffer()
    ser.write(frame_of(addr, func, start, count))
    ser.flush()
    exp = 5 + 2 * count
    resp = ser.read(exp)
    if not resp:
        return None
    if len(resp) < 5:
        return f"short:{resp.hex()}"
    if resp[0] != addr:
        return f"garbage:{resp.hex()}"
    if resp[1] & 0x80:
        return f"exception:{resp[2]}"
    if crc16(resp[:-2]) != int.from_bytes(resp[-2:], "little"):
        return f"crc:{resp.hex()}"
    words = [int.from_bytes(resp[3 + 2 * i:5 + 2 * i], "big") for i in range(count)]
    return f"OK words={words}"


def run_mode(port, baud, parity, mode, max_addr):
    ser = serial.Serial(port, baud, bytesize=8, parity=parity, stopbits=1,
                        timeout=0.15, xonxoff=False, rtscts=False)
    hits = []
    try:
        for addr in range(1, max_addr + 1):
            for func in (3, 4):
                # set driver-enable polarity for this mode
                if mode == "rts_high":
                    ser.rts = True
                elif mode == "rts_low":
                    ser.rts = False
                elif mode == "dtr_high":
                    ser.dtr = True
                elif mode == "dtr_low":
                    ser.dtr = False
                ser.reset_input_buffer()
                result = attempt(ser, addr, func, 0, 2)
                if mode in ("rts_high", "dtr_high"):
                    # release the driver so the transceiver can receive
                    ser.rts = False
                    ser.dtr = False
                    time.sleep(0.02)
                if result:
                    hits.append((addr, func, result))
        return hits
    finally:
        ser.close()


def main(argv):
    port = argv[1]
    max_addr = int(argv[2]) if len(argv) > 2 else 10
    results = []
    for baud, parity in ((9600, "N"), (19200, "N"), (38400, "N"), (9600, "E")):
        for mode in ("auto", "rts_high", "rts_low", "dtr_high", "dtr_low"):
            try:
                hits = run_mode(port, baud, parity, mode, max_addr)
            except Exception as exc:  # noqa: BLE001
                print(f"{port} {baud}{parity}1 {mode}: ERROR {exc}", flush=True)
                continue
            for addr, func, res in hits:
                line = f"{port} {baud} 8{parity}1 mode={mode} addr={addr} func={func} -> {res}"
                print(line, flush=True)
                results.append(line)
            print(f"{port} {baud} 8{parity}1 mode={mode}: {len(hits)} hits", flush=True)
    if not results:
        print(f"{port}: NO RESPONSES in any mode/baud/address tested", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
