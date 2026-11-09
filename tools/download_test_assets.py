#!/usr/bin/env python3
"""Downloads the public test assets listed in tools/test_assets.json into tests/.downloaded.

The engine's test suites never require these files: tests that need them are guarded by
`precondition::DOWNLOADED_ASSETS` and skip when `tests/.downloaded/.assets_ready` is absent
(docs/design/08_testing.md 3.2). Run this once to enable them:

    python tools/download_test_assets.py            # download what is missing, verify the rest
    python tools/download_test_assets.py --verify   # only verify existing files against the lock
    python tools/download_test_assets.py --force    # re-download everything

The first download of each file pins its SHA-256 into tests/.downloaded/assets.lock; later runs
verify against the lock so a changed upstream file is noticed instead of silently used.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "tools" / "test_assets.json"
TARGET = ROOT / "tests" / ".downloaded"
LOCK = TARGET / "assets.lock"
STAMP = TARGET / ".assets_ready"
USER_AGENT = "conjure-test-assets/1.0"


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_lock() -> dict[str, str]:
    if not LOCK.exists():
        return {}
    return json.loads(LOCK.read_text(encoding="utf-8"))


def save_lock(lock: dict[str, str]) -> None:
    LOCK.write_text(json.dumps(dict(sorted(lock.items())), indent=2) + "\n", encoding="utf-8")


def download(url: str, destination: pathlib.Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    temporary = destination.with_suffix(destination.suffix + ".part")
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as output:
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            output.write(chunk)
    temporary.replace(destination)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--verify", action="store_true", help="verify existing files only; download nothing")
    parser.add_argument("--force", action="store_true", help="re-download every asset")
    parser.add_argument("--include-optional", action="store_true", help="also fetch assets marked optional")
    args = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    lock = load_lock()
    failures: list[str] = []
    required_present = True

    for asset in manifest["assets"]:
        if asset.get("disabled"):
            continue
        optional = bool(asset.get("optional"))
        if optional and not args.include_optional and not (TARGET / asset["name"]).exists():
            continue

        destination = TARGET / asset["name"]
        name = asset["name"]

        if args.force or not destination.exists():
            if args.verify:
                print(f"missing   {name}")
                if not optional:
                    required_present = False
                continue
            print(f"download  {name}", flush=True)
            try:
                download(asset["url"], destination)
            except (urllib.error.URLError, OSError) as error:
                failures.append(f"{name}: {error}")
                if not optional:
                    required_present = False
                continue

        digest = sha256_of(destination)
        pinned = lock.get(name)
        if pinned is None:
            lock[name] = digest
            print(f"pinned    {name} {digest[:16]}")
        elif pinned != digest:
            failures.append(f"{name}: sha256 {digest[:16]} does not match the lock {pinned[:16]} (delete the file or the lock entry to accept)")
            if not optional:
                required_present = False
        else:
            print(f"ok        {name}")

    TARGET.mkdir(parents=True, exist_ok=True)
    save_lock(lock)

    if failures:
        print("\n".join(f"FAILED    {failure}" for failure in failures), file=sys.stderr)

    if required_present and not failures:
        STAMP.write_text("ready\n", encoding="utf-8")
        print(f"ready     {STAMP}")
        return 0

    if STAMP.exists():
        STAMP.unlink()
    return 1


if __name__ == "__main__":
    sys.exit(main())
