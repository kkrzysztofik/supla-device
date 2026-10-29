#!/usr/bin/env python3
"""Physical-layer diagnostics for FT232 based RS-485 adapters.

Checks:
  * modem control line states (CTS/DSR/CD/RI) with RTS/DTR toggled
  * whether a frame sent on one adapter is seen by the other (shared bus?)
  * whether any bytes arrive at all after a request (echo / noise / replies)

Usage: wiring_check.py [portA] [portB]
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


def modem_lines(ser):
    return {
        "CTS": ser.cts, "DSR": ser.dsr, "CD": ser.cd, "RI": ser.ri,
    }


def main(argv):
    port_a = argv[1] if len(argv) > 1 else "/dev/ttyUSB0"
    port_b = argv[2] if len(argv) > 2 else "/dev/ttyUSB1"

    ser_a = serial.Serial(port_a, 9600, bytesize=8, parity="N", stopbits=1, timeout=0.3)
    ser_b = serial.Serial(port_b, 9600, bytesize=8, parity="N", stopbits=1, timeout=0.3)
    try:
        for name, ser in (("A", ser_a), ("B", ser_b)):
            print(f"--- {name} {ser.name} ---")
            for rts, dtr in ((False, False), (True, False), (False, True), (True, True)):
                ser.rts = rts
                ser.dtr = dtr
                time.sleep(0.05)
                print(f"  rts={int(rts)} dtr={int(dtr)} -> {modem_lines(ser)}")
            ser.rts = False
            ser.dtr = False

        print("--- cross-bus test (9600 8N1, frame to slave 1) ---")
        for src_name, src, dst_name, dst in (
            ("A", ser_a, "B", ser_b), ("B", ser_b, "A", ser_a),
        ):
            src.reset_input_buffer()
            dst.reset_input_buffer()
            src.write(frame_of(1, 3, 0, 2))
            src.flush()
            time.sleep(0.35)
            seen = dst.read(64)
            own = src.read(64)
            print(f"  sent on {src_name}: dst {dst_name} saw {len(seen)} bytes {seen.hex()}; "
                  f"src echo {len(own)} bytes {own.hex()}")

        print("--- idle noise listen 2s per port ---")
        for name, ser in (("A", ser_a), ("B", ser_b)):
            ser.timeout = 0.2
            ser.reset_input_buffer()
            data = b""
            end = time.time() + 2.0
            while time.time() < end:
                chunk = ser.read(64)
                if chunk:
                    data += chunk
            print(f"  {name}: {len(data)} bytes {data[:64].hex()}")
    finally:
        ser_a.close()
        ser_b.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
