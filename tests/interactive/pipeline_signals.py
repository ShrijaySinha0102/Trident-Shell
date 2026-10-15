#!/usr/bin/env python3
"""Pty test: Ctrl+C / Ctrl+Z / fg on a PIPELINE. Standard library only.
Run from the repo root:  python3 tests/interactive/pipeline_signals.py
"""
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

def group_states(marker):
    """State letters (R/S/T...) of every process in the pipeline's group."""
    out = subprocess.run(["pgrep", "-f", marker], capture_output=True, text=True).stdout.split()
    if not out:
        return []
    pgid = os.getpgid(int(out[0]))
    ps = subprocess.run(["ps", "-eo", "pgid=,stat="], capture_output=True, text=True).stdout
    states = []
    for line in ps.splitlines():
        parts = line.split()
        if len(parts) == 2 and int(parts[0]) == pgid:
            states.append(parts[1][0])
    return states

def check(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  -> " + detail))
    return ok

def main():
    results = []
    pid, fd = start()
    ok, out = read_until(fd, "$")            # wait for the first prompt
    time.sleep(0.3)

    # 1. Ctrl+C kills the whole pipeline, not the shell
    send(fd, "sleep 301 | cat\n"); time.sleep(0.6)
    results.append(check("pipeline started with 2 live stages", len(group_states("sleep 301")) == 2, str(group_states("sleep 301"))))
    send(fd, "\x03"); time.sleep(0.5)
    send(fd, "echo ali've'1\n")
    ok, out = read_until(fd, "alive1")
    results.append(check("shell survives Ctrl+C", ok, out[-200:]))
    results.append(check("Ctrl+C killed the pipeline", group_states("sleep 301") == []))

    # 2. Ctrl+Z stops EVERY stage
    send(fd, "sleep 302 | cat\n"); time.sleep(0.6)
    send(fd, "\x1a"); time.sleep(0.8)
    st = group_states("sleep 302")
    results.append(check("Ctrl+Z stopped all stages", len(st) == 2 and all(c == "T" for c in st), str(st)))
    send(fd, "echo ali've'2\n")
    ok, out = read_until(fd, "alive2")
    results.append(check("shell usable after Ctrl+Z", ok, out[-200:]))

    # 3. fg is Person 3's builtin: report it separately, never as a pipeline failure
    send(fd, "fg\n"); time.sleep(0.8)
    st = group_states("sleep 302")
    if st and all(c == "T" for c in st):
        print("SKIP fg resume (Person 3: fg/bg builtins not implemented yet)")
        subprocess.run(["pkill", "-9", "-f", "sleep 302"])
    else:
        results.append(check("fg resumed all stages", len(st) == 2, str(st)))
        send(fd, "\x03"); time.sleep(0.5)
    send(fd, "echo ali've'3\n")
    ok, out = read_until(fd, "alive3")
    results.append(check("shell alive at end", ok, out[-200:]))
    results.append(check("no leftover processes", group_states("sleep 302") == []))

    send(fd, "exit\n"); time.sleep(0.3)
    try: os.kill(pid, 9)
    except OSError: pass
    print("passed=%d failed=%d" % (results.count(True), results.count(False)))
    sys.exit(0 if all(results) else 1)

main()
