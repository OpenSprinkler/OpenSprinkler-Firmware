#!/usr/bin/env python3
"""Smoke-test the public JSON contract against a fresh Demo instance."""

import argparse
import base64
import hashlib
import http.server
import json
import socket
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
        self._start()
        return self

    def restart(self):
        """Stops the firmware and starts it again on the same data directory, as after a power cut."""
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self._start()

    def _start(self):
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
                return
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

    def post_json(self, endpoint, params=None, body=b""):
        query_params = {"pw": PASSWORD_HASH}
        if params:
            query_params.update(params)
        query = urllib.parse.urlencode(query_params)
        response = self.raw_http(
            (
                f"POST /{endpoint}?{query} HTTP/1.1\r\n"
                f"Host: 127.0.0.1\r\n"
                f"Content-Type: text/plain\r\n"
                f"Content-Length: {len(body)}\r\n"
                f"Connection: close\r\n\r\n"
            ).encode(),
            [(0, body)] if body else (),
        )
        assert response.startswith(b"HTTP/1.1 200"), response[:80]
        return json.loads(response.split(b"\r\n\r\n", 1)[1])

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
            "sn2of", "wimod", "reset", "dexp", "mexp", "ms", "smap",
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


def check_sitemap(server):
    def revision():
        return server.get_json("jo")["smap"]

    # OpenSprinkler v3/v4 leave the image to the apps ("loc":1); every controller reports its fingerprint
    device_image = "loc" in server.get_json("jm")

    def mapped(**values):
        expected = {"fp": ""}
        if device_image:
            expected["loc"] = 1
        expected.update(values)
        return expected

    assert server.get_json("jm") == mapped(rev=0, img=0, drw=0, pins=[])
    assert revision() == 0

    # Pins in the request body replace the stored set
    assert server.post_json("cmp", body=b"0,0,0;2,10000,5000;1,25,75")["result"] == 1
    assert server.get_json("jm") == mapped(rev=1, img=0, drw=0, pins=[[0, 0, 0], [2, 10000, 5000], [1, 25, 75]])
    assert revision() == 1
    assert server.post_json("cmp", body=b"0,10001,0")["result"] == 0x11
    assert server.post_json("cmp", body=b"250,1,1")["result"] == 0x11
    assert server.post_json("cmp", body=b"0,1")["result"] == 0x12
    assert server.post_json("cmp", body=b"0,1,1;0,2,2")["result"] == 0x12
    assert server.post_json("cmp")["result"] == 0x10
    assert server.get_json("jm")["rev"] == 1

    # Small maps can use the query string; an empty value clears the pins
    assert server.get_json("cmp", {"p": "3,1,2"})["result"] == 1
    assert server.get_json("jm")["pins"] == [[3, 1, 2]]

    if device_image:
        check_sitemap_device_image(server, revision)
        check_sitemap_relay(server)
    else:
        check_sitemap_controller_image(server)

    # Drawings are app-defined text, uploaded in ordered chunks
    drawings = "".join(f"h,{n % 3},{n},{n * 2},{n + 5},{n * 2 + 5}\n" for n in range(600)).encode()
    assert len(drawings) > 8192
    before = revision()
    chunk = 6000
    for offset in range(0, len(drawings), chunk):
        part = drawings[offset:offset + chunk]
        assert server.post_json("umd", {"off": offset, "tot": len(drawings)}, part)["result"] == 1
        if offset + len(part) < len(drawings):
            assert server.get_json("jm")["drw"] == 0
    assert server.get_json("jm")["drw"] == len(drawings)
    assert revision() == before + 1
    status, headers, body = server.get_response("jmd")
    assert status == 200 and headers.get_content_type() == "text/plain"
    assert body == drawings
    assert server.post_json("umd", {"off": 0, "tot": 4}, b"h\x01,1")["result"] == 0x12
    assert server.post_json("umd", {"off": 0, "tot": 4}, "h,é".encode())["result"] == 0x12
    assert server.post_json("umd", {"off": 5, "tot": 10}, b"h,1,1")["result"] == 0x11
    assert server.post_json("umd", {"off": 0, "tot": 999999}, b"h")["result"] == 0x11
    assert server.post_json("umd", {"off": 0, "tot": 3})["result"] == 0x10
    assert server.get_response("jmd")[2] == drawings
    assert server.post_json("umd", {"off": 0, "tot": 0})["result"] == 1
    assert server.get_json("jm")["drw"] == 0
    assert server.get_json("jmd")["result"] == 0x10

    # OpenThings Cloud doesn't pass on POST requests with a body, so chunks may arrive in the d parameter instead
    cloud_drawings = "".join(f"d,{n % 3},{n},1,2,3\n" for n in range(40)).encode()
    for offset in range(0, len(cloud_drawings), 200):
        part = cloud_drawings[offset:offset + 200].decode()
        assert server.get_json("umd", {"off": offset, "tot": len(cloud_drawings), "d": part})["result"] == 1
    assert server.get_response("jmd")[2] == cloud_drawings
    assert server.get_json("umd", {"off": 0, "tot": 3, "d": "h,\x01"})["result"] == 0x12

    small = b"d,1,10,20,30,40\n"
    assert server.post_json("umd", {"off": 0, "tot": len(small)}, small)["result"] == 1
    before = revision()
    assert server.get_json("dm")["result"] == 1
    assert server.get_json("jm") == mapped(rev=before + 1, img=0, drw=0, pins=[])
    if not device_image:
        assert server.get_json("jmi")["result"] == 0x10
    assert server.get_json("jmd")["result"] == 0x10
    print("PASS site map API" + (" (image kept by the apps)" if device_image else ""))


