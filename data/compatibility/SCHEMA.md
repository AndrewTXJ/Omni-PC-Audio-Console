# Compatibility database schema

The device list the roadmap calls for in 4.8: "a maintained list of tested
devices with measured latency and known quirks", feeding "a public compatibility
database fed by the device report tool" (section 8).

Entries live in `devices.toml` and are validated by
[`tools/validate_compatibility.py`](../../tools/validate_compatibility.py),
which CI runs on every change. TOML was chosen because it is readable in a pull
request, writable by hand, and parseable by the Python standard library — so
validation needs no dependencies while ADR-0003 is open.

## Integrity rules

The validator enforces these, because a compatibility list that carries
unsupported claims is worse than no list. Every rule below is covered by a
regression fixture; none is aspirational.

1. `status = "supported"` **requires** at least one measurement block and a
   `[device.tested]` block. A device cannot be called supported without someone
   having measured it.
2. `status = "unsupported"` **requires** a `reason`. "It does not work" with no
   cause helps nobody and cannot be rechecked.
3. Every measurement **requires** the kernel and sound-server versions it was
   taken on, because a figure without a software context is not reproducible.
   Required fields must be **non-blank strings**: `kernel = ""` and
   `kernel = 6` are both rejected.
4. `vendor` plus `model` must be unique.
5. A measurement must **not** carry a `status` field. Status is a property of
   the device, and a measurement restating it can only ever contradict it.
6. **Unknown fields are rejected**, at every level. A misspelled
   `round_trip_ms` would otherwise drop the headline figure silently, which is
   the same false-pass failure these rules exist to prevent.
7. **A date must be a real date.** The template's `YYYY-MM-DD` placeholder is
   rejected explicitly, so an unfilled template cannot be published as a tested
   device.

### Wrong types are errors, not skipped checks

A value of the wrong TOML type **fails**; it is never passed over. This is worth
stating because the obvious implementation does the opposite: guarding each
numeric check with a type test means `period_frames = "fast"` skips the check
and validates clean. An earlier version of this validator had exactly that bug.

A malformed entry also must not crash the run — a list where a string belongs is
reported, not raised — because the validator collects every problem in one pass
and a traceback would discard the entries already checked.

### Requirements

Python **3.11 or newer**, for `tomllib`. The tool checks the version and exits
with an actionable message rather than an `ImportError`; note that Ubuntu 22.04
LTS still ships 3.10 as `python3`.

## Fields

### `[[device]]`

| Field | Required | Type | Notes |
|---|---|---|---|
| `vendor` | yes | string | As printed on the device |
| `model` | yes | string | Specific enough to identify the revision |
| `connection` | yes | enum | `usb2`, `usb3`, `thunderbolt`, `pcie`, `firewire`, `bluetooth`, `onboard` |
| `status` | yes | enum | `supported`, `partial`, `unsupported`, `untested` |
| `driver` | no | string | Kernel driver, e.g. `snd-usb-audio`, `snd-hdsp` |
| `class_compliant` | no | bool | UAC1/UAC2 without a vendor driver (4.8) |
| `channels_in` / `channels_out` | no | int | At the device's default mode |
| `rates` | no | int array | Supported sample rates in Hz |
| `formats` | no | string array | `S16_LE`, `S24_3LE`, `S24_LE`, `S32_LE` (4.8) |
| `reason` | if unsupported | string | Why it does not work, and whether an application could fix it |
| `workaround` | no | string | E.g. a class-compliant mode on the hardware (4.8) |
| `quirks` | no | string array | Unusual rate sets, implicit-feedback endpoints, fixed formats (4.8) |
| `alsa_controls` | no | string array | Exposed interface controls: phantom power, pad, direct monitoring (4.8) |
| `notes` | no | string | Anything a user would want to know before buying |
| `sources` | no | string array | Where the information came from. Required in practice for anything not measured in-house |

### `[device.tested]`

| Field | Required | Type | Notes |
|---|---|---|---|
| `date` | yes | string | `YYYY-MM-DD` |
| `kernel` | yes | string | `uname -r` |
| `sound_server` | yes | string | E.g. `pipewire 1.0.5`, `pulseaudio 16.1` |
| `distro` | no | string | |
| `tester` | no | string | Handle or name |

### `[[device.measurement]]`

One per backend and latency setting, matching the test plan's QS-02.

| Field | Required | Type | Notes |
|---|---|---|---|
| `backend` | yes | enum | `alsa`, `pipewire`, `pulseaudio`, `jack`, `wasapi_shared`, `wasapi_exclusive`, `asio` |
| `setting` | no | enum | `lowest`, `balanced`, `safe` (4.6) |
| `period_frames` | yes | int | Smallest stable period achieved |
| `round_trip_ms` | yes | float | Measured, per QS-02 |
| `engine_added_periods` | no | int | Must be 0 to meet QS-02 |
| `xruns_24h` | no | int | From QS-08, where run |
| `thd_n_db` | no | float | From QS-03 Part B, negative dB |
| `kernel` | yes | string | The kernel this figure was taken on |
| `sound_server` | yes | string | The sound-server version this figure was taken on |
| `notes` | no | string | |

## Adding a device

Copy `TEMPLATE.toml`, fill it in, append it to `devices.toml`, and run:

```sh
python3 tools/validate_compatibility.py
```

Figures must come from the procedures in [`docs/TEST-PLAN.md`](../../docs/TEST-PLAN.md),
not from a vendor's specification sheet. Latency is published per device
precisely because the practical floor is set by the interface, its driver and
the bus (4.6).
