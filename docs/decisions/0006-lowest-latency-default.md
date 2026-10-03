# ADR-0006: Whether Lowest is the default for pro interfaces

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 5; 4.6 (lowest-latency engine); 4.7 (backends)

## Context

The Simple view exposes one latency control with three settings (4.6):

| Setting | Path |
|---|---|
| Lowest | Direct ALSA `hw:` exclusive access, bypassing the sound server |
| Balanced | Native PipeWire client at quantum 32 to 64 |
| Safe | PipeWire or Pulse with larger buffers |

Lowest takes the card exclusively, acquiring it through
`org.freedesktop.ReserveDevice1` so PipeWire or Pulse release it, and handing it
back on exit (4.7). Other applications still reach the console through its
virtual devices, so this is not as disruptive as exclusive access usually is —
but it is a larger change to the user's system than Balanced, and the roadmap
flags that keeping virtual devices sample-synchronous in this mode is itself a
prototype task for M2.

The question is whether detecting a professional interface should be enough to
select it automatically.

## Options

- **Lowest by default on pro interfaces.** Delivers the headline figure (5 ms or
  less round trip on a good USB interface) without the user finding a setting.
  Risks: it rests on the M2 prototype succeeding; "professional interface" has
  no crisp definition; and a first-run failure on exclusive acquisition is the
  worst possible first impression, against principle 4.
- **Balanced by default, always.** Predictable, coexists with everything, and
  the auto-tuner (4.6) already finds the lowest stable setting within it. Risks:
  users who bought an interface for latency may conclude the console is slow
  and never find Lowest.
- **Let the first-run goal decide.** "Music and studio" already implies lowest
  latency and hardware direct monitoring (4.10); "Everyday" does not. This ties
  the setting to stated intent rather than to inferred hardware class.
- **Offer it, measured.** Default to Balanced, then have the auto-tuner measure
  what Lowest would achieve on this hardware and offer it with the real figure.
  Costs an extra step and some acquire/release churn during setup.

## Decision

**Not yet made.** It depends on an unresolved technical question: whether the
M2 prototype can make the ALSA clock drive the PipeWire graph so virtual
devices stay sample-synchronous in Lowest mode (4.7). If it cannot, Lowest
means degraded behaviour for other applications and should not be any kind of
default.

What would settle it:

1. The M2 prototype result above. This gates the rest.
2. Measured reliability of `ReserveDevice1` acquisition across the four-distro
   matrix (ADR-0007) — how often it fails, and how it fails.
3. Whether "professional interface" can be detected reliably enough to key a
   default on. The compatibility database (`data/compatibility/`) is the place
   that evidence accumulates, and if it cannot, the goal-based option wins by
   default.

## Consequences

- Either goal-based option keeps 4.6's promise that this is *one* control in the
  Simple view. Any option that adds a second latency control fails the
  admission rule of 2.1.
- Whatever the default, 4.6 requires the measured round trip to be displayed, so
  a user on Balanced can always see what they are getting.
