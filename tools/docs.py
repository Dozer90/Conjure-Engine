#!/usr/bin/env python3
"""Documentation site helper (docs/design/16_documentation.md section 4).

    python tools/docs.py install      # create .venv-docs and install the pinned toolchain
    python tools/docs.py serve        # live-reloading preview at http://127.0.0.1:8000
    python tools/docs.py build        # strict build into site/ (broken links fail)
    python tools/docs.py check        # tools/check_docs.py (structure, badges, coverage, links)
"""

from __future__ import annotations

import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
VENV = ROOT / ".venv-docs"
REQUIREMENTS = ROOT / "tools" / "docs_requirements.txt"


def venv_python() -> pathlib.Path:
    if os.name == "nt":
        return VENV / "Scripts" / "python.exe"
    return VENV / "bin" / "python"


def run(command: list[str]) -> int:
    print("+", " ".join(command), flush=True)
    return subprocess.run(command, cwd=ROOT).returncode


def ensure_toolchain() -> pathlib.Path:
    python = venv_python()
    if not python.exists():
        print(f"docs toolchain missing; run: python {pathlib.Path(__file__).name} install", file=sys.stderr)
        raise SystemExit(2)
    return python


def main(argv: list[str]) -> int:
    if len(argv) != 1 or argv[0] not in {"install", "serve", "build", "check"}:
        print(__doc__)
        return 2

    action = argv[0]
    if action == "install":
        if not venv_python().exists():
            code = run([sys.executable, "-m", "venv", str(VENV)])
            if code != 0:
                return code
        return run([str(venv_python()), "-m", "pip", "install", "-r", str(REQUIREMENTS)])

    if action == "check":
        return run([sys.executable, "-I", str(ROOT / "tools" / "check_docs.py")])

    python = ensure_toolchain()
    if action == "serve":
        return run([str(python), "-m", "mkdocs", "serve"])
    return run([str(python), "-m", "mkdocs", "build", "--strict"])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