def image_fingerprint(image):
    return hashlib.sha256(image).hexdigest()[:32]


def check_sitemap_controller_image(server):
    # Image uploads arrive in ordered chunks and only replace the image when complete
    image = b"\xff\xd8" + bytes(range(256)) * 60 + b"\xff\xd9"
    chunk = 6000
    for offset in range(0, len(image), chunk):
        part = image[offset:offset + chunk]
        result = server.post_json("um", {"off": offset, "tot": len(image)}, base64.b64encode(part))
        assert result["result"] == 1, (offset, result)
        if offset + len(part) < len(image):
            assert server.get_json("jm")["img"] == 0
    mapped = server.get_json("jm")
    assert mapped["img"] == len(image) and mapped["pins"] == [[3, 1, 2]]
    status, headers, body = server.get_response("jmi")
    assert status == 200 and headers.get_content_type() == "image/jpeg"
    assert body == image

    # The controller fingerprints the image, so apps can tell whether the copy they have is still current
    assert mapped["fp"] == image_fingerprint(image)
    fingerprint_file = Path(server.temp_dir.name) / "smapfp.dat"
    assert fingerprint_file.read_text() == image_fingerprint(image)

    # An image stored before fingerprints existed gets one when the map is next read
    fingerprint_file.unlink()
    assert server.get_json("jm")["fp"] == image_fingerprint(image)
    assert fingerprint_file.read_text() == image_fingerprint(image)

    def encoded(data):
        return base64.b64encode(data)

    assert server.post_json("um", {"off": 10, "tot": 100}, encoded(b"abc"))["result"] == 0x11
    assert server.post_json("um", {"off": 0, "tot": 2}, encoded(b"abc"))["result"] == 0x11
    assert server.post_json("um", {"off": 0, "tot": 2 * 1024 * 1024}, encoded(b"ab"))["result"] == 0x11
    assert server.post_json("um", {"off": 0, "tot": 3})["result"] == 0x10
    assert server.post_json("um", {"off": 0, "tot": 3}, b"not base64!")["result"] == 0x12
    assert server.post_json("um", {"off": 0, "tot": 3}, encoded(b"GIF"))["result"] == 0x12
    assert server.get_response("jmi")[2] == image
    assert server.get_json("jm")["fp"] == image_fingerprint(image)

    # Through OpenThings Cloud the chunks arrive in the d parameter
    cloud_image = b"\xff\xd8" + bytes(range(256)) * 3 + b"\xff\xd9"
    for offset in range(0, len(cloud_image), 300):
        part = base64.b64encode(cloud_image[offset:offset + 300]).decode()
        assert server.get_json("um", {"off": offset, "tot": len(cloud_image), "d": part})["result"] == 1
    assert server.get_response("jmi")[2] == cloud_image
    assert server.get_json("jm")["fp"] == image_fingerprint(cloud_image)
    assert server.get_json("um", {"off": 0, "tot": 3})["result"] == 0x10

    # Setting a fingerprint and relaying the image are only for controllers that leave the image to the apps
    for endpoint in ("cmi", "mxr", "mxs", "mxu", "mxd", "mxa", "mxc"):
        assert route_status(server, endpoint, {"cid": "0000a001"}) == 404, endpoint


