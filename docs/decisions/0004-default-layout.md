# ADR-0004: Default layout for new users

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 3; 2.4 (layout sizes); 2.5 (first run)

## Context

Layouts are views of the same session and switching never touches the audio
(2.4), so this decision is about first impressions, not capability. It still
matters: principle 1 is "simple by default", and the Phase 1 acceptance test
requires 8 of 10 new users to route game to speakers, music to headphones, and
mic plus game to a stream in under 5 minutes without documentation (4.13).

| Layout | Size | Comparable to |
|---|---|---|
| Compact | 3 hardware and 2 virtual inputs; A1–A3, B1–B2 | Voicemeeter Banana |
| Standard | 5 hardware and 3 virtual inputs; A1–A5, B1–B3 | Voicemeeter Potato |

The acceptance task needs 3 strips (game, music, mic), 2 A buses and 1 B bus.
Compact covers it with nothing to spare; Standard covers it with room left.

## Options

- **Compact by default.** Fewest controls on screen, so the admission rule of
  2.1 is easiest to honour visibly. Risk: a user who needs a fourth strip has
  to discover that layouts exist and switch, and "I ran out of channels" is a
  worse first experience than "there are some spare".
- **Standard by default.** Covers more setups without a switch, and matches the
  Potato layout that streaming guides most often assume. Risk: more empty
  strips at first run, which reads as clutter and works against the two-minute
  goal.
- **Let the first-run goal decide.** 2.5 already asks the user to pick
  Everyday, Gaming and chat, Streaming, Podcast with remote guest, or Music and
  home studio, and 4.10 defines what each template needs. Everyday needs 3
  strips and 2 buses; Streaming needs 5 strips and 3 buses. The template
  therefore implies a layout, and no global default is needed.

  Note the gap: 4.10 has seven templates and the wizard offers five, so Live
  event and AV and Conferencing arrive from the cookbook with no goal behind
  them. That is not fatal — a template implies its layout whenever it is
  applied, wizard or not — but it does mean "the goal decides" is shorthand for
  "the template decides", and a user who starts from the cookbook never passes
  through the goal step at all.

## Decision

**Not yet made.** The third option looks strongest on the roadmap's own terms —
it removes the decision rather than making it — but it must not be adopted on
reasoning alone, because the question is empirical and M0 has the means to
answer it.

What would settle it: the M0 usability tests on the clickable prototype
(section 8, M0) — but note that testing Compact against Standard does **not**
test the third option, so the experiment has to cover three arms, not two:

1. Compact as the global default.
2. Standard as the global default.
3. No global default: the first-run goal picks the layout, per 2.5's goal list
   and 4.10's templates.

Measure time-to-success on the 4.13 acceptance task, whether users ever notice
the layout switch, and — for arm 3 specifically — whether a user whose chosen
template runs out of strips can find the switch. Arm 3 only wins if that
recovery path works; an unnoticed switch is what makes running out of channels a
dead end rather than an inconvenience.

If arm 3 tests well, this record closes as "no global default", and 4.10's
templates become the surface that needs explicit strip and bus counts.

## Consequences

- Whichever is chosen, 2.4's guarantee holds: switching layouts is
  non-destructive and never interrupts audio. That is what makes a wrong
  default cheap.
- If the templates imply the layout, the templates in 4.10 become the primary
  surface for this and need their strip and bus counts stated explicitly.
