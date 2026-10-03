#!/usr/bin/env python3
"""Validate the device compatibility database.

Enforces the integrity rules in data/compatibility/SCHEMA.md. The point is to
stop the list acquiring claims nobody measured: a compatibility database that
overstates support is worse than none, because users buy hardware on it.

That purpose dictates the failure posture: a value of the wrong type is an
ERROR, never a skipped check. An earlier version guarded each check with
isinstance() and so silently accepted `period_frames = "fast"` -- a false pass,
which is the one outcome this tool exists to prevent.

Standard library only, so it runs anywhere and prejudges nothing about the
engine language (ADR-0003).

Usage:
    python3 tools/validate_compatibility.py [path/to/devices.toml]

Exit status 0 if the database is valid, 1 otherwise.
"""

from __future__ import annotations

import datetime as dt
import sys
from pathlib import Path

# tomllib is standard library from 3.11. Ubuntu 22.04 LTS still ships 3.10 as
# python3, so fail with an actionable message rather than an ImportError.
if sys.version_info < (3, 11):
    sys.stderr.write(
        "error: this tool needs Python 3.11 or newer for tomllib "
        f"(running {sys.version_info.major}.{sys.version_info.minor}).\n"
        "       Install a newer python3, or `pip install tomli` and edit the "
        "import below.\n"
    )
    raise SystemExit(1)

import tomllib  # noqa: E402  (deliberately after the version guard)

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

# Two different things, deliberately kept apart.
#
# PROJECT_RATES is what roadmap 7.6 fixes: the rates a session can RUN at.
# A device's `rates` field is a different claim -- what the hardware OFFERS --
# and checking one against the other is a category error: a card supporting
# 32 kHz is not a data problem just because no session renders at 32 kHz.
# So device rates are checked for plausibility against real audio rates, and
# only noted (never failed) when the project cannot render at them.
PROJECT_RATES = {44100, 48000, 88200, 96000, 176400, 192000}
KNOWN_AUDIO_RATES = {
    8000, 11025, 16000, 22050, 32000, 37800, 44056, 44100, 47250, 48000,
    64000, 88200, 96000, 176400, 192000, 352800, 384000, 705600, 768000,
}

REQUIRED_DEVICE = ("vendor", "model", "connection", "status")
REQUIRED_TESTED = ("date", "kernel", "sound_server")
REQUIRED_MEASUREMENT = (
    "backend", "period_frames", "round_trip_ms", "kernel", "sound_server",
)

# Unknown keys are rejected: a typo such as `round_trip_ms_` would otherwise
# drop a measurement's headline figure without a word.
DEVICE_KEYS = {
    "vendor", "model", "connection", "status", "driver", "class_compliant",
    "channels_in", "channels_out", "rates", "formats", "reason", "workaround",
    "quirks", "alsa_controls", "notes", "sources", "tested", "measurement",
}
TESTED_KEYS = {"date", "kernel", "sound_server", "distro", "tester"}
MEASUREMENT_KEYS = {
    "backend", "setting", "period_frames", "round_trip_ms",
    "engine_added_periods", "xruns_24h", "thd_n_db", "kernel", "sound_server",
    "notes",
}

STRING_FIELDS = {
    "vendor", "model", "connection", "status", "driver", "reason",
    "workaround", "notes",
}
INT_FIELDS = {"channels_in", "channels_out"}
BOOL_FIELDS = {"class_compliant"}
STRING_ARRAY_FIELDS = {"formats", "quirks", "alsa_controls", "sources"}


class Report:
    """Collects problems so one run reports everything, not just the first."""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, where: str, message: str) -> None:
        self.errors.append(f"{where}: {message}")

    def warn(self, where: str, message: str) -> None:
        self.warnings.append(f"{where}: {message}")


def type_name(value: object) -> str:
    return type(value).__name__


def check_unknown_keys(
    report: Report, where: str, table: dict, allowed: set[str], label: str
) -> None:
    for key in sorted(table):
        if key not in allowed:
            report.error(
                where,
                f"unknown {label} field {key!r} — check the spelling against "
                "SCHEMA.md; unknown fields are rejected so a typo cannot drop data",
            )


def check_required_string(
    report: Report, where: str, field: str, value: object, *, label: str = ""
) -> bool:
    """A required string must be present, a string, and not blank."""
    prefix = f"{label}{field}" if label else field
    if value is None:
        report.error(where, f"missing required field {prefix}")
        return False
    if not isinstance(value, str):
        report.error(where, f"{prefix} must be a string, got {type_name(value)}")
        return False
    if not value.strip():
        report.error(where, f"{prefix} is required but blank")
        return False
    return True