def check_sitemap_device_image(server, revision):
    # The controller has no room for the image, so it only stores the fingerprint and size the apps send
    fingerprint = "0123456789abcdef" * 2
    before = revision()
    assert server.get_json("cmi", {"fp": fingerprint, "sz": 123456})["result"] == 1
    assert server.get_json("jm") == {"loc": 1, "fp": fingerprint, "rev": before + 1, "img": 123456, "drw": 0,
        "pins": [[3, 1, 2]]}
    assert revision() == before + 1

    assert server.get_json("cmi", {"fp": "0123456789ABCDEF" * 2, "sz": 5})["result"] == 0x12
    assert server.get_json("cmi", {"fp": "0" * 31, "sz": 5})["result"] == 0x12
    assert server.get_json("cmi", {"fp": "g" * 32, "sz": 5})["result"] == 0x12
    assert server.get_json("cmi", {"fp": fingerprint, "sz": 0})["result"] == 0x12
    assert server.get_json("cmi", {"fp": "", "sz": 5})["result"] == 0x12
    assert server.get_json("cmi", {"fp": fingerprint, "sz": 64 * 1024 * 1024})["result"] == 0x11
    assert server.get_json("cmi", {"fp": fingerprint})["result"] == 0x11
    assert server.get_json("cmi", {"sz": 5})["result"] == 0x10
    assert server.get_json("jm")["fp"] == fingerprint and server.get_json("jm")["rev"] == before + 1

    # The image itself can't be uploaded or downloaded
    assert route_status(server, "um", {"off": 0, "tot": 3}) == 404
    assert route_status(server, "jmi") == 404

    # An empty fingerprint removes the image, keeping the pins
    assert server.get_json("cmi", {"fp": "", "sz": 0})["result"] == 1
    assert server.get_json("jm") == {"loc": 1, "fp": "", "rev": before + 2, "img": 0, "drw": 0, "pins": [[3, 1, 2]]}


# Built with short leases and an 8 KB window cap (see the test-api-sitemap-device-image make target)
RELAY_SENDER_LEASE = 1.0
RELAY_RECEIVER_LEASE = 2.0
RELAY_WINDOW = 6144


