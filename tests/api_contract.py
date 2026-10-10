#!/usr/bin/env python3
"""Smoke-test the public JSON contract against a fresh Demo instance."""

import argparse
import http.server
import json
import socket
import struct
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


PASSWORD_HASH = "a6d82bced638de3def1e9bbb4983225c"
MAX_SOPTS_SIZE = 320
SOPT_WEATHER_URL = 3


def require_keys(name, value, keys):
    if not isinstance(value, dict):
        raise AssertionError(f"{name}: expected object, got {type(value).__name__}")
    missing = sorted(set(keys) - set(value))
    if missing:
        raise AssertionError(f"{name}: missing keys: {', '.join(missing)}")


class DemoServer:
    def __init__(self, binary, port):
        self.binary = binary
        self.port = port
        self.temp_dir = None
        self.log_file = None
        self.process = None

    def __enter__(self):
        with socket.socket() as sock:
            if sock.connect_ex(("127.0.0.1", self.port)) == 0:
                raise RuntimeError(f"port {self.port} is already in use")

        self.temp_dir = tempfile.TemporaryDirectory(prefix="opensprinkler-api-")
        log_path = Path(self.temp_dir.name) / "server.log"
        self.log_file = log_path.open("w+")
        self.process = subprocess.Popen(
            [str(self.binary), "-d", self.temp_dir.name],
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
        )

        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                break
            try:
                self.get_json("jo")
                return self
            except (OSError, urllib.error.URLError, json.JSONDecodeError):
                time.sleep(0.1)

        self.log_file.flush()
        self.log_file.seek(0)
        output = self.log_file.read()
        self._cleanup()
        raise RuntimeError(f"Demo server did not start on port {self.port}\n{output}")

    def __exit__(self, exc_type, exc_value, traceback):
        self._cleanup()

    def _cleanup(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        if self.log_file:
            self.log_file.close()
        if self.temp_dir:
            self.temp_dir.cleanup()

    def get_json(self, endpoint, params=None):
        query_params = {"pw": PASSWORD_HASH}
        if params:
            query_params.update(params)
        query = urllib.parse.urlencode(query_params)
        url = f"http://127.0.0.1:{self.port}/{endpoint}?{query}"
        with urllib.request.urlopen(url, timeout=3) as response:
            return json.load(response)

    def get_response(self, endpoint, params=None):
        query_params = {"pw": PASSWORD_HASH}
        if params:
            query_params.update(params)
        query = urllib.parse.urlencode(query_params)
        url = f"http://127.0.0.1:{self.port}/{endpoint}?{query}"
        with urllib.request.urlopen(url, timeout=3) as response:
            return response.status, response.headers, response.read()

    def raw_http(self, headers, body_parts=(), shutdown_write=False):
        with socket.create_connection(("127.0.0.1", self.port), timeout=3) as sock:
            sock.settimeout(4)
            sock.sendall(headers)
            for delay, part in body_parts:
                if delay:
                    time.sleep(delay)
                sock.sendall(part)
            if shutdown_write:
                sock.shutdown(socket.SHUT_WR)

            response = bytearray()
            while True:
                try:
                    chunk = sock.recv(4096)
                except ConnectionResetError:
                    break
                if not chunk:
                    break
                response.extend(chunk)
            return bytes(response)

    def set_string_option(self, option_id, value):
        encoded = value.encode("ascii")
        if len(encoded) >= MAX_SOPTS_SIZE:
            raise ValueError("string option is too long")
        path = Path(self.temp_dir.name) / "sopts.dat"
        with path.open("r+b", buffering=0) as options:
            options.seek(option_id * MAX_SOPTS_SIZE)
            options.write(encoded + b"\0" * (MAX_SOPTS_SIZE - len(encoded)))


class WeatherMock:
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = self.server.weather_body.encode("ascii")
            response = (
                b"HTTP/1.0 200 OK\r\n"
                b"Content-Type: text/plain\r\n"
                + f"Content-Length: {len(body)}\r\n".encode("ascii")
                + b"Connection: close\r\n\r\n"
                + body
            )
            # The Linux firmware client performs one blocking read. Send the
            # complete response in one write so this fixture matches that
            # existing transport contract.
            self.connection.sendall(response)
            self.close_connection = True

        def log_message(self, _format, *_args):
            pass

    def __init__(self):
        self.server = http.server.ThreadingHTTPServer(
            ("127.0.0.1", 0), self.Handler
        )
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.set_response(100, 0)
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)

    @property
    def url(self):
        return f"http://127.0.0.1:{self.server.server_port}"

    def set_response(self, scale, restricted):
        self.server.weather_body = (
            f"&errCode=0&scale={scale}&restricted={restricted}"
            '&rawData={"provider":"test"}'
        )


def check_options(data):
    require_keys(
        "/jo",
        data,
        [
            "fwv", "fwm", "tz", "hp0", "hp1", "hwv", "hwt", "ext",
            "sdt", "mas", "mas2", "mas3", "mas4", "mton", "mton2",
            "mton3", "mton4", "mtof", "mtof2", "mtof3", "mtof4", "wl",
            "den", "ipas", "devid", "dim", "uwt", "ntp1", "ntp2", "ntp3",
            "ntp4", "lg", "fpr0", "fpr1", "re", "sar", "ife", "ife2",
            "sn1t", "sn1o", "sn2t", "sn2o", "sn1on", "sn1of", "sn2on",
            "sn2of", "wimod", "reset", "dexp", "mexp", "ms",
        ],
    )
    assert isinstance(data["ms"], list) and len(data["ms"]) == 12


