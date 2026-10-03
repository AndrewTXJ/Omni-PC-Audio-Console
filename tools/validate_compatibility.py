#!/usr/bin/env python3
"""Validate the device compatibility database.

Enforces the integrity rules in data/compatibility/SCHEMA.md. The point is to
stop the list acquiring claims nobody measured: a compatibility database that
overstates support is worse than none, because users buy hardware on it.

Standard library only, so it runs anywhere and prejudges nothing about the
engine language (ADR-0003).

Usage:
    python3 tools/validate_compatibility.py [path/to/devices.toml]

Exit status 0 if the database is valid, 1 otherwise.
"""

from __future__ import annotations

import datetime as dt
import sys
import tomllib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DB = REPO_ROOT / "data" / "compatibility" / "devices.toml"

CONNECTIONS = {
    "usb2", "usb3", "thunderbolt", "pcie", "firewire", "bluetooth", "onboard",
}
STATUSES = {"supported", "partial", "unsupported", "untested"}
BACKENDS = {
    "alsa", "pipewire", "pulseaudio", "jack",
    "wasapi_shared", "wasapi_exclusive", "asio",
}
SETTINGS = {"lowest", "balanced", "safe"}
FORMATS = {"S16_LE", "S24_3LE", "S24_LE", "S32_LE"}

# Roadmap 7.6 fixes the project sample rates.
RATES = {44100, 48000, 88200, 96000, 192000}

REQUIRED_DEVICE = ("vendor", "model", "connection", "status")
REQUIRED_TESTED = ("date", "kernel", "sound_server")
REQUIRED_MEASUREMENT = (
    "backend", "period_frames", "round_trip_ms", "kernel", "sound_server",
)


class Report:
    """Collects problems so one run reports everything, not just the first."""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, where: str, message: str) -> None:
        self.errors.append(f"{where}: {message}")

    def warn(self, where: str, message: str) -> None:
        self.warnings.append(f"{where}: {message}")


def check_date(report: Report, where: str, field: str, value: object) -> None:
    if isinstance(value, dt.date):
        return  # TOML parsed it as a native date, which is fine.
    if not isinstance(value, str):
        report.error(where, f"{field} must be a YYYY-MM-DD string, got {type(value).__name__}")
        return
    try:
        dt.date.fromisoformat(value)
    except ValueError:
        report.error(where, f"{field} is not a valid YYYY-MM-DD date: {value!r}")


def check_enum(
    report: Report, where: str, field: str, value: object, allowed: set[str]
) -> None:
    if value not in allowed:
        report.error(
            where,
            f"{field} must be one of {sorted(allowed)}, got {value!r}",
        )


def check_tested(report: Report, where: str, tested: object) -> None:
    if not isinstance(tested, dict):
        report.error(where, "[device.tested] must be a table")
        return
    for field in REQUIRED_TESTED:
        value = tested.get(field)
        if value is None or value == "":
            report.error(where, f"[device.tested] is missing {field}")
    if "date" in tested and tested["date"] not in ("", "YYYY-MM-DD"):
        check_date(report, where, "tested.date", tested["date"])


def check_measurement(
    report: Report, where: str, index: int, m: object
) -> None:
    mwhere = f"{where} measurement[{index}]"
    if not isinstance(m, dict):
        report.error(mwhere, "must be a table")
        return

    for field in REQUIRED_MEASUREMENT:
        value = m.get(field)
        if value is None or value == "":
            report.error(mwhere, f"missing required field {field}")

    if "backend" in m:
        check_enum(report, mwhere, "backend", m["backend"], BACKENDS)
    if "setting" in m and m["setting"] is not None:
        check_enum(report, mwhere, "setting", m["setting"], SETTINGS)

    period = m.get("period_frames")
    if isinstance(period, int) and period <= 0:
        report.error(mwhere, f"period_frames must be positive, got {period}")

    rtt = m.get("round_trip_ms")
    if isinstance(rtt, (int, float)) and rtt <= 0:
        report.error(mwhere, f"round_trip_ms must be positive, got {rtt}")

    # QS-02: the engine must add no periods. A recorded figure above 0 is a
    # failing measurement, so it is surfaced rather than quietly stored.
    added = m.get("engine_added_periods")
    if isinstance(added, int) and added > 0:
        report.warn(
            mwhere,
            f"engine_added_periods is {added}, but QS-02 requires 0 — "
            "this records a failing result",
        )

    thd = m.get("thd_n_db")
    if isinstance(thd, (int, float)):
        if thd > 0:
            report.error(mwhere, f"thd_n_db should be negative dB, got {thd}")
        elif thd > -120:
            report.warn(
                mwhere,
                f"thd_n_db of {thd} does not meet the QS-03 target of -120 dB "
                "(may be converter-limited — note it)",
            )

    xruns = m.get("xruns_24h")
    if isinstance(xruns, int) and xruns > 0:
        report.warn(
            mwhere,
            f"xruns_24h is {xruns}, but QS-08 requires zero at the default setting",
        )