def check_sitemap_relay(server):
    image = bytes((n * 7 + 3) % 256 for n in range(20000))
    fingerprint = image_fingerprint(image)
    assert server.get_json("cmi", {"fp": fingerprint, "sz": len(image)})["result"] == 1
    receiver, second_receiver = "0000b001", "0000b002"
    sender, second_sender = "0000a001", "0000a002"

    def relay(endpoint, client, **params):
        return server.get_json(endpoint, dict(params, cid=client))

    def summary():
        return server.get_json("jo")["smx"]

    def fill(client, in_url=False):
        claimed = relay("mxs", client, fp=fingerprint)
        assert claimed["result"] == 1 and claimed["st"] == 2, claimed
        position = claimed["off"] + claimed["len"]
        end = min(claimed["off"] + claimed["win"], len(image))
        # The local web server limits the URL to 1 KB, so pieces sent the OpenThings Cloud way are kept small here
        size = 300 if in_url else claimed["req"]
        while position < end:
            piece = image[position:min(end, position + size)]
            encoded = base64.b64encode(piece)
            params = {"cid": client, "off": position}
            if in_url:
                result = server.get_json("mxu", dict(params, d=encoded.decode()))
            else:
                result = server.post_json("mxu", params, encoded)
            assert result["result"] == 1, result
            position += len(piece)
        assert relay("mxs", client, fp=fingerprint)["st"] == 3

    def drain(client, received):
        ready = relay("mxr", client, fp=fingerprint)
        assert ready["result"] == 1 and ready["st"] == 3 and ready["pos"] == 0, ready
        assert ready["off"] == len(received)
        status, headers, body = server.get_response("mxd", {"cid": client})
        assert status == 200 and headers.get_content_type() == "application/octet-stream"
        assert len(body) == ready["len"]
        received.extend(body)
        assert relay("mxa", client, off=ready["off"] + ready["len"])["result"] == 1

    # Nobody is waiting, so there's nothing to send
    assert summary() == 0
    assert relay("mxs", sender, fp=fingerprint)["result"] == 0x10

    # Receivers queue up
    assert relay("mxr", receiver, fp=fingerprint) == {"result": 1, "st": 1, "pos": 0, "off": 0, "len": 0,
        "tot": len(image)}
    assert summary() == 1
    assert relay("mxr", second_receiver, fp=fingerprint)["pos"] == 1
    assert relay("mxr", receiver, fp="0" * 32)["result"] == 0x03
    assert relay("mxd", receiver)["result"] == 0x11

    # The first app with the image to claim the transfer sends it; others are turned away
    claimed = relay("mxs", sender, fp=fingerprint)
    assert claimed == {"result": 1, "st": 2, "off": 0, "len": 0, "win": RELAY_WINDOW, "req": 6144, "tot": len(image)}
    assert summary() == 2
    assert relay("mxs", second_sender, fp=fingerprint)["result"] == 0x30
    assert relay("mxs", second_sender, fp="0" * 32)["result"] == 0x03
    assert server.post_json("mxu", {"cid": second_sender, "off": 0}, base64.b64encode(image[:3]))["result"] == 0x30
    assert server.post_json("mxu", {"cid": sender, "off": 3}, base64.b64encode(image[:3]))["result"] == 0x11
    assert server.post_json("mxu", {"cid": sender, "off": 0}, b"not base64!")["result"] == 0x12
    assert server.post_json("mxu", {"cid": sender, "off": 0})["result"] == 0x10
    assert relay("mxd", second_receiver)["result"] == 0x30

    received = bytearray()
    while len(received) < len(image):
        fill(sender, in_url=len(received) % 2 == 0)
        drain(receiver, received)
    assert bytes(received) == image

    # The next receiver is served from the start; its sender stops partway and another app carries on
    assert relay("mxr", second_receiver, fp=fingerprint)["pos"] == 0
    received = bytearray()
    fill(sender)
    drain(second_receiver, received)
    time.sleep(RELAY_SENDER_LEASE + 0.3)
    waiting = relay("mxr", second_receiver, fp=fingerprint)
    assert waiting["st"] == 1 and waiting["off"] == RELAY_WINDOW and summary() == 1
    assert relay("mxs", second_sender, fp=fingerprint)["off"] == RELAY_WINDOW
    while len(received) < len(image):
        fill(second_sender)
        drain(second_receiver, received)
    assert bytes(received) == image
    assert summary() == 0
    assert relay("mxs", second_sender, fp=fingerprint)["result"] == 0x10

    # A receiver that cancels, or stops asking, gives up its place
    relay("mxr", receiver, fp=fingerprint)
    assert relay("mxc", receiver)["result"] == 1 and summary() == 0
    relay("mxr", receiver, fp=fingerprint)
    time.sleep(RELAY_RECEIVER_LEASE + 0.3)
    assert summary() == 0

    # Changing the image ends the transfer
    relay("mxr", receiver, fp=fingerprint)
    fill(sender)
    other = image[::-1]
    assert server.get_json("cmi", {"fp": image_fingerprint(other), "sz": len(other)})["result"] == 1
    assert summary() == 0
    assert relay("mxr", receiver, fp=fingerprint)["result"] == 0x03
    assert relay("mxs", sender, fp=fingerprint)["result"] == 0x03
    assert relay("mxd", receiver)["result"] == 0x30

    # Requests need a client id (DEMO builds don't check the password)
    assert server.get_json("mxr", {"fp": image_fingerprint(other)})["result"] == 0x10
    assert relay("mxr", "xyz", fp=image_fingerprint(other))["result"] == 0x12
    assert relay("mxr", "123456789", fp=image_fingerprint(other))["result"] == 0x12
    assert relay("mxr", "00000000", fp=image_fingerprint(other))["result"] == 0x12
    print("PASS site map image relay")