def check_controller(data):
    require_keys(
        "/jc",
        data,
        [
            "devt", "nbrd", "en", "sn1", "sn2", "rd", "rdst", "sunrise",
            "sunset", "eip", "lwc", "lswc", "lupt", "lrbtc", "lrun", "pq",
            "pt", "nq", "ocs", "otc", "otcs", "mac", "loc", "jsp", "wsp",
            "wto", "ifkey", "mqtt", "wtdata", "wterr", "wtrestr", "dname",
            "email", "wls", "sbits", "bap", "ps", "gpio",
        ],
    )
    assert isinstance(data["lrun"], list) and len(data["lrun"]) == 4
    assert isinstance(data["ps"], list)
    assert "hname" not in data, "Linux uses the operating-system hostname"


def check_stations(data):
    require_keys(
        "/jn",
        data,
        [
            "masop", "masop2", "masop3", "masop4", "ignore_rain",
            "ignore_sn1", "ignore_sn2", "stn_dis", "stn_spe", "stn_grp",
            "stn_bnd", "bmt", "snames", "maxlen",
        ],
    )
    assert len(data["snames"]) == len(data["stn_grp"])
    assert data["bmt"] == 1


def check_programs(data):
    require_keys("/jp", data, ["nprogs", "nboards", "mnp", "mnst", "pnsize", "pd"])
    assert isinstance(data["pd"], list) and data["nprogs"] == len(data["pd"])


def check_sensors(data):
    require_keys("/jsn", data, ["sn", "count"])
    assert isinstance(data["sn"], list) and data["count"] == len(data["sn"])


def check_sensor_definitions(data):
    require_keys("/jsd", data, ["sensors", "units", "enums", "as", "flags"])
    assert all(isinstance(item, list) and len(item) == 4 for item in data["units"])
    assert len(data["units"]) >= 56
    require_keys(
        "/jsd.enums",
        data["enums"],
        ["SensorUnitGroup", "AggregateAction", "WeatherAction"],
    )

    sensor_names = {item.get("n") for item in data["sensors"]}
    expected_names = {
        "Aggregate Sensor",
        "ADS1115 Sensor (simulated)",
        "Weather Sensor",
        "System Internal",
        "Onboard Digital",
    }
    assert expected_names <= sensor_names
    assert len(data["sensors"]) == 6
    assert data["sensors"][5] == {"dis": 1}  # Demo has no onboard ADC
    internal = data["sensors"][3]
    assert internal["n"] == "System Internal"
    metrics = internal["as"][0]
    assert metrics["a"] == "metric" and metrics["d"] == "3"
    assert [item["id"] for item in metrics["o"]] == [3]  # Linux/Demo
    assert metrics["o"][0]["l"] == "CPU Temperature"
    weather = next(item for item in data["sensors"] if item.get("n") == "Weather Sensor")
    assert not weather.get("dis")
    assert len(data["enums"]["WeatherAction"]) == 13
    assert {item.get("a") for item in data["as"]} == {
        "name", "interval", "unit", "min", "max", "type"
    }
    assert [item.get("n") for item in data["flags"]] == [
        "Enabled", "Logging", "Show on Home"
    ]


def check_program_adjustments(data):
    require_keys("/jpa", data, ["jpa", "maxrt"])
    assert isinstance(data["jpa"], list)
    assert isinstance(data["maxrt"], int) and data["maxrt"] > 0


def check_combined(data):
    require_keys(
        "/ja",
        data,
        ["settings", "programs", "options", "status", "stations", "sensors"],
    )
    require_keys("/ja.status", data["status"], ["sn", "bap", "nstations"])
    require_keys("/ja.stations", data["stations"], ["stn_bnd", "bmt"])
    assert data["stations"]["bmt"] == 1
    assert len(data["status"]["sn"]) == data["status"]["nstations"]


def check_request_bodies(server):
    path = f"/jo?pw={PASSWORD_HASH}"

    def request(content_length):
        return (
            f"POST {path} HTTP/1.1\r\n"
            f"Host: 127.0.0.1\r\n"
            f"Content-Length: {content_length}\r\n"
            f"Connection: close\r\n\r\n"
        ).encode()

    fragmented = server.raw_http(
        request("6"),
        [(0, b"abc"), (0.05, b"def")],
    )
    assert fragmented.startswith(b"HTTP/1.1 200"), fragmented[:80]

    malformed = server.raw_http(request("12x"))
    assert malformed.startswith(b"HTTP/1.1 400"), malformed[:80]

    oversized = server.raw_http(request("8193"), shutdown_write=True)
    assert oversized.startswith(b"HTTP/1.1 413"), oversized[:80]

    truncated = server.raw_http(
        request("6"),
        [(0, b"abc")],
        shutdown_write=True,
    )
    assert truncated.startswith(b"HTTP/1.1 408"), truncated[:80]
    print("PASS HTTP request body handling")


