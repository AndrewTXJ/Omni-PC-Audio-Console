# ADR-0003: Engine language and UI stack

- **Status:** Accepted
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

**C++20 for the engine, Qt 6/QML for the UI.** Chosen by the project owner.

What supports it:

- Every plugin API the roadmap wants to host — LV2, then CLAP, then VST3 (5.2)
  — is a C or C++ interface, and VST3 is the Windows priority (6.3).
- The Phase 3 Windows virtual audio driver (6.2) is kernel-mode C/C++ whatever
  the engine is written in. This keeps the project in one language instead of
  two across the same codebase.
- Qt 6/QML draws many faders and meters at 60 Hz without strain, and brings
  accessibility (a section 8 requirement) rather than needing it rebuilt.

**The accepted cost, stated plainly:** no compile-time protection against the
data races and lifetime bugs that cause intermittent xruns — the failure mode
that is hardest to reproduce in CI and therefore hardest to fix. Choosing this
language means the protection has to come from the build and the tests:

- `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`
  `-Wold-style-cast -Wnon-virtual-dtor -Wdouble-promotion -Werror` on every
  target. This is not ceremony: it caught two real double-promotion defects on
  the very first build.
- `-fno-fast-math -ffp-contract=off`, because `-ffast-math` permits
  reassociation and would silently invalidate every exactness claim in 7.7.
- The offline renderer and the null tests run on every commit, so the
  arithmetic is checked continuously rather than at milestone boundaries.
- **Owed at M2, when a real audio thread exists:** the lock-free parameter
  queue 4.6 requires, ThreadSanitizer in CI, and an assertion that nothing
  allocates on the audio thread. These are what a borrow checker would have
  given for nothing, so skipping them is not available.

**What was not done.** The prototype-both-languages experiment this record
previously prescribed was not run; the decision was made on the grounds above
instead, accepting two prototypes' worth of delay as the thing being avoided.
The number that experiment would have produced — M2's "0 extra periods" against
a raw ALSA loopback baseline (QS-02) — still has to be met, and a failure to
meet it is still what would reopen this record.

## Consequences

- M1 is unblocked and underway: the engine core, the offline renderer and the
  null tests are in `engine/`, `offline/` and `tests/`.
- ADR-0002 (licence) is now the only open decision M0's exit criteria name, and
  the repository is public, so it is the more urgent of the two.
- Whatever is chosen, 4.1's split stands: a headless daemon with the UI, tray,
  hotkeys, CLI and web remote as clients of one versioned control API. That
  split is what keeps this decision reversible for the UI and contained for the
  engine.
- The offline renderer (4.1, M1) was built first, not retrofitted, which is
  what makes the quality specifications testable in CI without hardware. It has
  already earned that: it caught a fade-in on the first rendered frame that
  every library-level test had hidden by pre-rolling.
- Qt 6 is not yet a build dependency. The engine and the renderer need neither
  Qt nor ALSA, so M1 builds and tests on any machine with a C++20 compiler;
  Qt enters at M3 with the Simple view.