def check_sitemap_relay_restart(server):
    # A transfer interrupted by a restart doesn't leave its window on flash
    data = Path(server.temp_dir.name)
    image = bytes(range(256)) * 40
    fingerprint = image_fingerprint(image)
    assert server.get_json("cmi", {"fp": fingerprint, "sz": len(image)})["result"] == 1
    assert server.get_json("mxr", {"cid": "0000b001", "fp": fingerprint})["result"] == 1
    assert server.get_json("mxs", {"cid": "0000a001", "fp": fingerprint})["result"] == 1
    assert server.post_json("mxu", {"cid": "0000a001", "off": 0}, base64.b64encode(image[:3000]))["result"] == 1
    assert (data / "smapxfr.tmp").exists()

    server.restart()
    assert not (data / "smapxfr.tmp").exists()
    assert server.get_json("jo")["smx"] == 0
    assert server.get_json("jm")["fp"] == fingerprint
    print("PASS site map relay restart")


def route_status(server, endpoint, params=None):
    try:
        return server.get_response(endpoint, params)[0]
    except urllib.error.HTTPError as error:
        return error.code


def check_sitemap_resent_chunks(server):
    # A chunk is resent when its reply was lost; the controller accepts it again instead of failing the upload
    def revision():
        return server.get_json("jo")["smap"]

    image = b"\xff\xd8" + bytes(range(256)) * 20 + b"\xff\xd9"
    parts = [(offset, image[offset:offset + 2000]) for offset in range(0, len(image), 2000)]
    assert len(parts) == 3

    def upload(offset, part):
        return server.post_json("um", {"off": offset, "tot": len(image)}, base64.b64encode(part))["result"]

    before = revision()
    assert upload(*parts[0]) == 1
    assert upload(*parts[1]) == 1
    assert upload(*parts[1]) == 1
    assert upload(parts[1][0], b"x" * len(parts[1][1])) == 0x11
    assert upload(parts[2][0] + 1, parts[2][1][1:]) == 0x11

    # Chunks out of order are still rejected
    assert upload(*parts[0]) == 1
    assert upload(*parts[2]) == 0x11
    assert upload(*parts[1]) == 1
    assert server.get_json("jm")["img"] == 0
    assert upload(*parts[2]) == 1
    assert revision() == before + 1

    # The last chunk again, after the upload completed, doesn't count as another change
    assert upload(*parts[2]) == 1
    assert revision() == before + 1
    assert server.get_json("jm")["img"] == len(image)
    assert server.get_response("jmi")[2] == image
    assert upload(parts[2][0], b"x" * len(parts[2][1])) == 0x11
    assert upload(*parts[1]) == 0x11
    assert revision() == before + 1

    drawings = "".join(f"d,{n % 3},{n},1,2,3\n" for n in range(300)).encode()
    pieces = [(offset, drawings[offset:offset + 1500]) for offset in range(0, len(drawings), 1500)]
    assert len(pieces) > 2
    before = revision()
    for index, (offset, piece) in enumerate(pieces):
        params = {"off": offset, "tot": len(drawings)}
        assert server.post_json("umd", params, piece)["result"] == 1
        assert server.post_json("umd", params, piece)["result"] == 1, index
        assert revision() == before + (1 if index == len(pieces) - 1 else 0)
    assert server.get_response("jmd")[2] == drawings
    last_offset, last_piece = pieces[-1]
    assert server.get_json("umd", {"off": last_offset, "tot": len(drawings), "d": last_piece.decode()})["result"] == 1
    assert revision() == before + 1
    print("PASS site map resent chunks")