def check_nested_option_encodings(server):
    original_name = server.get_json("jc")["dname"]
    for hostname in ("os-test", ""):
        result = server.get_json("co", {"hname": hostname, "dname": "must-not-change"})
        assert result == {"result": 48, "item": "hname"}, result
        assert server.get_json("jc")["dname"] == original_name
    assert "hostname" not in server.get_json("db")

    legacy_values = {
        "wto": {"h": 20, "t": 50, "r": 100},
        "otc": {"en": 0, "token": "legacy-token"},
        "mqtt": {"en": 0, "host": "legacy-broker", "port": 1883},
        "email": {"en": 0, "host": "legacy-mail", "port": 465},
    }
    braced_values = {
        "wto": {"h": 30, "t": 60, "r": 90},
        "otc": {"en": 0, "token": "braced-token"},
        "mqtt": {"en": 0, "host": "braced-broker", "extra": {"nested": 1}},
        "email": {"en": 0, "host": "braced-mail", "port": 587},
    }

    for key, value in legacy_values.items():
        encoded = json.dumps(value, separators=(",", ":"))[1:-1]
        result = server.get_json("co", {key: encoded})
        assert result["result"] == 1, (key, result)
        assert server.get_json("jc")[key] == value, key

    for key, value in braced_values.items():
        encoded = json.dumps(value, separators=(",", ":"))
        result = server.get_json("co", {key: encoded})
        assert result["result"] == 1, (key, result)
        assert server.get_json("jc")[key] == value, key

    # Empty complete objects normalize to empty stored fragments and remain
    # objects when emitted by /jc and /ja.
    for key in braced_values:
        result = server.get_json("co", {key: "{}"})
        assert result["result"] == 1, (key, result)
        assert server.get_json("jc")[key] == {}, key
    server.get_json("ja")
    print("PASS nested option object encodings")


def check_control_commands(server):
    result = server.get_json("cm", {"sid": 0, "en": 1, "t": 64800, "qo": 0})
    assert result["result"] == 1, result
    status = server.get_json("jc")
    assert status["ps"][0][0] == 99, status["ps"][0]
    assert status["nq"] == 1, status["nq"]

    result = server.get_json("cv", {"rsn": 0, "rbt": 0})
    assert result["result"] == 1, result
    status = server.get_json("jc")
    assert status["ps"][0][0] == 99, status["ps"][0]

    result = server.get_json("cm", {"sid": 0, "en": 1, "t": 30})
    assert result["result"] == 0x30, result
    status = server.get_json("jc")
    assert status["ps"][0][0] == 99, status["ps"][0]

    result = server.get_json("cm", {"sid": 0, "en": 0})
    assert result["result"] == 1, result
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["nq"] == 0 and status["sbits"][0] == 0:
            break
        time.sleep(0.05)
    assert status["nq"] == 0 and status["sbits"][0] == 0, status

    result = server.get_json("cm", {"sid": 0, "en": 0})
    assert result["result"] == 0x11, result

    result = server.get_json("cv", {"rd": -1})
    assert result["result"] == 0x11, result

    # A timed pause must keep the station off through the expiration tick.
    # Previously pause_state remained set for one tick after pause_timer reached
    # zero, allowing the scheduler to pulse the station on before resuming it.
    try:
        result = server.get_json("cv", {"rsn": 1})
        assert result["result"] == 1, result
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["nq"] == 0:
                break
            time.sleep(0.05)
        assert status["nq"] == 0, status

        result = server.get_json("cm", {"sid": 0, "en": 1, "t": 12, "qo": 2})
        assert result["result"] == 1, result

        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["sbits"][0] & 1:
                break
            time.sleep(0.05)
        assert status["sbits"][0] & 1, status

        result = server.get_json("pq", {"repl": 2})
        assert result["result"] == 1, result

        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["pq"] == 1 and not (status["sbits"][0] & 1):
                break
            time.sleep(0.05)
        assert status["pq"] == 1 and not (status["sbits"][0] & 1), status

        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["pq"] == 0:
                break
            assert not (status["sbits"][0] & 1), status
            time.sleep(0.05)
        assert status["pq"] == 0, status

        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["sbits"][0] & 1:
                break
            time.sleep(0.05)
        assert status["sbits"][0] & 1, status
    finally:
        server.get_json("cv", {"rsn": 1})
        server.get_json("pq", {"repl": 0})

    program_count = server.get_json("jp")["nprogs"]
    # Extra entries from a previously larger station setup remain compatible.
    result = server.get_json("cr", {"t": "[64800,0,0,0,0,0,0,0,999]", "cnt": 0})
    assert result["result"] == 1, result
    assert server.get_json("jp")["nprogs"] == program_count
    result = server.get_json("cv", {"rsn": 1})
    assert result["result"] == 1, result

    result = server.get_json("cr", {"t": "[60,0"})
    assert result["result"] == 0x12, result
    print("PASS shared control command behavior")


