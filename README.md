# Omni-PC-Audio-Console

A Voicemeeter-style mixer and router for the PC, with a professional engine
underneath.

You pick your devices, tick A/B buttons to route, and your apps just work. When
you need more, the same mixer opens up into EQ, dynamics, plugins, sends and a
full console view. Linux first, then processing, networking and control, then
Windows.

## Status

**Pre-M0 — planning only. There is no engine code yet, and nothing here runs
audio.**

The plan of record is [`docs/ROADMAP.md`](docs/ROADMAP.md). What exists so far is
the groundwork M0 calls for: the open decisions written down, the quality
specifications turned into runnable procedures, and a device database with the
integrity rules to keep it honest.

Six decisions are deliberately still open. Two are named in M0's own exit
criteria — the **licence** and the **engine language and UI stack** — and the
second of those blocks all of M1, which makes it the critical path. See
[`docs/decisions/`](docs/decisions/README.md), where each record states what
evidence would settle it.

Because [ADR-0002](docs/decisions/0002-licence.md) is unresolved there is no
`LICENSE` file, and therefore no stated terms for contributions yet. See
[`CONTRIBUTING.md`](CONTRIBUTING.md).

## What it is meant to do

| Job | How the console does it |
|---|---|
| Play the game on speakers and music in headphones | Per-app routing, or each app plays into its own virtual device, and the A buttons pick the output |
| Send game, music and mic to the stream, but not the call | Tick B1 on those strips and leave B1 off on the chat strip |
| Be on a call without hearing yourself or getting echo | The call app records from a B bus that excludes the call's own audio (mix-minus) |
| Switch headphones and speakers with one key | A profile or an output hotkey |
| Monitor with minimal latency on a pro interface | Lowest latency mode and direct-monitoring controls |

Existing Linux tools each cover one slice — pavucontrol for volumes, qpwgraph and
Helvum for patching, EasyEffects for effects, Carla for plugin hosting. This
project puts them behind one Voicemeeter-style mixer.

## Roadmap at a glance

| Phase | Theme |
|---|---|
| 1. Linux core | A Voicemeeter-style mixer that just works, on a transparent, lowest-latency engine |
| 2. Processing and connectivity | Simple knobs on a pro processing chain, plus PC automation |
| 3. Windows | Same app, same profiles |

Latency and quality figures in the roadmap are **targets to validate on a
reference test rig**, not promises. Where a figure is set by converters rather
than by the engine, the test plan says so.

## Documentation

| Document | What it covers |
|---|---|
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | The plan of record: phases, milestones M0–M12, pro-layer reference, quality specifications, risks, open decisions |
| [`docs/decisions/`](docs/decisions/README.md) | Architecture decision records, including the six open decisions and what would settle each |
| [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) | Roadmap 7.7's 19 quality specifications as runnable procedures, plus the reference rig |
| [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md) | Which interfaces work, and how to contribute a measured device |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Where help is useful now, and the scope rules |

## Repository layout

```
docs/
  ROADMAP.md              the plan of record
  TEST-PLAN.md            how each quality specification is measured
  COMPATIBILITY.md        device support, and how to add a device
  decisions/              architecture decision records
data/
  compatibility/          device database (TOML), its schema and a template
tools/
  validate_compatibility.py   enforces the database's integrity rules
  check_links.py              checks relative links and anchors in Markdown
```

## Checks

No compiled code yet, so the checks validate data and documentation:

```sh
python3 tools/validate_compatibility.py
python3 tools/check_links.py
```

Both use only the Python standard library, so the tooling does not prejudge the
engine language ([ADR-0003](docs/decisions/0003-engine-language-and-ui-stack.md)).
They need Python 3.11 or newer, for `tomllib`.

CI runs those two commands plus one more check that has no local equivalent: a
shell step that fails the build if a licence file exists while ADR-0002 is still
`Open`, or if that record is no longer `Open` and no licence file exists. See
[`CONTRIBUTING.md`](CONTRIBUTING.md).
