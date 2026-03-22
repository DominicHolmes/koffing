#!/usr/bin/env python3
"""Serial monitor with file logging for Arduino debugging.

Usage:
    uv run --with pyserial monitor.py              # monitor indefinitely, log to logs/
    uv run --with pyserial monitor.py --duration 60 # monitor for 60 seconds
    uv run --with pyserial monitor.py --port /dev/cu.usbmodemXXX  # explicit port
"""
import argparse
import glob
import os
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

LOG_DIR = Path(__file__).parent / "logs"
BAUD = 115200


def find_port():
    ports = glob.glob("/dev/cu.usbmodem*")
    if not ports:
        print("ERROR: No USB serial device found. Is the board plugged in?", file=sys.stderr)
        sys.exit(1)
    if len(ports) > 1:
        print(f"Multiple ports found: {ports} — using {ports[0]}", file=sys.stderr)
    return ports[0]


def main():
    parser = argparse.ArgumentParser(description="Serial monitor with file logging")
    parser.add_argument("--port", default=None, help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=BAUD)
    parser.add_argument("--duration", type=int, default=0, help="Seconds to run (0=indefinite)")
    parser.add_argument("--no-log", action="store_true", help="Skip file logging, stdout only")
    parser.add_argument("--reset", action="store_true", help="Toggle DTR to reset the board on connect")
    args = parser.parse_args()

    port = args.port or find_port()
    LOG_DIR.mkdir(exist_ok=True)

    logfile = None
    logpath = None
    if not args.no_log:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        logpath = LOG_DIR / f"serial_{ts}.log"
        logfile = open(logpath, "w", buffering=1)  # line-buffered
        print(f"Logging to: {logpath}")

    print(f"Connecting to {port} @ {args.baud}...")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"ERROR: {e}", file=sys.stderr)
        print("Is another serial monitor running? (kill with: pkill -f 'arduino-cli monitor')", file=sys.stderr)
        sys.exit(1)

    if args.reset:
        ser.dtr = False
        time.sleep(0.1)
        ser.dtr = True
        print("Board reset via DTR toggle")

    print(f"Connected. {'Ctrl+C to stop.' if args.duration == 0 else f'Running for {args.duration}s.'}\n")

    running = True

    def handle_sig(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, handle_sig)
    signal.signal(signal.SIGTERM, handle_sig)

    start = time.time()
    try:
        while running:
            if args.duration and time.time() - start > args.duration:
                break
            try:
                raw = ser.readline()
            except serial.SerialException:
                print("\n[Serial disconnected]")
                break
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            except Exception:
                continue
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            out = f"[{ts}] {line}"
            print(out, flush=True)
            if logfile:
                logfile.write(out + "\n")
    finally:
        ser.close()
        if logfile:
            logfile.close()
            print(f"\nLog saved: {logpath}")


if __name__ == "__main__":
    main()
