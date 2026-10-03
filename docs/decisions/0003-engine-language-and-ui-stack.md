# ADR-0003: Engine language and UI stack

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 2; section 8 (technology choices); 4.1, 4.6

## Context

The engine's constraints come from 4.6, and they are strict. On the audio
thread: no allocation, no locks, no syscalls, no logging. Parameters arrive
over lock-free queues; meters leave over lock-free shared memory (7.4). The
graph runs as one flat pass inside the device callback, and rebuilds happen off
the audio thread and swap in atomically (4.1).

The language choice is therefore about which errors the toolchain catches
before they become an xrun in someone's stream.

The UI is a separate question with one coupling: section 8 notes that a web UI
"doubles as a phone or tablet remote", which Phase 2 wants anyway (5.9). Since
4.1 makes the UI a *client* of the control API rather than part of the engine,
the two choices are genuinely independent, and a second UI can be added later
without touching the engine.

One constraint applies regardless: the Phase 3 Windows virtual audio driver
(6.2) is kernel-mode work against SYSVAD/PortCls or ACX, which is C or C++
whatever the engine is written in. No choice here avoids that.

## Options

### Engine

- **Rust.** Ownership rules catch data races at compile time, which is the
  failure mode that matters most on an RT thread shared with the UI. Good
  PipeWire bindings exist (`pipewire-rs`, named in section 8). Costs: RT
  discipline still needs care, because `Vec` growth and `Mutex` compile
  perfectly well on the audio thread — the guarantees are about races, not
  about real-time safety. Plugin SDK bindings (VST3, LV2) are thinner and
  partly community-maintained, and the Windows driver remains C/C++ anyway.
- **C++20.** The pro-audio default: every plugin SDK, JUCE, and the deepest
  pool of engineers who have debugged an xrun. Shares a language with the
  Windows driver. Costs: no compile-time protection against the races and
  lifetime bugs that produce intermittent glitches, which are exactly the bugs
  hardest to reproduce in CI.

### UI

- **Qt 6/QML.** Mature, draws many faders and meters at 60 Hz without strain,
  good accessibility support (section 8's requirement), native on both targets.
  Costs: a large dependency, licensing needs checking against ADR-0002, and the
  Phase 2 phone remote is separate work.
- **Web UI (Tauri).** The same interface serves as the phone and tablet remote
  (5.9) at little extra cost, and styling and theming are easy. Costs: 60 Hz
  meter redraw needs deliberate work to avoid burning CPU the engine needs;
  Flatpak plus webview adds moving parts; accessibility depends on getting ARIA
  right rather than inheriting it.

## Decision

**Not yet made,** at the user's direction. No engine code is written and the
repository holds no build system, so nothing yet presumes an answer.

What would settle it:

1. **Prototype the hot path in both.** M2's measurable target — the engine adds
   **0 extra periods** over a raw ALSA loopback baseline (7.7) — is the only
   test that matters, and it is cheap to run twice before the codebase exists.
   Use the same ALSA mmap callback and the same mixing loop in each.
2. **Check SIMD and plugin-hosting ergonomics** against 4.6's zero-copy,
   in-place mixing requirement and 5.2's LV2-then-CLAP-then-VST3 order.
3. **Count the team.** For one to three engineers (section 9), familiarity may
   outweigh every property above. An engine written fluently in the second-best
   language beats one written haltingly in the best.
4. **For the UI, test a 64-channel meter bridge** in both at 60 Hz and measure
   the CPU cost against 7.7's budget of under 25% of one core for 64 channels.

The tooling in this repository is deliberately neutral: `tools/` is Python
using only the standard library, and CI runs no compiler. Neither choice is
prejudged.

## Consequences

- M1 cannot start until this is decided. It and ADR-0002 are the two decisions
  M0's exit criteria name explicitly, and this is the one that blocks code.
- Whatever is chosen, 4.1's split stands: a headless daemon with the UI, tray,
  hotkeys, CLI and web remote as clients of one versioned control API. That
  split is what keeps this decision reversible for the UI and contained for the
  engine.
- The offline renderer (4.1, M1) must be part of the prototype in either
  language — it is what makes the quality specs testable in CI without
  hardware, and retrofitting it is much harder than building it in.