def check_sitemap_drawings_removal(server):
    # tot=0 removes the drawings only without data; with data it is out of range like any chunk past tot
    drawings = b"h,1,10,20,30,40\n"
    assert server.post_json("umd", {"off": 0, "tot": len(drawings)}, drawings)["result"] == 1
    before = server.get_json("jo")["smap"]
    assert server.post_json("umd", {"off": 0, "tot": 0}, b"h,1,2,3")["result"] == 0x11
    assert server.get_json("umd", {"off": 0, "tot": 0, "d": "h,1,2,3"})["result"] == 0x11
    assert server.get_response("jmd")[2] == drawings
    assert server.get_json("jo")["smap"] == before
    assert server.get_json("umd", {"off": 0, "tot": 0})["result"] == 1
    assert server.get_json("jmd")["result"] == 0x10
    print("PASS site map drawings removal")


def check_sitemap_unreadable_files(server):
    # Files that are gone answer with a result code instead of an empty response
    data = Path(server.temp_dir.name)
    image = b"\xff\xd8" + bytes(range(256)) + b"\xff\xd9"
    drawings = b"d,0,1,2,3,4\n"
    assert server.post_json("um", {"off": 0, "tot": len(image)}, base64.b64encode(image))["result"] == 1
    assert server.post_json("umd", {"off": 0, "tot": len(drawings)}, drawings)["result"] == 1
    (data / "smapimg.dat").unlink()
    (data / "smapdrw.dat").unlink()
    assert server.get_json("jmi")["result"] == 0x10
    assert server.get_json("jmd")["result"] == 0x10
    assert server.get_json("dm")["result"] == 1
    print("PASS site map unreadable files")


def check_sitemap_restart(server):
    # Uploads abandoned by a restart don't leave their temporary files on flash
    data = Path(server.temp_dir.name)
    assert server.post_json("cmp", body=b"1,20,30")["result"] == 1
    assert not (data / "smap.tmp").exists()
    image = b"\xff\xd8" + bytes(range(256)) * 4
    assert server.post_json("um", {"off": 0, "tot": len(image) + 100}, base64.b64encode(image))["result"] == 1
    assert server.post_json("umd", {"off": 0, "tot": 100}, b"h,1,2,3")["result"] == 1
    assert (data / "smapimg.tmp").exists() and (data / "smapdrw.tmp").exists()
    (data / "smap.tmp").write_bytes(b"\0" * 10)
    mapped = server.get_json("jm")

    server.restart()
    for name in ("smapimg.tmp", "smapdrw.tmp", "smap.tmp"):
        assert not (data / name).exists(), name
    assert server.get_json("jm") == mapped
    assert server.post_json("umd", {"off": 7, "tot": 100}, b"h,1,2,3")["result"] == 0x11
    print("PASS site map restart")


def check_debug(value):
    require_keys("db", value, ["sprlog_fail", "sprlog_mismatch"])
    assert value["sprlog_fail"] == 0
    assert value["sprlog_mismatch"] == 0


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
    check_sitemap(server)
    device_image = "loc" in server.get_json("jm")
    if not device_image:
        check_sitemap_resent_chunks(server)
    check_sitemap_drawings_removal(server)
    if not device_image:
        check_sitemap_unreadable_files(server)
    check_nested_option_encodings(server)
    if device_image:
        check_sitemap_relay_restart(server)
    else:
        check_sitemap_restart(server)


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


if __name__ == "__main__":
    main()
