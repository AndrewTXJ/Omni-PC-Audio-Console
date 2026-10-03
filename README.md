# Omni-PC-Audio-Console

A Voicemeeter-style mixer and router for the PC, with a professional engine underneath.

You pick your devices, tick A/B buttons to route, and your apps just work. When you
need more, the same mixer opens up into EQ, dynamics, plugins, sends and a full
console view. Linux first, then processing, networking and control, then Windows.

## Status

**Pre-M0.** This repository currently holds planning material only — there is no
code yet. The plan of record is [`docs/ROADMAP.md`](docs/ROADMAP.md).

Several foundational choices are deliberately still open (roadmap section 11),
including the licence, the engine language and the UI stack. Those are M0 exit
criteria and should be settled before engine work starts.

## What it is meant to do

| Job | How the console does it |
|---|---|
| Play the game on speakers and music in headphones | Per-app routing, or each app plays into its own virtual device, and the A buttons pick the output |
| Send game, music and mic to the stream, but not the call | Tick B1 on those strips and leave B1 off on the chat strip |
| Be on a call without hearing yourself or getting echo | The call app records from a B bus that excludes the call's own audio (mix-minus) |
| Switch headphones and speakers with one key | A profile or an output hotkey |
| Monitor with minimal latency on a pro interface | Lowest latency mode and direct-monitoring controls |

Existing Linux tools each cover one slice — pavucontrol for volumes, qpwgraph and
Helvum for patching, EasyEffects for effects, Carla for plugin hosting. This project
puts them behind one Voicemeeter-style mixer.

## Roadmap at a glance

| Phase | Theme |
|---|---|
| 1. Linux core | A Voicemeeter-style mixer that just works, on a transparent, lowest-latency engine |
| 2. Processing and connectivity | Simple knobs on a pro processing chain, plus PC automation |
| 3. Windows | Same app, same profiles |

Latency and quality figures in the roadmap are **targets to validate on a reference
test rig**, not promises.

## Documentation

- [`docs/ROADMAP.md`](docs/ROADMAP.md) — phases, milestones, pro-layer reference, quality specs, risks and open decisions.
