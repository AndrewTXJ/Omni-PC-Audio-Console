#!/usr/bin/env python3
"""Check relative links and anchors in the repository's Markdown files.

Only local targets are checked: files, directories, and same-file `#anchors`.
Nothing is fetched over the network, so this is safe and fast in CI, and it
cannot fail because an external site is down.

Standard library only (see ADR-0003).

Usage:
    python3 tools/check_links.py [root]

Exit status 0 if every local link resolves, 1 otherwise.
"""

from __future__ import annotations

import re
import sys
import unicodedata
from pathlib import Path

# [text](target) — skips images (![...]) by requiring no leading '!'.
LINK = re.compile(r"(?<!!)\[(?P<text>[^\]]*)\]\((?P<target>[^)\s]+)(?:\s+\"[^\"]*\")?\)")
# ATX headings, used to build the anchor list for a file.
HEADING = re.compile(r"^(?P<hashes>#{1,6})\s+(?P<title>.+?)\s*#*\s*$")
FENCE = re.compile(r"^\s*(?P<ticks>`{3,}|~{3,})")

SKIP_DIRS = {".git", ".github/workflows"}


def slugify(title: str) -> str:
    """Approximate GitHub's heading-anchor algorithm.

    Lowercase, strip anything that is not a word character, space or hyphen,
    then replace spaces with hyphens. Inline Markdown is stripped first.
    """
    text = re.sub(r"`([^`]*)`", r"\1", title)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"[*_]{1,3}([^*_]+)[*_]{1,3}", r"\1", text)
    text = unicodedata.normalize("NFKD", text)
    text = text.lower()
    text = re.sub(r"[^\w\s-]", "", text)
    return re.sub(r"\s+", "-", text.strip())


def parse(path: Path) -> tuple[list[tuple[int, str]], set[str]]:
    """Return (links, anchors) for one file, ignoring fenced code blocks."""
    links: list[tuple[int, str]] = []
    anchors: set[str] = set()
    fence: str | None = None

    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        match = FENCE.match(line)
        if match:
            ticks = match.group("ticks")
            if fence is None:
                fence = ticks[0] * 3
                continue
            if stripped.startswith(fence):
                fence = None
                continue
        if fence is not None:
            continue

        heading = HEADING.match(line)
        if heading:
            slug = slugify(heading.group("title"))
            # GitHub disambiguates repeated headings with -1, -2, ...
            candidate, suffix = slug, 1
            while candidate in anchors:
                candidate = f"{slug}-{suffix}"
                suffix += 1
            anchors.add(candidate)

        for link in LINK.finditer(line):
            links.append((lineno, link.group("target")))

    return links, anchors


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    root = root.resolve()

    files = sorted(
        p for p in root.rglob("*.md")
        if not any(part == ".git" for part in p.relative_to(root).parts)
    )
    if not files:
        print(f"error: no Markdown files found under {root}", file=sys.stderr)
        return 1

    parsed = {p: parse(p) for p in files}
    anchors_by_file = {p: a for p, (_, a) in parsed.items()}

    errors: list[str] = []
    checked = 0
    skipped = 0

    for path, (links, _) in parsed.items():
        rel = path.relative_to(root)
        for lineno, target in links:
            # External and non-file schemes are out of scope.
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("//"):
                skipped += 1
                continue

            checked += 1
            file_part, _, anchor = target.partition("#")

            if not file_part:
                # Same-file anchor.
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
        print(
            f"\n{len(errors)} broken link(s) in {len(files)} file(s)",
            file=sys.stderr,
        )
        return 1

    print(
        f"{len(files)} Markdown file(s): {checked} local link(s) resolve, "
        f"{skipped} external link(s) skipped"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
