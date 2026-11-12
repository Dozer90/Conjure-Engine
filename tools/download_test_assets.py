#!/usr/bin/env python3
"""Downloads the public test assets listed in tools/test_assets.json into tests/.downloaded.

The engine's test suites never require these files: tests that need them are guarded by
`precondition::DOWNLOADED_ASSETS` and skip when `tests/.assets_ready` is absent
(docs/design/08_testing.md 3.2). Run this once to enable them:

    python tools/download_test_assets.py            # download what is missing, verify the rest
    python tools/download_test_assets.py --verify   # only verify existing files against the lock
    python tools/download_test_assets.py --locked   # like the default, but an asset the lock does not pin fails
    python tools/download_test_assets.py --force    # re-download everything

The SHA-256 of every file is pinned in tools/test_assets.lock, which is committed: a downloaded file is
verified against it, a mismatch fails, and a file the lock does not know yet is pinned with a reminder to
commit the lock (so the pins come from a reviewed change, not from whatever upstream served a CI runner on a
cache miss). --verify and --locked treat an unpinned asset as a failure; --verify and --force are exclusive.

tests/.assets_ready is written whenever every required asset is present and verified, and removed otherwise.
The exit code is 0 only when every required asset is present and nothing failed: an optional asset that failed
(--include-optional) exits 1 but keeps the stamp. A failed or interrupted transfer leaves no partial file behind.
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
LOCK = ROOT / "tools" / "test_assets.lock"       # committed: asset name -> sha256
TARGET = ROOT / "tests" / ".downloaded"
STAMP = TARGET.parent / ".assets_ready"   # tests/.assets_ready: where conjure::testing looks (precondition::DOWNLOADED_ASSETS)
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
    """Fetches 'url' into 'destination' through a .part file, which a failed or interrupted transfer removes."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    temporary = destination.with_suffix(destination.suffix + ".part")
    try:
        with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as output:
            while True:
                chunk = response.read(1 << 20)
                if not chunk:
                    break
                output.write(chunk)
        temporary.replace(destination)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--verify", action="store_true", help="verify existing files only; download nothing")
    parser.add_argument("--locked", action="store_true",
                        help="fail on an asset tools/test_assets.lock does not pin instead of pinning it")
    mode.add_argument("--force", action="store_true", help="re-download every asset")
    parser.add_argument("--include-optional", action="store_true", help="also fetch assets marked optional")
    args = parser.parse_args(argv)

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    lock = load_lock()
    failures: list[str] = []
    required_present = True
    newly_pinned: list[str] = []

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
            if args.verify or args.locked:
                failures.append(f"{name}: not pinned in {LOCK.relative_to(ROOT).as_posix()} (run without --verify/--locked and commit the lock)")
                if not optional:
                    required_present = False
            else:
                lock[name] = digest
                newly_pinned.append(name)
                print(f"pinned    {name} {digest[:16]}")
        elif pinned != digest:
            failures.append(f"{name}: sha256 {digest[:16]} does not match the lock {pinned[:16]} (a changed upstream file; re-pin by removing the lock entry only if the change is intended)")
            if not optional:
                required_present = False
        else:
            print(f"ok        {name}")

    TARGET.mkdir(parents=True, exist_ok=True)
    if newly_pinned:
        save_lock(lock)
        print(f"NOTE      {len(newly_pinned)} asset(s) pinned for the first time: commit {LOCK.relative_to(ROOT).as_posix()}")

    if failures:
        print("\n".join(f"FAILED    {failure}" for failure in failures), file=sys.stderr)

    # The stamp says the required assets are there and verified; an optional asset that failed does not
    # withdraw it, but still fails the run.
    if required_present:
        STAMP.write_text("ready\n", encoding="utf-8")
        print(f"ready     {STAMP}")
    else:
        STAMP.unlink(missing_ok=True)
    return 0 if required_present and not failures else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
