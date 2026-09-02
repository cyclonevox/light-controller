#!/usr/bin/env python3
"""不插板子：在本机起固件进程，用 HTTP 走配网、存 WiFi、开关灯。"""
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(__file__).resolve().parent / "build"
FIRMWARE = BUILD / "firmware"
WRITE_EEPROM = BUILD / "write_eeprom"

passed = 0
failed = 0


class Failed(Exception):
    pass


def pick_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


PORT = pick_port()


def expect(ok, name):
    global passed, failed
    if ok:
        passed += 1
        print(f"ok  {name}")
    else:
        failed += 1
        print(f"FAIL  {name}")
    return ok


def http_get(path, timeout=3):
    sock = socket.create_connection(("127.0.0.1", PORT), timeout=timeout)
    try:
        sock.sendall(
            f"GET {path} HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n".encode()
        )
        chunks = []
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        return b"".join(chunks).decode("utf-8", "replace")
    finally:
        sock.close()


def wait_port(proc, timeout=3):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            raise Failed(f"firmware exited early with {proc.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", PORT), 0.15):
                return
        except OSError:
            time.sleep(0.05)
    raise Failed("firmware did not open the HTTP port")


def start_fw(tmpdir, **extra):
    env = os.environ.copy()
    env.update(
        {
            "HTTP_PORT": str(PORT),
            "EEPROM_FILE": str(tmpdir / "eeprom.bin"),
            "SERIAL_LOG": str(tmpdir / "serial.log"),
            "DISPLAY_FILE": str(tmpdir / "display.txt"),
            "PIN_FILE": str(tmpdir / "pins.txt"),
            "WIFI_SIM": "ok",
            "WIFI_MODULE": "ok",
        }
    )
    env.update(extra)
    proc = subprocess.Popen(
        [str(FIRMWARE)],
        cwd=str(ROOT),
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return proc


def stop(proc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=1)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=1)


def read_text(path):
    path = Path(path)
    return path.read_text(encoding="utf-8") if path.exists() else ""


def write_eeprom(path, state, ssid, password, boot_flag):
    subprocess.check_call(
        [str(WRITE_EEPROM), str(path), str(state), ssid, password, str(boot_flag)]
    )


def test_first_boot_opens_setup():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        proc = start_fw(tmp)
        try:
            wait_port(proc)
            body = http_get("/")
            expect("配置 WiFi" in body, "no saved WiFi opens setup page")
            expect("SETUP 192.168.4.1" in read_text(tmp / "display.txt"), "AP IP scrolls on matrix")
            expect("8=0" in read_text(tmp / "pins.txt"), "relay starts off")
        finally:
            stop(proc)


def test_save_then_switch_light():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        proc = start_fw(tmp)
        try:
            wait_port(proc)
            body = http_get("/save?ssid=my%20net&pass=s3cret")
            expect("已保存" in body, "save returns confirmation page")
        except Failed as exc:
            expect(False, f"save request failed: {exc}")
            stop(proc)
            return
        code = proc.wait(timeout=2)
        expect(code == 82, "save reboots the board")

        proc = start_fw(tmp)
        try:
            wait_port(proc)
            page = http_get("/")
            status = http_get("/api/status")
            serial = read_text(tmp / "serial.log")
            expect("灯光开关" in page, "after save, station mode serves light page")
            expect("Connecting to my net" in serial, "ssid is url-decoded before connect")
            expect("Open http://192.168.1.50" in serial, "station IP is printed")
            expect('"on":false' in status, "status starts off")

            on = http_get("/api/on")
            expect('"on":true' in on and "8=1" in read_text(tmp / "pins.txt"), "/api/on turns relay on")
            off = http_get("/api/off")
            expect('"on":false' in off and "8=0" in read_text(tmp / "pins.txt"), "/api/off turns relay off")
            toggle = http_get("/api/toggle")
            expect('"on":true' in toggle and "8=1" in read_text(tmp / "pins.txt"), "/api/toggle flips the light")
        finally:
            stop(proc)


def test_forget_returns_to_setup():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        write_eeprom(tmp / "eeprom.bin", 1, "home", "pw", 0)
        proc = start_fw(tmp)
        try:
            wait_port(proc)
            expect("灯光开关" in http_get("/"), "saved creds boot into light page")
            expect("已保存" in http_get("/forget?confirm=1"), "forget returns confirmation")
        except Failed as exc:
            expect(False, f"forget request failed: {exc}")
            stop(proc)
            return
        proc.wait(timeout=2)

        proc = start_fw(tmp)
        try:
            wait_port(proc)
            expect("配置 WiFi" in http_get("/"), "after forget, setup page is back")
        finally:
            stop(proc)


def test_bad_wifi_falls_back_to_setup():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        write_eeprom(tmp / "eeprom.bin", 1, "home", "pw", 0)
        proc = start_fw(tmp, WIFI_SIM="fail")
        try:
            wait_port(proc)
            expect("配置 WiFi" in http_get("/"), "failed station connect opens setup")
            expect("SETUP 192.168.4.1" in read_text(tmp / "display.txt"), "failed connect scrolls setup IP")
        finally:
            stop(proc)


def test_double_reset_opens_setup_even_with_creds():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        write_eeprom(tmp / "eeprom.bin", 1, "home", "pw", 1)
        proc = start_fw(tmp)
        try:
            wait_port(proc)
            page = http_get("/")
            expect("配置 WiFi" in page, "armed boot flag forces setup portal")
            expect("灯光开关" not in page, "armed boot flag does not serve light page")
        finally:
            stop(proc)


def test_config_mode_ignores_light_api():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        proc = start_fw(tmp)
        try:
            wait_port(proc)
            body = http_get("/api/toggle")
            expect("配置 WiFi" in body, "setup mode ignores /api/toggle")
            expect("8=0" in read_text(tmp / "pins.txt"), "setup mode does not flip the relay")
        finally:
            stop(proc)


def test_missing_wifi_module():
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        proc = start_fw(tmp, WIFI_MODULE="none")
        time.sleep(0.3)
        try:
            listening = True
            try:
                with socket.create_connection(("127.0.0.1", PORT), 0.2):
                    pass
            except OSError:
                listening = False
            serial = read_text(tmp / "serial.log")
            expect(not listening, "no WiFi module does not start HTTP")
            expect("No WiFi module" in serial, "no WiFi module is logged")
            expect("NO WIFI" in read_text(tmp / "display.txt"), "no WiFi module scrolls NO WIFI")
        finally:
            stop(proc)


def main():
    if not FIRMWARE.is_file():
        print(f"missing {FIRMWARE}; run test/run.sh", file=sys.stderr)
        return 2
    test_first_boot_opens_setup()
    test_save_then_switch_light()
    test_forget_returns_to_setup()
    test_bad_wifi_falls_back_to_setup()
    test_double_reset_opens_setup_even_with_creds()
    test_config_mode_ignores_light_api()
    test_missing_wifi_module()
    print(f"{passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
