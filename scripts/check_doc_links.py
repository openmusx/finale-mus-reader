#!/usr/bin/env python3
"""Verify that every relative markdown link and heading anchor in the repository resolves.

The documentation is navigated by link, so a broken link is a documentation outage: a reader
following it lands nowhere.
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {"build", "build-release", "third_party", ".git", ".cache", "node_modules"}
SKIP_PREFIXES = ()

LINK = re.compile(r"\[[^\]\n]*\]\(([^)\s]+)\)")
HEADING = re.compile(r"^#{1,6} (.*)$", re.MULTILINE)


def repo_relative(path):
    """Repo-relative path with forward slashes, so prefix tests work on Windows too."""
    return os.path.relpath(path, REPO).replace(os.sep, "/")


def slug(text):
    """GitHub's heading slug: lowercase, drop everything but word chars, spaces and hyphens,
    then spaces become hyphens. Repeats are not collapsed, so an em dash between two words
    leaves a double hyphen."""
    t = text.strip().lower()
    t = re.sub(r"[^\w\s-]", "", t, flags=re.UNICODE)
    return t.strip().replace(" ", "-")


def anchors_of(path):
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return set()
    return {slug(m.group(1)) for m in HEADING.finditer(text)}


def markdown_files():
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in files:
            if not name.endswith(".md"):
                continue
            path = os.path.join(root, name)
            if repo_relative(path).startswith(SKIP_PREFIXES):
                continue
            yield path


def main() -> int:
    anchor_cache = {}
    failures = []
    for path in markdown_files():
        rel = repo_relative(path)
        text = open(path, encoding="utf-8").read()
        for match in LINK.finditer(text):
            target = match.group(1)
            if "://" in target or target.startswith("mailto:"):
                continue
            frag = ""
            if "#" in target:
                target, _, frag = target.partition("#")
            if not target:
                if frag and frag.lower() not in anchors_of(path):
                    failures.append("%s: no such heading '#%s' in this file" % (rel, frag))
                continue
            dest = os.path.normpath(os.path.join(os.path.dirname(path), target))
            if not os.path.exists(dest):
                failures.append("%s: broken link -> %s" % (rel, target))
                continue
            if frag and dest.endswith(".md"):
                if dest not in anchor_cache:
                    anchor_cache[dest] = anchors_of(dest)
                if frag.lower() not in anchor_cache[dest]:
                    failures.append("%s: no such heading '#%s' in %s" % (rel, frag, target))

    for line in failures:
        print(line, file=sys.stderr)
    print("%d markdown files checked, %d broken references" % (
        sum(1 for _ in markdown_files()), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