def check_weather_failsafe(server):
    original_options = server.get_json("jo")
    original_weather_url = server.get_json("jc")["wsp"]
    program_id = None

    def reset_queue():
        result = server.get_json("cv", {"rsn": 1})
        assert result["result"] == 1, result
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["nq"] == 0:
                return
            time.sleep(0.05)
        raise AssertionError(("queue did not reset", status))

    def wait_for_weather(scale, restricted):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            options = server.get_json("jo")
            if (status["wterr"] == 0 and status["wtrestr"] == restricted and
                    options["wl"] == scale):
                return
            time.sleep(0.05)
        raise AssertionError(("weather response not applied", status, options))

    def start_guard_station():
        result = server.get_json("cm", {"sid": 7, "en": 1, "t": 120, "qo": 2})
        assert result["result"] == 1, result
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["ps"][7][0] == 99:
                return
            time.sleep(0.05)
        raise AssertionError(("guard station did not start", status))

    def assert_scaled_runs(expected_duration):
        result = server.get_json(
            "cr", {"t": "[100,0,0,0,0,0,0,0]", "uwt": 1, "qo": 0}
        )
        assert result["result"] == 1, result
        result = server.get_json("mp", {"pid": program_id, "uwt": 1, "qo": 0})
        assert result["result"] == 1, result
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            status = server.get_json("jc")
            if status["ps"][0][0] and status["ps"][1][0]:
                break
            time.sleep(0.05)
        assert status["ps"][0][1] == expected_duration, status["ps"][0]
        assert status["ps"][1][1] == expected_duration, status["ps"][1]

    with WeatherMock() as weather:
        try:
            server.set_string_option(SOPT_WEATHER_URL, weather.url)
            program_id = server.get_json("jp")["nprogs"]
            packed_program = json.dumps(
                [3, 127, 0, [0, 0, 0, 0], [0, 100, 0, 0, 0, 0, 0, 0]],
                separators=(",", ":"),
            )
            result = server.get_json("cp", {
                "pid": -1,
                "v": packed_program,
                "name": "Weather fail-safe test",
            })
            assert result["result"] == 1, result

            # A fresh restricted Zimmerman response with wl=0 would suppress
            # both runs. Keep cleanup deferred while its short test freshness
            # horizon expires; the consumers must then ignore both values.
            weather.set_response(0, 1)
            result = server.get_json("co", {"uwt": 1, "wl": 0})
            assert result["result"] == 1, result
            wait_for_weather(0, 1)
            start_guard_station()
            time.sleep(2.2)
            status = server.get_json("jc")
            assert status["wtrestr"] == 1, status
            assert server.get_json("jo")["wl"] == 0
            assert_scaled_runs(100)

            # Restrictions also expire under Manual, but the locally selected
            # water level remains effective.
            reset_queue()
            weather.set_response(40, 1)
            result = server.get_json("co", {"uwt": 0, "wl": 40})
            assert result["result"] == 1, result
            wait_for_weather(40, 1)
            start_guard_station()
            time.sleep(2.2)
            status = server.get_json("jc")
            assert status["wtrestr"] == 1, status
            assert_scaled_runs(40)
        finally:
            reset_queue()
            if program_id is not None and program_id < server.get_json("jp")["nprogs"]:
                server.get_json("dp", {"pid": program_id})

            # Force a final weather-state reset, restore the original settings,
            # and let the mock satisfy the resulting immediate query before its
            # URL is removed.
            alternate_method = 0 if original_options["uwt"] != 0 else 1
            server.get_json("co", {"uwt": alternate_method})
            weather.set_response(original_options["wl"], 0)
            server.get_json("co", {
                "uwt": original_options["uwt"],
                "wl": original_options["wl"],
            })
            wait_for_weather(original_options["wl"], 0)
            server.set_string_option(SOPT_WEATHER_URL, original_weather_url)

    print("PASS stale weather fail-safe integration")