def check_date(report: Report, where: str, field: str, value: object) -> None:
    """Dates must be real ISO dates. The template placeholder is not one.

    An earlier version whitelisted the literal "YYYY-MM-DD" so an unfilled
    template would pass, which let a device be published as supported with no
    test date at all. The exemption is gone on purpose.
    """
    if isinstance(value, dt.date):
        return  # TOML parsed a native date, which is fine.
    if not isinstance(value, str):
        report.error(
            where, f"{field} must be a YYYY-MM-DD string, got {type_name(value)}"
        )
        return
    if value.strip() in ("", "YYYY-MM-DD"):
        report.error(
            where,
            f"{field} is still the template placeholder ({value!r}) — "
            "fill in the real date the measurement was taken",
        )
        return
    try:
        dt.date.fromisoformat(value)
    except ValueError:
        report.error(where, f"{field} is not a valid YYYY-MM-DD date: {value!r}")


def check_enum(
    report: Report, where: str, field: str, value: object, allowed: set[str]
) -> None:
    # Type first: a list or table here is unhashable, and testing membership
    # against a set would raise TypeError and abort the whole run, losing every
    # error collected so far.
    if not isinstance(value, str):
        report.error(where, f"{field} must be a string, got {type_name(value)}")
        return
    if value not in allowed:
        report.error(
            where, f"{field} must be one of {sorted(allowed)}, got {value!r}"
        )


def check_number(
    report: Report,
    where: str,
    field: str,
    value: object,
    *,
    integer: bool = False,
    positive: bool = False,
) -> bool:
    if isinstance(value, bool):  # bool is an int subclass; never a measurement.
        report.error(where, f"{field} must be a number, got bool")
        return False
    if integer:
        if not isinstance(value, int):
            report.error(where, f"{field} must be an integer, got {type_name(value)}")
            return False
    elif not isinstance(value, (int, float)):
        report.error(where, f"{field} must be a number, got {type_name(value)}")
        return False
    if positive and value <= 0:
        report.error(where, f"{field} must be positive, got {value}")
        return False
    return True


def check_string_array(
    report: Report, where: str, field: str, value: object
) -> list[str]:
    if not isinstance(value, list):
        report.error(where, f"{field} must be an array, got {type_name(value)}")
        return []
    out = []
    for i, item in enumerate(value):
        if not isinstance(item, str):
            report.error(
                where, f"{field}[{i}] must be a string, got {type_name(item)}"
            )
        else:
            out.append(item)
    return out


def check_tested(report: Report, where: str, tested: object) -> None:
    if not isinstance(tested, dict):
        report.error(
            where, f"[device.tested] must be a table, got {type_name(tested)}"
        )
        return
    check_unknown_keys(report, where, tested, TESTED_KEYS, "device.tested")
    for field in REQUIRED_TESTED:
        check_required_string(
            report, where, field, tested.get(field), label="tested."
        )
    if "date" in tested:
        check_date(report, where, "tested.date", tested["date"])
    for field in ("distro", "tester"):
        if field in tested and not isinstance(tested[field], str):
            report.error(
                where,
                f"tested.{field} must be a string, got {type_name(tested[field])}",
            )


def check_measurement(
    report: Report, where: str, index: int, m: object, device_status: object
) -> None:
    mwhere = f"{where} measurement[{index}]"
    if not isinstance(m, dict):
        report.error(mwhere, f"must be a table, got {type_name(m)}")
        return

    check_unknown_keys(report, mwhere, m, MEASUREMENT_KEYS, "measurement")

    # Rule 5: a measurement may not restate the device's status. Catching it
    # here is what makes the documented rule real rather than aspirational.
    if "status" in m:
        report.error(
            mwhere,
            "measurements must not carry a status field — status belongs to the "
            f"device (this device is {device_status!r}); see SCHEMA.md rule 5",
        )

    for field in ("backend", "kernel", "sound_server"):
        check_required_string(report, mwhere, field, m.get(field))
    for field in ("period_frames", "round_trip_ms"):
        if field not in m:
            report.error(mwhere, f"missing required field {field}")

    if isinstance(m.get("backend"), str):
        check_enum(report, mwhere, "backend", m["backend"], BACKENDS)
    if m.get("setting") is not None:
        check_enum(report, mwhere, "setting", m["setting"], SETTINGS)

    if "period_frames" in m:
        check_number(
            report, mwhere, "period_frames", m["period_frames"],
            integer=True, positive=True,
        )
    if "round_trip_ms" in m:
        check_number(
            report, mwhere, "round_trip_ms", m["round_trip_ms"], positive=True
        )

    # QS-02: the engine must add no periods. A recorded figure above 0 is a
    # failing measurement, so it is surfaced rather than quietly stored.
    if "engine_added_periods" in m:
        added = m["engine_added_periods"]
        if check_number(report, mwhere, "engine_added_periods", added, integer=True):
            if added < 0:
                report.error(
                    mwhere, f"engine_added_periods cannot be negative, got {added}"
                )
            elif added > 0:
                report.warn(
                    mwhere,
                    f"engine_added_periods is {added}, but QS-02 requires 0 — "
                    "this records a failing result",
                )

    if "thd_n_db" in m:
        thd = m["thd_n_db"]
        if check_number(report, mwhere, "thd_n_db", thd):
            if thd > 0:
                report.error(mwhere, f"thd_n_db should be negative dB, got {thd}")
            elif thd > -120:
                report.warn(
                    mwhere,
                    f"thd_n_db of {thd} does not meet the QS-03 target of -120 dB "
                    "(may be converter-limited — note it)",
                )

    if "xruns_24h" in m:
        xruns = m["xruns_24h"]
        if check_number(report, mwhere, "xruns_24h", xruns, integer=True):
            if xruns < 0:
                report.error(mwhere, f"xruns_24h cannot be negative, got {xruns}")
            elif xruns > 0:
                report.warn(
                    mwhere,
                    f"xruns_24h is {xruns}, but QS-08 requires zero at the "
                    "default setting",
                )

    if "notes" in m and not isinstance(m["notes"], str):
        report.error(mwhere, f"notes must be a string, got {type_name(m['notes'])}")


