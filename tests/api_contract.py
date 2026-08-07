#!/usr/bin/env python3
"""Smoke-test the public JSON contract against a fresh Demo instance."""

import argparse
import json
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


PASSWORD_HASH = "a6d82bced638de3def1e9bbb4983225c"


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

    def get_json(self, endpoint):
        query = urllib.parse.urlencode({"pw": PASSWORD_HASH})
        url = f"http://127.0.0.1:{self.port}/{endpoint}?{query}"
        with urllib.request.urlopen(url, timeout=3) as response:
            return json.load(response)


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
            "email", "wls", "sbits", "ps", "gpio",
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
            "snames", "maxlen",
        ],
    )
    assert len(data["snames"]) == len(data["stn_grp"])


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
    require_keys("/ja.status", data["status"], ["sn", "nstations"])
    assert len(data["status"]["sn"]) == data["status"]["nstations"]


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
    ]
    for endpoint, check in checks:
        check(server.get_json(endpoint))
        print(f"PASS /{endpoint}")


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
