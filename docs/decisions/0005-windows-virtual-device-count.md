# ADR-0005: Windows virtual device count

- **Status:** Open
- **Date:** 2026-10-03
- **Roadmap:** section 11 decision 4; 6.2 (virtual devices); section 10 (risks)

## Context

Linux lets an application create virtual devices in user space, with no fixed
limit (4.2). Windows does not (6.2). A Windows audio driver normally exposes a
fixed set of endpoints decided at install time, so the platform may not be able
to match Linux's "as many as you like", and the session format is shared
between the two (6.5).

The roadmap names this the largest schedule risk in the project, together with
driver signing (section 10).

Two constraints are already known:

- **Names are limited to 31 characters**, because MME truncates longer ones, and
  names and IDs must be unique enough to coexist with Voicemeeter and VB-Cable
  (6.2).
- **Per-app capture does not need the driver at all.** The process loopback
  capture API covers many uses (6.2), which reduces how many endpoints the
  driver must provide.

## Options

- **Fixed set, e.g. 4 in and 4 out,** with an installer option for more.
  Predictable, matches how comparable products ship, and the simplest driver.
  Costs: a Linux profile using 6 virtual inputs will not load unchanged, which
  undercuts 6.6's parity test; changing the count means reinstalling a driver.
- **Runtime endpoint creation,** if the framework supports it. Matches Linux and
  keeps profiles portable. Costs: unproven on both candidate frameworks, and it
  is the part of Phase 3 most likely to overrun.
- **Fixed set now, runtime creation later.** Ships Phase 3 on a known path and
  treats runtime creation as an enhancement. Costs: the migration may require a
  driver redesign rather than an extension.

## Decision

**Not yet made,** and it cannot be made responsibly yet: it depends on
evaluating the Microsoft SYSVAD/PortCls sample against the newer ACX framework
(6.2), including whether current Microsoft guidance still recommends SYSVAD.

What would settle it:

1. Evaluate both frameworks for runtime endpoint creation specifically, not
   just for basic function. Verify current guidance rather than relying on the
   roadmap's summary.
2. Establish what EV code-signing and attestation signing require in practice
   (6.2), since cost and lead time feed the same schedule risk.
3. Decide how a profile that exceeds the available endpoint count should
   degrade. This is needed under *every* option, because even runtime creation
   has some ceiling, and 6.5 binds patches to logical endpoints by role and
   name — which is the mechanism a graceful degradation would use.

Per 6.2 and section 10, this research starts during Phase 2, not at the start of
Phase 3.

## Consequences

- Phase 3's parity acceptance (6.6) must state the endpoint-count exception
  explicitly if a fixed set is chosen.
- Point 3 above is work regardless of the outcome, so it can begin immediately
  and is the part worth doing first.
