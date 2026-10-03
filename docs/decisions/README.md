# Architecture decision records

Each file here records one decision: the context, the options weighed, the
choice, and what the choice costs. Records are immutable once accepted — a
decision that changes gets a new record that supersedes the old one, so the
reasoning behind a past choice stays readable.

## Status values

| Status | Meaning |
|---|---|
| `Open` | The decision is identified but not made. Blocks any work that depends on it. |
| `Accepted` | Decided. Implementation should follow it. |
| `Superseded by ADR-NNNN` | No longer in force; the named record replaces it. |
| `Rejected` | Considered and deliberately not taken. |

## Index

| # | Decision | Status | Blocks |
|---|---|---|---|
| [0001](0001-record-architecture-decisions.md) | Record architecture decisions | Accepted | — |
| [0002](0002-licence.md) | Project licence | **Open** | M0 exit; dependency and SDK choices |
| [0003](0003-engine-language-and-ui-stack.md) | Engine language and UI stack | **Open** | M0 exit; all of M1 |
| [0004](0004-default-layout.md) | Default layout for new users | **Open** | M3 (Simple view v1) |
| [0005](0005-windows-virtual-device-count.md) | Windows virtual device count | **Open** | M12; research starts in Phase 2 |
| [0006](0006-lowest-latency-default.md) | Whether Lowest is the default for pro interfaces | **Open** | M5 (first-run wizard defaults) |
| [0007](0007-support-baseline.md) | Support baseline: PipeWire, kernel, distributions | **Open** | The CI and test matrix |

The six records marked **Open** are the six open decisions in roadmap
section 11, one for one. M0's exit criteria name two of them — the licence
(0002) and the engine language and UI stack (0003) — and M0's usability tests
are what answer 0004. The rest should be settled early for the reasons each
record gives, but the roadmap does not gate M0 on them.

## Template

```markdown
# ADR-NNNN: Title

- **Status:** Open | Accepted | Superseded by ADR-NNNN | Rejected
- **Date:** YYYY-MM-DD
- **Roadmap:** the section this decision serves

## Context
What makes this a decision rather than an obvious default.

## Options
Each option with its consequences, not just its name.

## Decision
The choice, or "Not yet made" with what would settle it.

## Consequences
What follows, including what this forecloses.
```
