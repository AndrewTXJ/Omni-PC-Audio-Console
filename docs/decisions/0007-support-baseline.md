# ADR-0007: Support baseline — PipeWire, kernel and distributions

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 6; 4.12 (packaging); section 10 (risks)

## Context

Section 10 names Linux configuration diversity a standing support burden, and
singles out the WirePlumber 0.4 versus 0.5 configuration-format split. Every
version floor is a trade: a higher one removes compatibility code and whole
classes of bug report; a lower one reaches users on long-term-support
distributions who cannot move.

Three floors need setting, and they interact:

- **PipeWire**, which decides which node APIs and quantum controls can be
  assumed (4.7).
- **WirePlumber**, where 0.4 and 0.5 take different configuration formats, and
  the console must set device priority to hold the system default (4.4).
- **Kernel**, where the roadmap notes RT support has been in mainline since
  6.12 — worth verifying — while `threadirqs` and a low-latency build remain
  the practical path on older kernels (4.6).

4.12 already names the certification targets: current Fedora, Ubuntu LTS,
Debian stable and Arch on PipeWire, plus a PulseAudio-only setup. What it does
not fix is the *version floor*, and Debian stable is normally the binding
constraint.

## Options

- **Floor at the oldest version on a currently supported Ubuntu LTS and Debian
  stable.** Widest reach, and matches 4.12's stated matrix. Costs: carrying
  both WirePlumber configuration formats, and compatibility paths that the
  `doctor` tool must then diagnose.
- **Floor at a recent PipeWire and WirePlumber 0.5.** Much less compatibility
  code and a cleaner `doctor`. Costs: excludes users on older stable releases,
  who are a meaningful share of the Linux desktop and precisely the audience
  that finds a mixer through their distribution's repositories.
- **Tiered support.** Certify the four distributions, and declare older
  versions best-effort with the `doctor` tool reporting what is degraded.
  Honest, and matches section 10's mitigation of "minimal reliance on config
  files". Costs: "best-effort" needs a concrete definition or it becomes an
  unbounded promise.

## Decision

**Not yet made.** Specifically undecided: the minimum PipeWire version, whether
WirePlumber 0.4 is supported alongside 0.5, and the minimum kernel.

What would settle it:

1. Record the PipeWire and WirePlumber versions actually shipped by current
   Fedora, Ubuntu LTS, Debian stable and Arch, with dates. This is a table
   someone can produce in an afternoon and it likely decides the question.
2. Determine whether the features 4.4 and 4.7 need — device priority, graph
   driver control, quantum setting — are available at the oldest of those
   versions. If they are, the wide floor is nearly free.
3. Verify the claim that mainline RT landed in 6.12, and establish what the
   `doctor` tool should recommend on kernels without it.

AppImage (4.12) is sometimes offered as the escape hatch here, but it is not
one for this decision: all three floors are host-side. PipeWire and WirePlumber
are daemons the console talks to, with the configuration format living in the
user's own system, and the kernel is the kernel. Bundling the application
changes none of them. AppImage removes the *packaging* dependency on a
distribution; it does not remove the *runtime* dependency on that
distribution's sound server and kernel.

So this decision does block some users while it stays open, and saying otherwise
would be the kind of comfortable claim the `doctor` tool exists to replace. What
limits the damage is diagnosis, not packaging: `doctor` can state plainly which
floor a given system misses and what degrades.

## Consequences

- This decision defines the CI matrix, so it should be settled before much CI
  is written. The current workflow is deliberately distribution-independent.
- The Flatpak edition is PipeWire-only regardless (4.12), because ALSA
  exclusive access and real-time priority need permissions Flatpak does not
  normally grant — so Lowest mode (ADR-0006) is outside the Flatpak build's
  scope whatever the floor turns out to be.