def check_bundle_commands(server):
    assert server.get_json("cv", {"rsn": 1})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and server.get_json("jc")["nq"]:
        time.sleep(0.05)
    assert server.get_json("jc")["nq"] == 0

    station_data = server.get_json("jn")
    board_count = len(station_data["stn_bnd"])
    assert board_count >= 1

    def membership(*station_ids):
        boards = [0] * board_count
        for sid in station_ids:
            boards[sid // 8] |= 1 << (sid % 8)
        return "".join(f"{value:02X}" for value in boards)

    # The advertised member-type capability and API enforcement must agree.
    # Every setup is asserted independently so an invalid fixture cannot count
    # as a successful member rejection. Demo intentionally exposes no GPIO pin,
    # so type 3 is covered by the native type-mask matrix instead.
    member_type_mask = station_data["bmt"]
    type_payloads = {
        0: "0",
        1: "05331305333C0021",
        2: "C0A80050005000",
        4: "example.com,80,on,off",
        5: "example.com,443,on,off",
        6: "00000000000000000000000000000000X00",
        7: membership(7),
    }
    candidate_sid = 6
    for station_type, payload in type_payloads.items():
        assert server.get_json("cs", {"sid": 0, "st": 0, "sd": "0", "p0": 0})["result"] == 1
        setup = server.get_json("cs", {
            "sid": candidate_sid,
            "st": station_type,
            "sd": payload,
            "p0": 0 if station_type == 0 else 1 << candidate_sid,
        })
        assert setup["result"] == 1, (station_type, setup)
        member_result = server.get_json("cs", {
            "sid": 0, "st": 7, "sd": membership(candidate_sid)
        })["result"]
        advertised = bool(member_type_mask & (1 << station_type))
        assert (member_result == 1) == advertised, (station_type, member_result, member_type_mask)

    assert server.get_json("cs", {"sid": 0, "st": 0, "sd": "0", "p0": 0})["result"] == 1
    assert server.get_json("cs", {"sid": candidate_sid, "st": 0, "sd": "0", "p0": 0})["result"] == 1

    # Bundle 1 claims stations 2 and 3. Its own bit is forbidden.
    result = server.get_json("cs", {"sid": 0, "st": 7, "sd": membership(1, 2)})
    assert result["result"] == 1, result
    assert server.get_json("je")["0"]["st"] == 7
    assert server.get_json("cs", {"sid": 0, "st": 7, "sd": membership(0)})["result"] == 0x11

    # Referenced members cannot become any special-station type, and
    # unsupported special stations cannot be added as bundle members.
    assert server.get_json("cs", {
        "sid": 1, "st": 4, "sd": "example.com,80,on,off"
    })["result"] == 0x30
    assert server.get_json("cs", {
        "sid": 1, "st": 7, "sd": membership(3)
    })["result"] == 0x30
    assert server.get_json("cs", {
        "sid": 3, "st": 4, "sd": "example.com,80,on,off", "p0": 9
    })["result"] == 1
    assert server.get_json("cs", {
        "sid": 0, "st": 7, "sd": membership(1, 3)
    })["result"] == 0x11
    assert server.get_json("cs", {"sid": 3, "st": 0, "sd": "0", "p0": 1})["result"] == 1

    # A bundle member cannot subsequently become a master.
    assert server.get_json("co", {"mas": 2})["result"] == 0x11

    # Five member transitions need 1.25 seconds, plus at least one second of
    # runtime for the final member. Queue timing rounds that up to 3 seconds.
    assert server.get_json("cs", {
        "sid": 0, "st": 7, "sd": membership(1, 2, 3, 4, 5)
    })["result"] == 1
    assert server.get_json("cm", {"sid": 0, "en": 1, "t": 1})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["ps"][0][0] != 0:
            break
        time.sleep(0.05)
    assert status["ps"][0][1] >= 3, status["ps"][0]
    assert server.get_json("cv", {"rsn": 1})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["nq"] == 0 and status["sbits"][0] == 0:
            break
        time.sleep(0.05)
    assert status["nq"] == 0 and status["sbits"][0] == 0
    assert server.get_json("cs", {"sid": 0, "st": 7, "sd": membership(1, 2)})["result"] == 1

    # Disabled members are skipped. Re-enabling one while the leader is active
    # adds its derived claim through the same staggered output path.
    assert server.get_json("cs", {"d0": 4})["result"] == 1
    result = server.get_json("cm", {"sid": 0, "en": 1, "t": 60})
    assert result["result"] == 1, result
    deadline = time.monotonic() + 5
    status = None
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["sbits"][0] & 0x03 == 0x03:
            break
        time.sleep(0.05)
    assert status["sbits"][0] & 0x03 == 0x03, status["sbits"]
    time.sleep(0.5)
    assert server.get_json("jc")["sbits"][0] & 0x04 == 0
    assert server.get_json("cs", {"d0": 0})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["sbits"][0] & 0x07 == 0x07:
            break
        time.sleep(0.05)
    assert status["sbits"][0] & 0x07 == 0x07, status["sbits"]
    assert status["bap"][0] & 0x07 == 0x06, status["bap"]
    assert status["ps"][0][0] != 0, status["ps"][0]

    # Derived-only activity is not an individual /cm run.
    derived_stop = server.get_json("cm", {"sid": 1, "en": 0})
    assert derived_stop["result"] == 0x11, derived_stop
    after_derived_stop = server.get_json("jc")
    assert after_derived_stop["ps"][0][0] != 0, after_derived_stop["ps"][0]

    # Removing an individual claim does not defeat the bundle's OR claim.
    assert server.get_json("cs", {"g1": 255})["result"] == 1
    assert server.get_json("cm", {"sid": 1, "en": 1, "t": 1})["result"] == 1
    deadline = time.monotonic() + 5
    saw_individual_run = False
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["ps"][1][0] != 0:
            saw_individual_run = True
        if saw_individual_run and status["ps"][1][0] == 0:
            break
        time.sleep(0.05)
    assert saw_individual_run and status["ps"][1][0] == 0
    assert status["sbits"][0] & 0x02
    assert status["bap"][0] & 0x02
    assert server.get_json("cs", {"g1": 0})["result"] == 1

    # A member's master binding contributes to the bundle leader's demand window.
    assert server.get_json("cv", {"rsn": 1})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["sbits"][0] & 0x07 == 0:
            break
        time.sleep(0.05)
    assert status["sbits"][0] & 0x07 == 0
    assert server.get_json("cs", {"m0": 2})["result"] == 1
    assert server.get_json("co", {"mas": 8})["result"] == 1
    assert server.get_json("cm", {"sid": 0, "en": 1, "t": 60})["result"] == 1
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["sbits"][0] & 0x87 == 0x87:
            break
        time.sleep(0.05)
    assert status["sbits"][0] & 0x87 == 0x87, status["sbits"]
    assert server.get_json("cv", {"rsn": 1})["result"] == 1
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and server.get_json("jc")["nq"]:
        time.sleep(0.05)
    assert server.get_json("co", {"mas": 0})["result"] == 1
    assert server.get_json("cs", {"m0": 0})["result"] == 1

    # Inserting ahead of a nearly-finished bundle forces it to restart its
    # physical member sequence. Its resumed duration and following station
    # must both account for that new startup interval.
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and server.get_json("jc")["nq"]:
        time.sleep(0.05)
    assert server.get_json("cs", {"sid": 1, "st": 0, "sd": "0"})["result"] == 1
    station_data = server.get_json("jn")
    assert station_data["stn_spe"][0] & 0x02 == 0, station_data["stn_spe"]
    assert server.get_json("cs", {
        "sid": 0, "st": 7, "sd": membership(1, 2, 3, 4, 5)
    })["result"] == 1
    assert server.get_json("cm", {"sid": 0, "en": 1, "t": 1})["result"] == 1
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["ps"][0][0] != 0 and status["ps"][0][1] <= 2:
            break
        time.sleep(0.05)
    assert status["ps"][0][0] != 0
    assert server.get_json("cm", {"sid": 6, "en": 1, "t": 2})["result"] == 1
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["ps"][0][1] <= 1:
            break
        time.sleep(0.05)
    assert server.get_json("cm", {"sid": 7, "en": 1, "t": 1, "qo": 1})["result"] == 1
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        status = server.get_json("jc")
        if status["ps"][0][0] != 0 and status["ps"][0][1] >= 3 and status["ps"][6][2]:
            break
        time.sleep(0.05)
    assert status["ps"][0][1] >= 3, status["ps"][0]
    assert status["ps"][6][2] >= status["ps"][0][2] + 3, (status["ps"][0], status["ps"][6])
    assert server.get_json("cv", {"rsn": 1})["result"] == 1
    print("PASS bundle station behavior")


def check_sprinkler_logs(server):
    assert isinstance(server.get_json("jl", {"hist": 1}), list)
    status, headers, body = server.get_response(
        "jl", {"hist": "all", "fmt": "binary", "page": 1, "count": 2500}
    )
    assert status == 200
    assert headers.get_content_type() == "application/octet-stream"
    assert headers["X-OS-Page-Done"] == "1"
    scanned = int(headers["X-OS-Scanned-Slots"])
    assert 0 <= scanned <= 2500
    assert headers["X-OS-Record-Size"] == "16"
    assert len(headers["X-OS-Next-Cursor"]) == 12
    assert len(body) % 16 == 0 and len(body) // 16 <= scanned
    assert server.get_json("jl", {"hist": 1, "fmt": "binary"})["result"] == 0x12

    day = 10
    log_dir = Path(server.temp_dir.name) / "logs"
    log_dir.mkdir(exist_ok=True)
    (log_dir / f"{day}.txt").write_text(
        "[3,2,90,864100]\n[0,\"wl\",100,864200]\n", encoding="ascii"
    )
    window = {"start": day * 86400, "end": (day + 1) * 86400 - 1}
    # Legacy daily files remain on Linux, but /jl is ring-only in 2.2.1(6).
    assert server.get_json("jl", window) == []
    assert server.get_json("jl", {**window, "type": "wl"}) == []

    binary_params = {**window, "fmt": "binary", "page": 1, "count": 1}
    _, first_headers, first_body = server.get_response("jl", binary_params)
    assert first_headers["X-OS-Page-Done"] == "1"
    assert first_headers["X-OS-Scanned-Slots"] == "0"
    assert first_body == b""
    assert server.get_json("jl", {
        **binary_params, "cursor": "0000000000000000000000000000"
    })["result"] == 0x12

    assert server.get_json("dl", {"day": day})["result"] == 1
    assert server.get_json("jl", window) == []
    assert server.get_json("dl", {"before": 1})["result"] == 1
    print("PASS sprinkler log API")


def check_debug(value):
    require_keys("db", value, ["sprlog_fail", "sprlog_mismatch"])
    assert value["sprlog_fail"] == 0
    assert value["sprlog_mismatch"] == 0


def check_scoped_file_reads(server):
    assert server.get_json("co", {"ext": 8})["result"] == 1
    policy_path = Path(server.temp_dir.name) / "stnx.dat"
    assert not policy_path.exists()
    assert server.get_json("jn")["cs"] == [[0, 0, 0]] * 72
    assert not policy_path.exists(), "reading missing policies must not create the file"

    names = [f"Zone-{sid:02d}-" + "x" * 24 for sid in range(72)]
    policies = [[sid % 2, (sid + 1) * 60, 120] for sid in range(72)]
    for sid, (enabled, cycle, soak) in enumerate(policies):
        assert server.get_json("cs", {
            f"s{sid}": names[sid], "sid": sid, "cse": enabled,
            "csc": cycle, "css": soak,
        })["result"] == 1
    for endpoint in ("jn", "ja"):
        data = server.get_json(endpoint)
        stations = data if endpoint == "jn" else data["stations"]
        assert stations["snames"] == names
        assert stations["cs"] == policies

    names[71] = "Updated last zone"
    policies[71] = [1, 120, 180]
    assert server.get_json("cs", {
        "s71": names[71], "sid": 71, "cse": 1, "csc": 120, "css": 180,
    })["result"] == 1
    assert server.get_json("jn")["snames"] == names
    assert server.get_json("jn")["cs"] == policies
    for name in ("First device name", "Updated device name"):
        assert server.get_json("co", {"dname": name})["result"] == 1
        assert server.get_json("jc")["dname"] == name
        assert server.get_json("ja")["settings"]["dname"] == name

    original = policy_path.read_bytes()
    policy_path.write_bytes(b"BAD!" + original[4:])
    assert server.get_json("jn")["cs"] == [[0, 0, 0]] * 72
    policy_path.write_bytes(original)
    assert server.get_json("jn")["cs"] == policies
    policy_path.unlink()
    assert server.get_json("jn")["cs"] == [[0, 0, 0]] * 72
    assert not policy_path.exists()
    print("PASS scoped file reads, 72-zone responses and request-to-request freshness")


def check_cycle_runtime(server):
    path = Path(server.temp_dir.name) / "stnx.dat"
    assert server.get_json("cs", {"sid": 0, "cse": 1, "csc": 60, "css": 60})["result"] == 1
    stations = server.get_json("jn")
    assert stations["csver"] == 1 and stations["qmax"] == 255
    assert stations["cs"][0] == [1, 60, 60]
    original = path.read_bytes()
    for bad in ("", "60junk", "-1", "59", "14401"):
        assert server.get_json("cs", {"sid": 0, "csc": bad})["result"] == 0x11
        assert path.read_bytes() == original

    def wait_status(predicate):
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            value = server.get_json("jc")
            if predicate(value):
                return value
            time.sleep(0.05)
        raise AssertionError(value)

    def stop():
        assert server.get_json("cv", {"rsn": 1})["result"] == 1
        wait_status(lambda value: value["nq"] == 0)

    try:
        # Run Once consumes the policy. 121 seconds splits into 41+40+40.
        assert server.get_json("cr", {"t": "[121,0,0,0,0,0,0,0]"})["result"] == 1
        value = wait_status(lambda value: value["ps"][0][0] != 0)
        assert 38 <= value["ps"][0][1] <= 41, value
        assert value["cs"][0][0] == 0 and value["cs"][0][3] >= 118, value
        assert server.get_json("cs", {"sid": 0, "csc": 120})["result"] == 1
        assert server.get_json("jc")["cs"][0][4] <= 41  # live chain keeps its snapshot
        stop()
        assert server.get_json("cs", {"sid": 0, "csc": 60})["result"] == 1

        # Direct manual control remains a bypass unless explicitly opted in.
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121})["result"] == 1
        value = wait_status(lambda value: value["ps"][0][0] != 0)
        assert value["ps"][0][1] > 100, value
        stop()
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121, "cs": 1})["result"] == 1
        value = wait_status(lambda value: value["ps"][0][0] != 0)
        assert 38 <= value["ps"][0][1] <= 41, value
        stop()
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121, "cs": 2})["result"] == 0x11

        # RE bypasses even the explicit flag. No change to stored policy.
        assert server.get_json("cv", {"re": 1})["result"] == 1
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121, "cs": 1})["result"] == 1
        value = wait_status(lambda value: value["ps"][0][0] != 0)
        assert value["ps"][0][1] > 100, value
        stop()
        assert path.read_bytes() == original
        assert server.get_json("cv", {"re": 0})["result"] == 1

        # Replacement stops a bypass run but must retain its station-wide soak.
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121})["result"] == 1
        wait_status(lambda value: value["sbits"][0] & 1)
        assert server.get_json("cr", {"t": "[121,0,0,0,0,0,0,0]", "qo": 2})["result"] == 1
        value = server.get_json("jc")
        assert value["nq"] == 1 and not value["sbits"][0] & 1, value
        assert value["cs"][0][6] >= value["devt"] + 58, value

        # This disposable DEMO exercises the clock-reset path without setting the host clock.
        assert server.get_json("co", {"ntp": 0, "ttt": value["devt"]})["result"] == 1
        assert server.get_json("jc")["nq"] == 0
        assert server.get_json("cr", {"t": "[121,0,0,0,0,0,0,0]"})["result"] == 1
        value = wait_status(lambda value: value["sbits"][0] & 1)
        assert value["cs"][0][2] == 1, value
        stop()

        # A malformed file stays untouched and falls back to ordinary watering.
        path.write_bytes(b"bad file")
        assert server.get_json("cm", {"sid": 0, "en": 1, "t": 121, "cs": 1})["result"] == 1
        value = wait_status(lambda value: value["ps"][0][0] != 0)
        assert value["ps"][0][1] > 100, value
        assert path.read_bytes() == b"bad file"
        assert value["cserr"] == 1, value
        stop()
        assert server.get_json("cs", {"csreset": 1})["result"] == 1
        assert server.get_json("jn")["cs"][0] == [0, 0, 0]
        assert server.get_json("jc")["cserr"] == 0
    finally:
        server.get_json("cv", {"rsn": 1, "re": 0})
        path.unlink(missing_ok=True)
    print("PASS cycle/soak runtime admission integration")


