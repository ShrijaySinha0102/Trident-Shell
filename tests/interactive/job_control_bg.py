#!/usr/bin/env python3
"""Pty test: Ctrl+Z, bg, jobs, fg, Ctrl+C for background and foreground job control."""
import os, pty, select, subprocess, sys, time

def start():
    pid, fd = pty.fork()
    if pid == 0:
        os.execv("./nsh", ["./nsh"])
    return pid, fd

def read_until(fd, text, timeout=5.0):
    buf, end = b"", time.time() + timeout
    while time.time() < end:
        if select.select([fd], [], [], 0.2)[0]:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            buf += data
            if text.encode() in buf:
                return True, buf.decode(errors="replace")
    return False, buf.decode(errors="replace")

def send(fd, s):
    os.write(fd, s.encode())

def job_state(marker):
    out = subprocess.run(["pgrep", "-f", marker], capture_output=True, text=True).stdout.split()
    if not out:
        return None
    pid = out[0]
    ps = subprocess.run(["ps", "-o", "stat=", "-p", pid], capture_output=True, text=True).stdout.strip()
    return ps[0] if ps else None

def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  -> " + detail))
    return ok

def main():
    results = []
    pid, fd = start()
    ok, out = read_until(fd, "nsh>", timeout=3.0)
    time.sleep(0.3)

    # 1. Start sleep 305, stop with Ctrl+Z
    send(fd, "sleep 305\n")
    time.sleep(0.5)
    send(fd, "\x1a")  # Ctrl+Z
    time.sleep(0.8)
    st = job_state("sleep 305")
    results.append(check("Ctrl+Z stopped job", st == "T", str(st)))

    # 2. bg resumes job in background
    send(fd, "bg\n")
    time.sleep(0.8)
    st = job_state("sleep 305")
    results.append(check("bg resumed job", st is not None and st != "T", str(st)))

    # 3. jobs lists running job
    send(fd, "jobs\n")
    time.sleep(0.5)
    ok, out = read_until(fd, "sleep 305", timeout=2.0)
    results.append(check("jobs listed the job", ok, out[-200:]))

    # 4. fg brings job to foreground, Ctrl+C terminates it
    send(fd, "fg\n")
    time.sleep(0.5)
    send(fd, "\x03")  # Ctrl+C
    time.sleep(0.5)
    st = job_state("sleep 305")
    results.append(check("fg + Ctrl+C killed the job", st is None, str(st)))

    send(fd, "echo al'iv'e_bg\n")
    ok, out = read_until(fd, "alive_bg", timeout=3.0)
    results.append(check("shell alive after job control", ok, out[-200:]))

    send(fd, "exit\n")
    time.sleep(0.3)
    try:
        os.kill(pid, 9)
    except OSError:
        pass

    passed = results.count(True)
    failed = results.count(False)
    print("passed=%d failed=%d" % (passed, failed))
    sys.exit(0 if failed == 0 else 1)

if __name__ == "__main__":
    main()
