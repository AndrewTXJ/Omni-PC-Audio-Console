# Omni-PC-Audio-Console

A Voicemeeter-style mixer and router for the PC, with a professional engine
underneath.

You pick your devices, tick A/B buttons to route, and your apps just work. When
you need more, the same mixer opens up into EQ, dynamics, plugins, sends and a
full console view. Linux first, then processing, networking and control, then
Windows.

## Status

**M1 in progress: the engine core and the offline renderer exist and are tested.
Nothing talks to a sound card yet** — that is M2.

What works today:

- A graph of strips and buses with sends, smoothing, metering and a safety
  limiter, following the signal flow in roadmap 7.1.
- `omni-render`, an offline renderer, so routing and DSP are testable in CI with
  no audio hardware (4.1).
- The null tests: the unity path is **bit-exact**, and unrouted buses are
  **exactly** silent — through the library and through the renderer.

The plan of record is [`docs/ROADMAP.md`](docs/ROADMAP.md).
[`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) has a table of exactly which quality
specifications run today and which are still waiting on a milestone.

The engine is **C++20 with Qt 6/QML** ([ADR-0003](docs/decisions/0003-engine-language-and-ui-stack.md)).
Qt is not yet a dependency: the engine and renderer need neither Qt nor ALSA, so
this builds anywhere with a C++20 compiler. Five decisions remain open, and the
**licence** ([ADR-0002](docs/decisions/0002-licence.md)) is the one M0's exit
criteria still name — on a public repository, which makes it the urgent one.

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
| [`docs/decisions/`](docs/decisions/README.md) | Architecture decision records: five open decisions and what would settle each, plus the settled stack choice |
| [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) | Roadmap 7.7's 19 quality specifications as runnable procedures, plus the reference rig |
| [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md) | Which interfaces work, and how to contribute a measured device |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Where help is useful now, and the scope rules |

## Building

Needs a C++20 compiler, CMake 3.22+ and Ninja. No Qt, no ALSA, no network.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Warnings are errors by default (`-DOMNI_WERROR=OFF` to relax). `-ffast-math` is
explicitly disabled and must stay that way: it permits reassociation, which would
silently invalidate every exactness claim in roadmap 7.7.

### Trying the renderer

The roadmap's headline job — game and music to the headphones, but only the game
to the stream:

```sh
./build/offline/omni-render   --strip game.wav  --route 0:0 --route 0:1   --strip music.wav --route 1:0   --bus 0:headphones.wav:s24   --bus 1:stream.wav:s24 --verbose
```

`--route S:B[:dB]` is an A/B button: a send from strip S to bus B, 0 dB and
post-fader by default. Leaving the music strip off bus 1 is all mix-minus is.
`--help` lists the per-strip and per-bus options.

## Repository layout

```
engine/
  include/omni/dsp/       dB, ramps, pan laws, metering, guards, safety limiter
  include/omni/engine/    strip, bus, graph, delay line
  include/omni/io/        WAV reader and writer
  src/                    strip, bus, graph and WAV implementations
offline/                  omni-render, the offline renderer
tests/                    null tests and unit tests (no external framework)
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

Alongside the C++ tests, two checks validate data and documentation:

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