def check_atomic_admission(server):
    # Long durations keep this independent of wall-clock progress while filling
    # the production Linux queue to its 255-slot boundary.
    program_id = server.get_json("jp")["nprogs"]
    packed = json.dumps([1, 127, 0, [0, 0, 0, 0], [1200, 1200, 0, 0, 0, 0, 0, 0]], separators=(",", ":"))
    assert server.get_json("cp", {"pid": -1, "v": packed, "name": "Atomic admission"})["result"] == 1
    try:
        for sid in range(8):
            assert server.get_json("cs", {"sid": sid, "cse": 1, "csc": 1200, "css": 60})["result"] == 1
        for _ in range(31):
            assert server.get_json("cr", {"t": "[1200,1200,1200,1200,1200,1200,1200,1200]", "qo": 0})["result"] == 1
        assert server.get_json("cr", {"t": "[1200,1200,1200,1200,1200,1200,0,0]", "qo": 0})["result"] == 1
        deadline = time.monotonic() + 3
        before = server.get_json("jc")
        while before["ps"][0][0] == 0 and time.monotonic() < deadline:
            time.sleep(0.05)
            before = server.get_json("jc")
        assert before["ps"][0][0] != 0, before
        assert before["nq"] == 254, before
        starts = [station[2] for station in before["ps"]]
        programs_before = server.get_json("jp")
        # Neither insertion nor the repeat-save side effect can precede admission.
        assert server.get_json("cr", {
            "t": "[1200,1200,0,0,0,0,0,0]", "qo": 1, "cnt": 2, "int": 10
        })["result"] == 0x30
        result = server.get_json("mp", {"pid": program_id, "qo": 1})
        assert result["result"] == 0x30, (result, server.get_json("jp"), server.get_json("jc"))
        after = server.get_json("jc")
        assert after["nq"] == 254 and after["qrej"] == before["qrej"] + 2, after
        assert [station[2] for station in after["ps"]] == starts
        assert server.get_json("jp") == programs_before
        assert server.get_json("cr", {"t": "[1200,0,0,0,0,0,0,0]", "qo": 0})["result"] == 1
        assert server.get_json("jc")["nq"] == 255
        assert len(server.get_json("jc")["cs"]) == 255
        assert server.get_json("cr", {"t": "[1200,0,0,0,0,0,0,0]", "qo": 0})["result"] == 0x30
        # Empty or malformed replacements do not discard accepted work.
        assert server.get_json("cr", {"t": "[0,0,0,0,0,0,0,0]", "qo": 2})["result"] == 0x10
        assert server.get_json("cr", {"t": "[oops]", "qo": 2})["result"] != 1
        assert server.get_json("jc")["nq"] == 255
        assert server.get_json("mp", {"pid": program_id, "qo": 2})["result"] == 1
        assert server.get_json("jc")["nq"] == 2
        # A successful repeated Run Once both persists its repeat and replaces the queue.
        assert server.get_json("cr", {
            "t": "[1200,0,0,0,0,0,0,0]", "cnt": 2, "int": 10, "qo": 2
        })["result"] == 1
        assert server.get_json("jc")["nq"] == 1
        assert server.get_json("jp")["nprogs"] == programs_before["nprogs"] + 1
    finally:
        server.get_json("cv", {"rsn": 1})
        server.get_json("dp", {"pid": -1})
        server.get_json("cs", {"csreset": 1})
    print("PASS atomic program/Run Once admission at queue capacity")


