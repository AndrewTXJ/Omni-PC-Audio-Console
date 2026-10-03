#!/usr/bin/env python3
"""Check relative links and anchors in the repository's Markdown files.

Only local targets are checked: files, directories, and `#anchors`. Nothing is
fetched over the network, so this is safe and fast in CI, and it cannot fail
because an external site is down.

The slug algorithm mirrors github-slugger, which is what GitHub actually uses:
lowercase, strip characters that are not word/space/hyphen, then replace each
space with its own hyphen. That last detail matters -- "[P0 . S/E]" yields a
DOUBLE hyphen on GitHub because the dot is removed from between two spaces, and
an earlier version of this tool collapsed whitespace runs and so disagreed with
GitHub on 19 headings already in this repository.

Standard library only (see ADR-0003).

Usage:
    python3 tools/check_links.py [root]

Exit status 0 if every local link resolves, 1 otherwise.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Inline images: stripped before link matching so that the badge idiom
# [![alt](img.svg)](page.md) reports on the PAGE, not the image.
IMAGE = re.compile(r"!\[[^\]]*\]\(\s*<?([^)\s>]*)>?(?:\s+\"[^\"]*\")?\s*\)")
# Inline links. Applied to a line whose images and code spans are already masked.
LINK = re.compile(r"\[(?P<text>[^\]]*)\]\(\s*<?(?P<target>[^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
# Reference-style use: [text][label] or collapsed [label][].
REF_USE = re.compile(r"\[(?P<text>[^\]]+)\]\[(?P<label>[^\]]*)\]")
# Reference-style definition: [label]: target
REF_DEF = re.compile(r"^\s{0,3}\[(?P<label>[^\]]+)\]:\s*<?(?P<target>\S+)>?")
# Inline code spans, including multi-backtick runs.
CODE_SPAN = re.compile(r"(?P<ticks>`+)(?P<body>.+?)(?P=ticks)")
ATX = re.compile(r"^\s{0,3}(?P<hashes>#{1,6})\s+(?P<title>.*?)\s*#*\s*$")
# Fenced code: the closing fence must use the same character and be at least as
# long as the opener (CommonMark), which is how a Markdown sample is shown
# inside a longer fence.
FENCE_OPEN = re.compile(r"^(?P<indent>\s{0,3})(?P<ticks>`{3,}|~{3,})(?P<info>.*)$")
SETEXT = re.compile(r"^\s{0,3}(?P<rule>=+|-+)\s*$")
SCHEME = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")


def slugify(title: str) -> str:
    """Approximate github-slugger on already-rendered heading text."""
    text = title
    # Strip inline markup the way the renderer would, leaving visible text.
    text = re.sub(r"`+([^`]*)`+", r"\1", text)
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\[[^\]]*\]", r"\1", text)
    text = re.sub(r"(\*\*|__)(.+?)\1", r"\2", text)
    text = re.sub(r"(\*|_)(.+?)\1", r"\2", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = text.lower()
    # Normalise tabs etc. to spaces WITHOUT collapsing runs: each space becomes
    # its own hyphen on GitHub.
    text = re.sub(r"[\t\r\f\v]", " ", text)
    # Remove everything that is not a word character, space or hyphen. \w keeps
    # accented letters, which GitHub also keeps -- so no NFKD normalisation here;
    # decomposing would strip the accent and produce the wrong slug.
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    text = text.strip()
    return text.replace(" ", "-")


def mask_code_spans(line: str) -> str:
    """Blank out inline code so links shown as examples are not resolved."""
    return CODE_SPAN.sub(lambda m: m.group("ticks") + " " * len(m.group("body")) + m.group("ticks"), line)


def parse(path: Path) -> tuple[list[tuple[int, str]], set[str], list[str]]:
    """Return (links, anchors, problems) for one file.

    links    -- (lineno, target) for every inline and reference-style link
    anchors  -- heading slugs, disambiguated the way GitHub does
    problems -- file-level problems (undecodable bytes, undefined ref labels)
    """
    problems: list[str] = []
    try:
        raw = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        return [], set(), [f"is not valid UTF-8 ({exc})"]
    except OSError as exc:
        return [], set(), [f"cannot be read ({exc})"]

    lines = raw.splitlines()
    links: list[tuple[int, str]] = []
    anchors: set[str] = set()
    ref_defs: dict[str, str] = {}
    ref_uses: list[tuple[int, str, str]] = []
    fence: tuple[str, int] | None = None  # (char, length)
    in_code: list[bool] = []

    # Pass 1: fence state, headings, reference definitions.
    for i, line in enumerate(lines):
        if fence is not None:
            char, length = fence
            m = FENCE_OPEN.match(line)
            if m and m.group("ticks")[0] == char and len(m.group("ticks")) >= length \
                    and not m.group("info").strip():
                fence = None
            in_code.append(True)
            continue

        m = FENCE_OPEN.match(line)
        if m:
            fence = (m.group("ticks")[0], len(m.group("ticks")))
            in_code.append(True)
            continue

        in_code.append(False)

        heading = ATX.match(line)
        title = None
        if heading:
            title = heading.group("title")
        else:
            # Setext: a text line underlined by === or ---. GitHub gives these
            # anchors too. Guard against a --- thematic break by requiring a
            # non-blank, non-list previous line.
            st = SETEXT.match(line)
            if st and i > 0:
                prev = lines[i - 1]
                if prev.strip() and not ATX.match(prev) and not FENCE_OPEN.match(prev) \
                        and not re.match(r"^\s{0,3}([-*+]\s|\d+[.)]\s|>|\|)", prev) \
                        and not SETEXT.match(prev):
                    title = prev.strip()
        if title is not None:
            slug = slugify(title)
            if slug:
                candidate, suffix = slug, 1
                while candidate in anchors:
                    candidate = f"{slug}-{suffix}"
                    suffix += 1
                anchors.add(candidate)

        d = REF_DEF.match(line)
        if d:
            ref_defs[d.group("label").strip().lower()] = d.group("target")

    # Pass 2: links, skipping code.
    for i, line in enumerate(lines):
        if i < len(in_code) and in_code[i]:
            continue
        if REF_DEF.match(line):
            continue
        masked = mask_code_spans(line)
        # Record image targets, then blank the image syntax so the outer link of
        # a badge resolves to the page.
        for m in IMAGE.finditer(masked):
            if m.group(1):
                links.append((i + 1, m.group(1)))
        cleaned = IMAGE.sub(lambda m: " " * len(m.group(0)), masked)
        for m in LINK.finditer(cleaned):
            links.append((i + 1, m.group("target")))
        for m in REF_USE.finditer(cleaned):
            label = (m.group("label").strip() or m.group("text").strip()).lower()
            ref_uses.append((i + 1, label, m.group("text")))

    for lineno, label, text in ref_uses:
        if label in ref_defs:
            links.append((lineno, ref_defs[label]))
        else:
            problems.append(f"line {lineno}: reference link [{text}][{label}] has no [{label}]: definition")

    return links, anchors, problems


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    root = root.resolve()

    files = sorted(
        p for p in root.rglob("*.md")
        if ".git" not in p.relative_to(root).parts
    )
    if not files:
        print(f"error: no Markdown files found under {root}", file=sys.stderr)
        return 1

    parsed = {p: parse(p) for p in files}
    anchors_by_file = {p: a for p, (_, a, _) in parsed.items()}

    errors: list[str] = []
    checked = 0
    skipped = 0

    for path, (links, _, problems) in parsed.items():
        rel = path.relative_to(root)
        for problem in problems:
            errors.append(f"{rel}: {problem}")
        for lineno, target in links:
            if SCHEME.match(target) or target.startswith("//"):
                skipped += 1
                continue

            checked += 1
            file_part, _, anchor = target.partition("#")
            # A link may be percent-encoded; decode only the common space case.
            file_part = file_part.replace("%20", " ")

            if not file_part:
                if anchor and anchor not in anchors_by_file[path]:
                    errors.append(
                        f"{rel}:{lineno}: anchor #{anchor} not found in this file"
                    )
                continue

            resolved = (path.parent / file_part).resolve()
            if not resolved.exists():
                errors.append(f"{rel}:{lineno}: target does not exist: {target}")
                continue

            try:
                resolved.relative_to(root)
            except ValueError:
                errors.append(f"{rel}:{lineno}: target escapes the repository: {target}")
                continue

            if anchor and resolved.suffix == ".md":
                known = anchors_by_file.get(resolved)
                if known is None:
                    known = parse(resolved)[1]
                    anchors_by_file[resolved] = known
                if anchor not in known:
                    errors.append(
                        f"{rel}:{lineno}: anchor #{anchor} not found in {file_part}"
                    )

    for error in errors:
        print(f"error: {error}", file=sys.stderr)

    if errors:
        print(f"\n{len(errors)} broken link(s) in {len(files)} file(s)", file=sys.stderr)
        return 1

    print(
        f"{len(files)} Markdown file(s): {checked} local link(s) resolve, "
        f"{skipped} external link(s) skipped"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
