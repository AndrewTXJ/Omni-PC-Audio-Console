# ADR-0001: Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-10-03
- **Roadmap:** section 11 (open decisions)

## Context

The roadmap carries six decisions it deliberately leaves open, and four of them
gate M0's exit. Several more will arise during M1 to M5 — the backend
abstraction, the control protocol's shape, the session file format. A roadmap
is the wrong place to hold them: it describes the destination, is edited in
place, and loses the reasoning behind a choice as soon as the text changes.

Without a record of *why*, a later contributor sees only the outcome, and
reopening a settled question costs the same research twice.

## Options

- **Decision records in the repository.** Plain Markdown, one file per
  decision, numbered and immutable. Cheap, reviewable in the same pull request
  as the code that implements the decision, and readable offline.
- **An issue tracker label.** Searchable and already present, but issues get
  closed and drift out of view, and the reasoning is scattered across comments.
- **A section of the roadmap.** No extra files, but edited in place, so history
  is only recoverable from `git log` of a large document.
- **A wiki.** Easy to write, but separate from the code, not reviewed, and
  prone to going stale.

## Decision

Keep decision records in `docs/decisions/`, in the format ADR-0001 itself
follows, as described in [the index](README.md).

Each of roadmap section 11's open decisions gets a record immediately, with
status `Open`, stating what evidence would settle it. An open decision with a
written-down question is a smaller obstacle than one that only exists as a line
in a table.

## Consequences

- A pull request that settles an open decision must update its record in the
  same change, so the record and the code never disagree.
- Records are immutable once accepted. Changing a decision means a new record
  that supersedes the old one — the index shows the chain.
- The index table duplicates status, so it needs updating alongside any status
  change. Accepted cost: a reader gets the whole picture from one table.
