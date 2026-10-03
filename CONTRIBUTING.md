# Contributing

The project is at **pre-M0**: planning material only, no engine code. What is
most useful right now is help closing the open decisions, not code.

## Before anything else: the licence is not settled

[ADR-0002](docs/decisions/0002-licence.md) is open, and there is no `LICENSE`
file. Until that is resolved the project has no stated terms, so **please do not
submit substantial contributions yet** — there is nothing to tell you what
happens to them. Issues, measurements and discussion of the open decisions are
very welcome in the meantime.

## Where the work is

Read [`docs/ROADMAP.md`](docs/ROADMAP.md) first; it is the plan of record. Then:

| If you want to | Start at |
|---|---|
| Help settle an open decision | [`docs/decisions/`](docs/decisions/README.md) — each record states what evidence would settle it |
| Add a tested device | [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md) |
| Work on how quality is measured | [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) |

The six open decisions are the critical path. M0's exit criteria name two of
them, and [ADR-0003](docs/decisions/0003-engine-language-and-ui-stack.md) — the
engine language and UI stack — blocks all of M1, so it is the one most worth
closing first.

## Decisions

Anything architectural gets a record in [`docs/decisions/`](docs/decisions/README.md),
in the same pull request as the change that implements it. Records are immutable
once accepted; a changed decision gets a new record that supersedes the old one.

If you are settling an existing open decision, update that record's status and
the index table rather than writing a new one.

## Scope discipline

The roadmap names scope creep as a risk, and answers it with P0/P1/P2 tags,
templates before features, and hosting plugins rather than writing them
(section 10). Two rules follow:

- **The admission rule (2.1).** A control enters the Simple view only if a
  typical PC user would use it weekly. Everything else goes in the Expert panel.
  A pull request that adds to the Simple view should say why it clears that bar.
- **Respect the priorities.** P0 must ship in its phase, P1 should, P2 is a
  stretch. Moving an item earlier is a roadmap change, so propose it as one.

## Quality specifications are not optional

[`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) turns roadmap 7.7 into runnable
procedures, and each phase's acceptance depends on them. Changes to the audio
path need the relevant test to still pass. Several are absolute rather than
tolerance-based — a null test means *exactly* zero, not small.

Three deserve special care because they are safety or honesty properties rather
than quality ones:

- The safety limiter is last in every bus and cannot be bypassed (QS-14).
- Float-to-integer conversion saturates and never wraps (QS-18).
- The bit-transparent indicator is true only when the path really is
  bit-exact (QS-01).

## Checks

The repository currently holds no compiled code, so CI only validates data and
documentation:

```sh
python3 tools/validate_compatibility.py   # device database integrity
python3 tools/check_links.py              # relative links in Markdown
```

Both use the Python standard library only, deliberately — the tooling must not
prejudge [ADR-0003](docs/decisions/0003-engine-language-and-ui-stack.md).
Engine jobs join CI at M1.

## Commit messages

Say what changed and why. Reference the roadmap section or ADR a change serves,
since much of the work here only makes sense against the plan.
