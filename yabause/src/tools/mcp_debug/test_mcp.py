"""Smoke test for the in-process MCP debug server.

Prerequisite: yabasanshiro.exe running with the MCP debug server enabled
(default port 9640). Run: python test_mcp.py [port]
"""
import json
import sys
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9640
URL = f"http://127.0.0.1:{PORT}/mcp"
_id = 0


def rpc(method, params=None):
    global _id
    _id += 1
    body = {"jsonrpc": "2.0", "id": _id, "method": method}
    if params is not None:
        body["params"] = params
    req = urllib.request.Request(
        URL, json.dumps(body).encode(),
        headers={"Content-Type": "application/json",
                 "Accept": "application/json, text/event-stream"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def notify(method):
    body = {"jsonrpc": "2.0", "method": method}
    req = urllib.request.Request(
        URL, json.dumps(body).encode(),
        headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=10)


def call_tool(name, args=None):
    r = rpc("tools/call", {"name": name, "arguments": args or {}})
    assert "result" in r, f"tool {name} returned error: {r}"
    return r["result"]


def test_handshake():
    r = rpc("initialize", {"protocolVersion": "2025-03-26",
                           "capabilities": {},
                           "clientInfo": {"name": "test", "version": "0"}})
    assert r["result"]["serverInfo"]["name"] == "yabasanshiro-debug"
    notify("notifications/initialized")
    tools = rpc("tools/list")["result"]["tools"]
    names = {t["name"] for t in tools}
    assert "debug_status" in names, names
    print(f"handshake OK, {len(tools)} tools")


def text_of(result):
    return result["content"][0]["text"]


def test_exec_control():
    # step requires the debug interpreter core; the default boot core is
    # the plain (non-debug) interpreter, so switch explicitly first.
    r = call_tool("set_debug_option", {"cpu_core": "interpreter"})
    assert not r.get("isError"), r

    st = text_of(call_tool("debug_status"))
    print("status:", st.splitlines()[0])
    assert "state:" in st

    r = call_tool("pause")
    assert not r.get("isError"), r
    st = text_of(call_tool("debug_status"))
    assert "state: stopped" in st, st

    r = call_tool("step", {"cpu": "master", "count": 5})
    assert not r.get("isError"), r
    assert "PC=" in text_of(r)

    r = call_tool("resume")
    assert not r.get("isError"), r
    st = text_of(call_tool("debug_status"))
    assert "state: running" in st, st

    # wait_for_stop must time out cleanly while nothing stops the CPU
    r = call_tool("wait_for_stop", {"timeout_ms": 500})
    assert "stopped: false" in text_of(r), r
    print("exec control OK")


def test_breakpoints():
    r = call_tool("breakpoint_add",
                  {"cpu": "master", "type": "code", "address": "0x06004000"})
    assert not r.get("isError"), r
    lst = text_of(call_tool("breakpoint_list", {"cpu": "master"}))
    assert "06004000" in lst, lst
    r = call_tool("breakpoint_remove",
                  {"cpu": "master", "type": "code", "address": "0x06004000"})
    assert not r.get("isError"), r
    lst = text_of(call_tool("breakpoint_list", {"cpu": "master"}))
    assert "06004000" not in lst, lst
    print("breakpoints OK")


def test_history_and_options():
    r = call_tool("set_debug_option", {"cpu_core": "interpreter"})
    assert not r.get("isError"), r

    # Negative test: an invalid cpu_core must be rejected and must not
    # change the currently active core (no partial application).
    core_before = [l for l in text_of(call_tool("debug_status")).splitlines()
                   if l.startswith("sh2 core:")][0]
    r = call_tool("set_debug_option", {"cpu_core": "bogus"})
    assert r.get("isError"), r
    core_after = [l for l in text_of(call_tool("debug_status")).splitlines()
                  if l.startswith("sh2 core:")][0]
    assert core_after == core_before, (core_before, core_after)

    r = call_tool("set_debug_option", {"history": "on"})
    assert not r.get("isError"), r
    import time
    time.sleep(0.5)  # let some instructions record
    call_tool("pause")
    h = text_of(call_tool("exec_history", {"cpu": "master", "count": 20}))
    assert h.count("\n") >= 5, h
    bt = text_of(call_tool("backtrace", {"cpu": "master"}))
    assert "0x" in bt or "empty" in bt, bt
    call_tool("resume")

    # Verify debug_status reports the debug interpreter core after the switch.
    st = text_of(call_tool("debug_status"))
    assert "SH2 Debugger Interpreter" in st, st

    # Confirm the game is still alive: state running and frame count advancing.
    st1 = text_of(call_tool("debug_status"))
    assert "state: running" in st1, st1
    frame1 = int(
        [l for l in st1.splitlines() if l.startswith("frame:")][0].split()[1])
    time.sleep(1.0)
    st2 = text_of(call_tool("debug_status"))
    frame2 = int(
        [l for l in st2.splitlines() if l.startswith("frame:")][0].split()[1])
    assert frame2 > frame1, (frame1, frame2, st1, st2)

    # Switch back to dynarec and confirm the game is still alive afterwards.
    r = call_tool("set_debug_option", {"cpu_core": "dynarec"})
    assert not r.get("isError"), r
    st3 = text_of(call_tool("debug_status"))
    assert "state: running" in st3, st3
    frame3 = int(
        [l for l in st3.splitlines() if l.startswith("frame:")][0].split()[1])
    time.sleep(1.0)
    st4 = text_of(call_tool("debug_status"))
    assert "state: running" in st4, st4
    frame4 = int(
        [l for l in st4.splitlines() if l.startswith("frame:")][0].split()[1])
    assert frame4 > frame3, (frame3, frame4, st3, st4)
    print("history/options OK")


def test_inspection():
    call_tool("pause")
    regs = text_of(call_tool("registers_get", {"cpu": "master"}))
    assert "R0=" in regs and "PC=" in regs

    # Saturn work RAM high: write via MCP and read back
    call_tool("memory_write",
              {"address": "0x06000000", "size": "long", "value": "0xDEADBEEF"})
    dump = text_of(call_tool("memory_read",
                             {"address": "0x06000000", "length": 16,
                              "format": "u32"}))
    assert "DEADBEEF" in dump.upper(), dump

    dis = text_of(call_tool("disassemble",
                            {"address": "0x06000000", "count": 4}))
    assert dis.count("\n") >= 3, dis

    # registers_set roundtrip on R12: set, read back, verify, restore
    orig_regs = text_of(call_tool("registers_get", {"cpu": "master"}))
    orig_r12 = None
    for line in orig_regs.splitlines():
      if "R12=" in line:
        parts = line.split("R12=")
        if len(parts) > 1:
          orig_r12 = int(parts[1].split()[0], 16)
          break

    call_tool("registers_set", {"cpu": "master", "reg": "R12", "value": "0x12345678"})
    regs_after = text_of(call_tool("registers_get", {"cpu": "master"}))
    assert "R12=0x12345678" in regs_after, regs_after

    if orig_r12 is not None:
      call_tool("registers_set", {"cpu": "master", "reg": "R12", "value": f"0x{orig_r12:X}"})

    call_tool("resume")
    print("inspection OK")


def test_breakpoint_hit_flow():
    call_tool("set_debug_option", {"cpu_core": "interpreter"})
    # Current master PC is a place the game will re-execute in most loops;
    # use the current PC itself as a self-evident breakpoint target.
    call_tool("pause")
    st = text_of(call_tool("debug_status"))
    pc = [l for l in st.splitlines() if "MSH2 PC=" in l][0]
    addr = pc.split("PC=")[1].split()[0]
    call_tool("breakpoint_add",
              {"cpu": "master", "type": "code", "address": addr})
    call_tool("resume")
    r = call_tool("wait_for_stop", {"timeout_ms": 10000})
    rt = text_of(r)
    if "stopped: false" in rt:
        # Game loops usually revisit the paused PC quickly, but not always
        # within 10s; give it one more chance before failing.
        call_tool("resume")
        r = call_tool("wait_for_stop", {"timeout_ms": 10000})
        rt = text_of(r)
    assert "stopped: true" in rt, rt
    assert addr.lower().replace("0x", "") in rt.lower(), rt
    st = text_of(call_tool("debug_status"))
    assert addr.lower() in st.lower(), ("CPU not frozen at BP", st)
    call_tool("breakpoint_remove",
              {"cpu": "master", "type": "code", "address": addr})
    call_tool("resume")
    print("breakpoint hit flow OK")


if __name__ == "__main__":
    test_handshake()
    test_exec_control()
    test_breakpoints()
    test_inspection()
    test_history_and_options()
    test_breakpoint_hit_flow()
    print("ALL OK")
