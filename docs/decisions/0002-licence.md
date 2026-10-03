# ADR-0002: Project licence

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 1; section 8 (licensing); sections 5.2, 5.7, 6.1

## Context

The licence decides what the project can link against, and therefore which
features are reachable at all. The roadmap depends on components under
incompatible terms:

| Component | Terms as the roadmap records them | Used for |
|---|---|---|
| libsoxr | LGPL | Asynchronous resampling (7.6) |
| RNNoise | BSD | The Denoise knob (5.3) |
| DeepFilterNet | Permissive — verify which | Alternative denoiser (5.3) |
| JUCE | GPL or commercial | Candidate DSP and plugin-hosting framework (8) |
| NDI SDK | Free but proprietary | Optional A/V network path (5.7) |
| ASIO SDK | Verify current terms | Windows pro interfaces (6.1), ASIO bridge (6.4) |
| VST3 SDK | Verify current terms — dual GPLv3/proprietary historically | Plugin hosting (5.2), Windows priority (6.3) |

**Every entry in that table needs verifying against current terms before it is
relied on.** They are reproduced from the roadmap, not from a legal review, and
SDK licensing changes.

The tension is concentrated: a strong copyleft licence makes the engine
unambiguously free software and keeps contributions open, but makes bundling
NDI, ASIO and VST3 difficult or impossible. A permissive licence makes those
straightforward and allows closed derivatives of the whole console.

Two mitigations apply under any licence, and the roadmap already assumes both:

- **Runtime loading.** The roadmap specifies NDI as "an optional runtime
  plugin" (5.7, section 10). A plugin loaded at runtime, not distributed with
  the project, changes the analysis considerably — though not identically
  across jurisdictions or licences.
- **Hosting, not embedding.** The project hosts LV2, CLAP and VST3 plugins
  rather than shipping them. Hosting an interface is a different question from
  linking an SDK.

## Options

- **GPL-3.0.** Strong copyleft. Unambiguously free; contributions stay open.
  JUCE usable under its GPL terms. NDI and ASIO become optional runtime-loaded
  components at best, and the Phase 3 ASIO bridge (6.4, already P2) may not be
  distributable. Verify the VST3 SDK's current GPL option.
- **LGPL-3.0.** Copyleft on the engine, with linking permitted. A middle
  position, but awkward for a monolithic application: the boundary that LGPL
  relies on is clear for a library and murky for an app.
- **MPL-2.0.** File-level copyleft. The engine's own files stay open; linking
  proprietary SDKs is workable. Less familiar to contributors than GPL or MIT,
  and offers weaker guarantees that downstream improvements come back.
- **Apache-2.0 or MIT.** Permissive. Every SDK path is open, downstream
  commercial use is easy, and the Windows driver work (6.2) has no licence
  friction. Nothing obliges a commercial fork to contribute back — a real risk
  for a project whose closest competitor is proprietary.
- **Dual licensing.** GPL plus a commercial option, as JUCE does. Keeps both
  doors open but requires a contributor licence agreement, which deters
  contributors and adds administration a team of one to three cannot absorb.

## Decision

**Not yet made.** No `LICENSE` file is in the repository, so the work is
currently unlicensed — i.e. all rights reserved by default, which is itself a
reason to settle this before accepting outside contributions.

What would settle it:

1. Verify the current terms of the ASIO, VST3 and NDI SDKs, and of libsoxr and
   the denoiser candidates. The table above is a starting point, not evidence.
2. Decide whether a commercial fork of the whole console is an acceptable
   outcome. This is a goal question, not a legal one, and it decides copyleft
   versus permissive more than any SDK detail does.
3. Confirm whether the resampler will be libsoxr (LGPL, so dynamic linking) or
   in-house — 7.6 leaves this open too, and it is the one hard dependency on
   the critical path.

## Consequences

- Until this is settled the repository cannot accept outside contributions on
  clear terms, and no dependency choice is final.
- Whatever is chosen, the roadmap's structural decision stands: NDI stays an
  optional runtime plugin and is not bundled (5.7, section 10).
- A licence change later requires the agreement of every contributor, so the
  cost of this decision rises with each one accepted. Settle it before M1.
