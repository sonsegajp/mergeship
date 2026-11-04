"""Checks an Anchor world-switch run (tools/test_anchor.ps1): one connection for the whole run, whichever world.

    python tools/anchor_check.py <run directory>

Reads, from the run directory:
  mergeship-host.log   the host's NetService lines ("net: ...") and the engines' "ANCHOR ..." lines (both engines log
                       their Anchor state through the host at every suspend and resume)
  trace.log            SoH's harness trace (ANCHOR lines, GIVE_ITEM given in Hyrule)
  trace_2ship.log      2Ship's harness trace (ANCHOR lines, remote item received in Termina)
  anchor_server.log    tools/anchor_mock_server.py's log (CONNECT / HANDSHAKE / DISCONNECT)
and writes anchor_result.txt: PASS or FAIL with the reasons. Exit code 0 on PASS.
"""

import os
import re
import sys

STATE = re.compile(r"ANCHOR (\w+): engine (\w+), hosted (\d), active (\d), enabled (\d), connected (\d), "
                   r"connection (\d+), reconnects (\d+), clientId (\d+), players (\d+)")


def read(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read().splitlines()
    except OSError:
        return []


def main():
    run = sys.argv[1] if len(sys.argv) > 1 else "."
    host = read(os.path.join(run, "mergeship-host.log"))
    soh = read(os.path.join(run, "trace.log"))
    mm = read(os.path.join(run, "trace_2ship.log"))
    server = read(os.path.join(run, "anchor_server.log"))
    failures = []
    notes = []

    connected = [l for l in host if "net: Anchor connected to" in l]
    closed = [l for l in host if re.search(r"net: Anchor connection \d+ closed:", l)]
    # (switches before the first connect, e.g. at startup while an auto-enabled Anchor is still connecting, don't count)
    first = next((i for i, l in enumerate(host) if "net: Anchor connected to" in l), len(host))
    switches = [l for l in host[first:] if "net: active engine" in l]
    notes.append(f"host: {len(connected)} connect(s), {len(closed)} close(s), {len(switches)} engine switch(es)")
    if len(connected) != 1:
        failures.append(f"the host connected {len(connected)} times (expected once): " + " | ".join(connected))
    elif "(connection 1)" not in connected[0]:
        failures.append("the host's connection is not connection 1: " + connected[0])
    for line in closed:
        if "mergeship.exe is exiting" not in line:
            failures.append("the connection closed during the run: " + line)
    if not switches:
        failures.append("no world switch while Anchor was on (no 'net: active engine' line)")
    for line in switches:
        if "Anchor connection 1 up" not in line:
            failures.append("a switch found the connection not up, or another connection: " + line)

    states = []
    for source, lines in (("host", host), ("soh", soh), ("2ship", mm)):
        for line in lines:
            m = STATE.search(line)
            if m:
                states.append((source, m.groups(), line))
    engines_resumed = {g[1] for _, g, _ in states if g[0] == "resumed"}
    notes.append(f"ANCHOR state lines: {len(states)}; resumed engines: {sorted(engines_resumed)}")
    client_ids = {g[8] for _, g, _ in states if g[5] == "1"}
    if len(client_ids) > 1:
        failures.append(f"more than one client id while connected: {sorted(client_ids)}")
    if "0" in client_ids:
        failures.append("connected with client id 0 (no ALL_CLIENT_STATE received)")
    for _, g, line in states:
        why, engine, hosted, active, enabled, conn = g[0], g[1], g[2], g[3], g[4], g[5]
        if why in ("resumed", "suspended") and enabled == "1":
            if conn != "1" or g[6] != "1":
                failures.append(f"{engine} {why} without connection 1 up: {line}")
            if hosted != "1":
                failures.append(f"{engine} is not on the host's connection: {line}")
    for engine in ("soh", "2ship"):
        if engine not in engines_resumed:
            failures.append(f"no 'ANCHOR resumed' line from {engine} (did the player cross both ways?)")

    connects = [l for l in server if " CONNECT " in l]
    handshakes = [l for l in server if " HANDSHAKE " in l]
    disconnects = [l for l in server if " DISCONNECT " in l]
    notes.append(f"server: {len(connects)} connect(s), {len(handshakes)} handshake(s), {len(disconnects)} disconnect(s)")
    if server:
        if len(connects) != 1:
            failures.append(f"the server saw {len(connects)} connections (expected 1)")
        if len(handshakes) != 1:
            failures.append(f"the server saw {len(handshakes)} handshakes (expected 1)")
        if len(disconnects) > 1:
            failures.append(f"the server saw {len(disconnects)} disconnects")
    else:
        notes.append("no anchor_server.log (a real server?): server checks skipped")

    # The cross-world item (the mock bot's GIVE_ITEM, arriving while the player is in Termina): exactly once.
    termina = [l for l in mm if "ANCHOR remote item" in l]
    termina_received = [l for l in termina if "received in Termina" in l]
    hyrule_given = [l for l in soh if "GIVE_ITEM" in l and "given in Hyrule" in l]
    hyrule_skipped = [l for l in soh if "GIVE_ITEM" in l and "already given in Termina" in l]
    notes.append(f"item: {len(termina_received)} received in Termina, {len(hyrule_given)} given in Hyrule, "
                 f"{len(hyrule_skipped)} skipped in Hyrule, {len(termina) - len(termina_received)} left for Hyrule")
    if termina or hyrule_given or hyrule_skipped:
        if len(termina_received) + len(hyrule_given) != 1:
            failures.append("the teammate's item was applied " +
                            f"{len(termina_received) + len(hyrule_given)} times (expected exactly once)")
    else:
        notes.append("no cross-world item in this run")

    result = "PASS" if not failures else "FAIL"
    text = "\n".join([f"ANCHOR WORLD-SWITCH TEST: {result}"] + ["  " + n for n in notes] +
                     ["  FAIL: " + f for f in failures]) + "\n"
    with open(os.path.join(run, "anchor_result.txt"), "w", encoding="utf-8") as f:
        f.write(text)
    print(text, end="")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
