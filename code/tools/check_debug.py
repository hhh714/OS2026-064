#!/usr/bin/env python3
"""Exercise the real make debug/gdb entry points and the live demo commands."""

import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time


def port_available():
    with socket.socket() as probe:
        try:
            probe.bind(("127.0.0.1", 1234))
            return True
        except OSError:
            return False


def main():
    root = Path(__file__).resolve().parent.parent
    if not port_available():
        print("Port 1234 is in use. Exit your existing QEMU/GDB session first.")
        return 1
    result = subprocess.run(["make", "--no-print-directory", "debug-build"], cwd=root)
    if result.returncode:
        return result.returncode
    logs = root / "obj/debug"
    gdb_log = logs / "demo-gdb.log"
    qemu_log = logs / "demo-qemu.log"
    # A separate process group lets cleanup stop only the QEMU we started.
    with qemu_log.open("w", encoding="utf-8") as console:
        qemu = subprocess.Popen(
            ["make", "--no-print-directory", "debug"], cwd=root,
            stdout=console, stderr=subprocess.STDOUT, start_new_session=True,
        )
        try:
            deadline = time.monotonic() + 10
            while port_available():
                if qemu.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("make debug did not open its GDB port")
                time.sleep(0.05)
            result = subprocess.run(
                ["make", "--no-print-directory", "gdb",
                 "GDBFLAGS=-batch -x tools/verify_debug.gdb"],
                cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, timeout=30,
            )
            gdb_log.write_text(result.stdout, encoding="utf-8")
            markers = ["[Debug pages] PASS", "[Debug processes] PASS",
                       "[Debug switch] PASS", "[Debug entry] PASS"]
            if result.returncode or not all(x in result.stdout for x in markers):
                print(result.stdout)
                raise RuntimeError("GDB demonstration check failed")
            for marker in markers:
                print(marker)
            print("GDB log: " + str(gdb_log.relative_to(root)))
            print("QEMU log: " + str(qemu_log.relative_to(root)))
            return 0
        except (RuntimeError, subprocess.TimeoutExpired) as error:
            print(error)
            return 1
        finally:
            if qemu.poll() is None:
                os.killpg(qemu.pid, signal.SIGTERM)
            try:
                qemu.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(qemu.pid, signal.SIGKILL)
                qemu.wait()


if __name__ == "__main__":
    sys.exit(main())
