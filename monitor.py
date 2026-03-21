#!/usr/bin/env python3
"""Monitor Arduino serial output. Uses arduino-cli monitor with line buffering."""
import subprocess, time, sys, os

PORT = "/dev/cu.usbmodemE4B063AF29FC2"
BAUD = "115200"
WAIT = int(sys.argv[1]) if len(sys.argv) > 1 else 5
DURATION = int(sys.argv[2]) if len(sys.argv) > 2 else 30

# Kill any existing monitors
os.system("pkill -f 'arduino-cli monitor' 2>/dev/null")
time.sleep(WAIT)

proc = subprocess.Popen(
    ["arduino-cli", "monitor", "-p", PORT, "--config", f"baudrate={BAUD}"],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    bufsize=1, env={**os.environ, "PYTHONUNBUFFERED": "1"}
)

start = time.time()
try:
    while time.time() - start < DURATION:
        line = proc.stdout.readline()
        if line:
            sys.stdout.write(line)
            sys.stdout.flush()
        elif proc.poll() is not None:
            # Process ended
            err = proc.stderr.read()
            if err:
                print(f"STDERR: {err}", file=sys.stderr)
            break
finally:
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()