def check_device(report: Report, index: int, d: object) -> tuple[str, str] | None:
    where = f"device[{index}]"
    if not isinstance(d, dict):
        report.error(where, "must be a table")
        return None

    for field in REQUIRED_DEVICE:
        value = d.get(field)
        if value is None or value == "":
            report.error(where, f"missing required field {field}")

    vendor = d.get("vendor", "")
    model = d.get("model", "")
    if vendor and model:
        where = f"device[{index}] {vendor} {model}"

    if "connection" in d:
        check_enum(report, where, "connection", d["connection"], CONNECTIONS)
    status = d.get("status")
    if status is not None:
        check_enum(report, where, "status", status, STATUSES)

    for field in ("rates",):
        values = d.get(field)
        if values is not None:
            if not isinstance(values, list):
                report.error(where, f"{field} must be an array")
            else:
                for r in values:
                    if r not in RATES:
                        report.warn(
                            where,
                            f"rate {r} is outside the project rates {sorted(RATES)} "
                            "(roadmap 7.6) — intentional?",
                        )

    formats = d.get("formats")
    if formats is not None:
        if not isinstance(formats, list):
            report.error(where, "formats must be an array")
        else:
            for f in formats:
                if f not in FORMATS:
                    report.error(
                        where,
                        f"format {f!r} is not one of {sorted(FORMATS)} (roadmap 4.8)",
                    )

    for field in ("quirks", "alsa_controls", "sources"):
        if field in d and not isinstance(d[field], list):
            report.error(where, f"{field} must be an array")

    tested = d.get("tested")
    measurements = d.get("measurement", [])
    if measurements and not isinstance(measurements, list):
        report.error(where, "[[device.measurement]] must be an array of tables")
        measurements = []

    # Rule 1: supported requires evidence.
    if status == "supported":
        if not measurements:
            report.error(
                where,
                'status is "supported" but there is no [[device.measurement]] — '
                "a device cannot be called supported without a measurement",
            )
        if tested is None:
            report.error(
                where,
                'status is "supported" but there is no [device.tested] block',
            )

    # Rule 2: unsupported requires a cause.
    if status == "unsupported" and not d.get("reason"):
        report.error(
            where,
            'status is "unsupported" but no reason is given — '
            "an unexplained failure cannot be rechecked",
        )

    # An untested device claiming measurements is a contradiction.
    if status == "untested" and measurements:
        report.error(
            where,
            'status is "untested" but measurements are present — '
            "set the status to match the evidence",
        )

    if tested is not None:
        check_tested(report, where, tested)
    for i, m in enumerate(measurements):
        check_measurement(report, where, i, m)

    if not vendor or not model:
        return None
    return (str(vendor), str(model))


def main(argv: list[str]) -> int:
    path = Path(argv[1]) if len(argv) > 1 else DEFAULT_DB
    if not path.is_file():
        print(f"error: database not found: {path}", file=sys.stderr)
        return 1

    try:
        with path.open("rb") as handle:
            data = tomllib.load(handle)
    except tomllib.TOMLDecodeError as exc:
        print(f"error: {path} is not valid TOML: {exc}", file=sys.stderr)
        return 1

    report = Report()
    devices = data.get("device", [])
    if not isinstance(devices, list):
        print("error: top level must contain an array of [[device]] tables",
              file=sys.stderr)
        return 1

    seen: dict[tuple[str, str], int] = {}
    for index, device in enumerate(devices):
        key = check_device(report, index, device)
        # Rule 4: vendor plus model is unique.
        if key is not None:
            if key in seen:
                report.error(
                    f"device[{index}] {key[0]} {key[1]}",
                    f"duplicates device[{seen[key]}]",
                )
            else:
                seen[key] = index

    for warning in report.warnings:
        print(f"warning: {warning}")
    for error in report.errors:
        print(f"error: {error}", file=sys.stderr)

    counts: dict[str, int] = {}
    for device in devices:
        if isinstance(device, dict):
            status = str(device.get("status", "?"))
            counts[status] = counts.get(status, 0) + 1
    summary = ", ".join(f"{k}: {v}" for k, v in sorted(counts.items())) or "none"

    if report.errors:
        print(
            f"\n{len(report.errors)} error(s), {len(report.warnings)} warning(s) "
            f"in {len(devices)} device(s) — {summary}",
            file=sys.stderr,
        )
        return 1

    print(
        f"{path.name}: {len(devices)} device(s) valid "
        f"({summary}), {len(report.warnings)} warning(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
