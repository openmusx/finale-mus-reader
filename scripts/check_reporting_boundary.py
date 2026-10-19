#!/usr/bin/env python3
"""Reject instrumentation build switches and erased report calls in class importers."""

import re
import sys
from pathlib import Path


def violations(root: Path):
    forbidden = re.compile(
        r"FINALE_MUS_READER_ENABLE_INSTRUMENTATION"
        r"|FINALE_MUS_READER_REPORT_\w+"
        r"|^\s*#\s*define\s+(?:report\w*|REPORT_\w+)\([^)]*\)\s*\(\(void\)0\)"
    )
    direct_api = re.compile(
        r"\bValueOrigin\b"
        r"|(?<!:)\b(?:FieldInfo|InstanceKey|TextFieldInfo)\b"
        r"|(?<!template )(?<![.\w:])instanceKey\s*<"
        r"|\b(?:context\.report|report)\.(?:setField|findField|setInstanceOrigin|"
        r"findInstanceOrigin|fieldProvenance|setTextField|fields|instanceOrigins|textFields)\b"
    )
    for pool in ("options", "others", "details", "texts"):
        for path in sorted((root / "src" / "import" / pool).rglob("*")):
            if path.suffix not in (".cpp", ".h"):
                continue
            for number, line in enumerate(path.read_text().splitlines(), 1):
                code = line.split("//", 1)[0]
                if forbidden.search(code) or direct_api.search(code):
                    yield f"{path.relative_to(root)}:{number}: {line.strip()}"


def main():
    root = Path(__file__).resolve().parents[1]
    failures = list(violations(root))
    if failures:
        print("Class importers must use the lazy reporting boundary:", file=sys.stderr)
        print("\n".join(failures), file=sys.stderr)
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