def check_analog_port_contract(server):
    before = server.get_json("jo")
    # Unsupported Analog mode must fail before changing another option.
    result = server.get_json("co", {"sn1t": 4, "wl": 77})
    assert result["result"] == 0x30 and result["item"] == "sn1t"
    assert server.get_json("jo")["wl"] == before["wl"]
    assert server.get_json("jo")["sn1t"] == before["sn1t"]
    assert server.get_json("co", {"sn1t": before["sn1t"]})["result"] == 1
    assert server.get_json("csn", {"sid": -1, "type": 5, "input": 0})["result"] == 0x30
    assert server.get_json("csn", {"sid": -1, "type": 5, "input": 4})["result"] == 17
    assert server.get_json("csn", {"sid": -1, "type": 4, "input": 3})["result"] == 17
    # A disabled onboard digital definition is still a stored reference.
    assert server.get_json("csn", {"sid": -1, "type": 4, "input": 0, "flag": 0})["result"] == 1
    assert server.get_json("co", {"sn1t": before["sn1t"]})["result"] == 1
    assert server.get_json("dsn", {"sid": -1})["result"] == 1
    print("PASS onboard analog capability and transactional mode validation")


def run_contract(server):
    checks = [
        ("jo", check_options),
        ("jc", check_controller),
        ("jn", check_stations),
        ("jp", check_programs),
        ("jsn", check_sensors),
        ("jsd", check_sensor_definitions),
        ("jpa", check_program_adjustments),
        ("ja", check_combined),
        ("db", check_debug),
    ]
    for endpoint, check in checks:
        check(server.get_json(endpoint))
        print(f"PASS /{endpoint}")
    check_request_bodies(server)
    check_control_commands(server)
    check_weather_failsafe(server)
    check_bundle_commands(server)
    check_sprinkler_logs(server)
    check_nested_option_encodings(server)
    check_analog_port_contract(server)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="./OpenSprinkler")
    parser.add_argument("--port", type=int, default=18080)
    args = parser.parse_args()

    binary = Path(args.binary).resolve()
    if not binary.is_file():
        parser.error(f"firmware binary not found: {binary}")

    with DemoServer(binary, args.port) as server:
        run_contract(server)
    with DemoServer(binary, args.port) as server:
        check_scoped_file_reads(server)
    with DemoServer(binary, args.port) as server:
        check_cycle_runtime(server)
    with DemoServer(binary, args.port) as server:
        check_atomic_admission(server)


if __name__ == "__main__":
    main()