def check_device(report: Report, index: int, d: object) -> tuple[str, str] | None:
    where = f"device[{index}]"
    if not isinstance(d, dict):
        report.error(where, f"must be a table, got {type_name(d)}")
        return None

    vendor = d.get("vendor")
    model = d.get("model")
    if isinstance(vendor, str) and isinstance(model, str) and vendor and model:
        where = f"device[{index}] {vendor} {model}"

    check_unknown_keys(report, where, d, DEVICE_KEYS, "device")

    for field in REQUIRED_DEVICE:
        check_required_string(report, where, field, d.get(field))

    if isinstance(d.get("connection"), str):
        check_enum(report, where, "connection", d["connection"], CONNECTIONS)
    status = d.get("status")
    if isinstance(status, str):
        check_enum(report, where, "status", status, STATUSES)

    for field in STRING_FIELDS - set(REQUIRED_DEVICE):
        if field in d and not isinstance(d[field], str):
            report.error(
                where, f"{field} must be a string, got {type_name(d[field])}"
            )
    for field in INT_FIELDS:
        if field in d:
            check_number(report, where, field, d[field], integer=True)
    for field in BOOL_FIELDS:
        if field in d and not isinstance(d[field], bool):
            report.error(
                where, f"{field} must be true or false, got {type_name(d[field])}"
            )
    for field in STRING_ARRAY_FIELDS - {"formats"}:
        if field in d:
            check_string_array(report, where, field, d[field])

    if "rates" in d:
        rates = d["rates"]
        if not isinstance(rates, list):
            report.error(where, f"rates must be an array, got {type_name(rates)}")
        else:
            for i, r in enumerate(rates):
                if isinstance(r, bool) or not isinstance(r, int):
                    report.error(
                        where, f"rates[{i}] must be an integer, got {type_name(r)}"
                    )
                elif r not in KNOWN_AUDIO_RATES:
                    # Implausible as a hardware rate at all -- most likely a typo
                    # such as 4800 for 48000, which would otherwise be published.
                    report.error(
                        where,
                        f"rates[{i}] = {r} is not a known audio sample rate; "
                        "check for a typo",
                    )
                elif r not in PROJECT_RATES:
                    report.warn(
                        where,
                        f"the device offers {r} Hz, which is outside the project "
                        f"rates {sorted(PROJECT_RATES)} (roadmap 7.6) — fine to "
                        "record, but no session will run at it",
                    )

    if "formats" in d:
        for f in check_string_array(report, where, "formats", d["formats"]):
            if f not in FORMATS:
                report.error(
                    where,
                    f"format {f!r} is not one of {sorted(FORMATS)} (roadmap 4.8)",
                )

    tested = d.get("tested")
    measurements = d.get("measurement", [])
    if "measurement" in d and not isinstance(measurements, list):
        report.error(
            where,
            "[[device.measurement]] must be an array of tables, got "
            f"{type_name(measurements)}",
        )
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
        check_measurement(report, where, i, m, status)

    if not isinstance(vendor, str) or not isinstance(model, str):
        return None
    if not vendor or not model:
        return None
    return (vendor, model)


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
    except UnicodeDecodeError as exc:
        # tomllib decodes the stream itself, so non-UTF-8 bytes surface here and
        # are NOT a TOMLDecodeError.
        print(f"error: {path} is not valid UTF-8: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"error: cannot read {path}: {exc}", file=sys.stderr)
        return 1

    report = Report()

    for key in sorted(data):
        if key != "device":
            report.error(
                str(path.name),
                f"unknown top-level key {key!r} — device entries use "
                "[[device]]; nothing else is read",
            )

    if "device" not in data:
        # An empty or mistyped file must not read as a clean pass.
        print(
            f"error: {path} contains no [[device]] entries. If that is "
            "deliberate, the file should not exist; if not, check the table "
            "name is [[device]] (singular).",
            file=sys.stderr,
        )
        return 1

    devices = data["device"]
    if not isinstance(devices, list):
        print(
            f"error: 'device' must be an array of tables ([[device]]), got "
            f"{type_name(devices)}",
            file=sys.stderr,
        )
        return 1
    if not devices:
        print(f"error: {path} has an empty [[device]] array", file=sys.stderr)
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
            status = device.get("status")
            label = status if isinstance(status, str) else "?"
            counts[label] = counts.get(label, 0) + 1
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
