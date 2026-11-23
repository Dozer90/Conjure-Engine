#!/usr/bin/env python3
"""Documentation site helper (docs/design/16_documentation.md section 4).

    python tools/docs.py install      # create .venv-docs and install the pinned toolchain
    python tools/docs.py serve        # live-reloading preview at http://127.0.0.1:8000
    python tools/docs.py build        # strict build into site/ (broken links fail)
    python tools/docs.py check        # links, alerts, header coverage, scope/module pages
    python tools/docs.py new --help   # create a draft from one of the six page kinds
    python tools/docs.py regenerate   # refresh pages and all public-header details, preserving authored text
    python tools/docs.py assets       # prepare readable, editable CSS and JavaScript sources
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
        print("docs toolchain missing; run: python tools/docs.py install", file=sys.stderr)
        raise SystemExit(2)
    return python


def main(argv: list[str]) -> int:
    if argv and argv[0] == "assets":
        python = ensure_toolchain()
        return run([str(python), "-I", str(ROOT / "tools" / "docs_assets.py"), *argv[1:]])
    if argv and argv[0] == "new":
        return run([sys.executable, "-I", str(ROOT / "tools" / "create_doc.py"), *argv[1:]])
    if argv and argv[0] == "regenerate":
        return run([sys.executable, "-I", str(ROOT / "tools" / "regenerate_docs.py"), *argv[1:]])
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
