#!/usr/bin/env python3
"""Self-test for tools/download_test_assets.py and the manifest tools/test_assets.json.

Points the downloader's manifest, lock, target directory and stamp at a temporary directory, replaces the
network transfer with fakes, and asserts the stamp, exit-code and partial-file rules; then checks the real
manifest's entries. Runs as the CTest entry `test_assets.downloader_tool` and never touches the network; no
third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import json
import pathlib
import sys
import tempfile
import urllib.parse

HERE = pathlib.Path(__file__).resolve().parent
DOWNLOADER = HERE.parent / "download_test_assets.py"
REAL_MANIFEST = HERE.parent / "test_assets.json"

REQUIRED_NAME = "models/required.glb"
OPTIONAL_NAME = "models/optional.glb"
REQUIRED_BYTES = b"required asset"


def load_downloader():
    specification = importlib.util.spec_from_file_location("download_test_assets", DOWNLOADER)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def point_at(downloader, root: pathlib.Path) -> None:
    """Redirects every path the downloader writes to 'root' and writes a manifest with one required and one
    optional asset, the required one pinned in the lock."""
    downloader.ROOT = root
    downloader.MANIFEST = root / "tools" / "test_assets.json"
    downloader.LOCK = root / "tools" / "test_assets.lock"
    downloader.TARGET = root / "tests" / ".downloaded"
    downloader.STAMP = root / "tests" / ".assets_ready"
    downloader.MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    downloader.MANIFEST.write_text(json.dumps({"assets": [
        {"name": REQUIRED_NAME, "url": "https://assets.invalid/required.glb"},
        {"name": OPTIONAL_NAME, "url": "https://assets.invalid/optional.glb", "optional": True},
    ]}), encoding="utf-8")
    downloader.LOCK.write_text(json.dumps({REQUIRED_NAME: hashlib.sha256(REQUIRED_BYTES).hexdigest()}),
                               encoding="utf-8")


def run_quietly(downloader, argv: list[str]) -> int:
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return downloader.main(argv)


def optional_failure_keeps_stamp(downloader, root: pathlib.Path) -> None:
    point_at(downloader, root)

    def fake_download(url: str, destination: pathlib.Path) -> None:
        if "optional" in url:
            raise OSError("optional host unreachable")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(REQUIRED_BYTES)

    downloader.download = fake_download
    code = run_quietly(downloader, ["--include-optional"])
    assert code == 1, f"exit code {code}, expected 1 for the failed optional asset"
    assert downloader.STAMP.exists(), "the stamp of the complete required assets was withdrawn"
    assert not (downloader.TARGET / OPTIONAL_NAME).exists(), "the failed optional asset exists"


def required_failure_removes_stamp(downloader, root: pathlib.Path) -> None:
    point_at(downloader, root)
    downloader.STAMP.parent.mkdir(parents=True, exist_ok=True)
    downloader.STAMP.write_text("ready\n", encoding="utf-8")

    def failing_download(url: str, destination: pathlib.Path) -> None:
        raise OSError(f"{url} unreachable")

    downloader.download = failing_download
    code = run_quietly(downloader, [])
    assert code == 1, f"exit code {code}, expected 1"
    assert not downloader.STAMP.exists(), "the stamp survived a missing required asset"


def verify_with_missing_required_asset_fails(downloader, root: pathlib.Path) -> None:
    point_at(downloader, root)
    code = run_quietly(downloader, ["--verify"])
    assert code == 1, f"exit code {code}, expected 1 for a missing required asset"
    assert not downloader.STAMP.exists(), "the stamp was written without the required asset"


def force_with_verify_is_usage_error(downloader) -> None:
    try:
        run_quietly(downloader, ["--force", "--verify"])
    except SystemExit as exit_request:
        assert exit_request.code == 2, f"exit code {exit_request.code}, expected 2"
        return
    raise AssertionError("--force --verify was accepted")


class failing_response:
    """A urlopen() result whose first read succeeds and whose second read fails mid-transfer."""

    def __init__(self) -> None:
        self.reads = 0

    def __enter__(self) -> "failing_response":
        return self

    def __exit__(self, *exception) -> None:
        return None

    def read(self, size: int) -> bytes:
        self.reads += 1
        if self.reads > 1:
            raise OSError("connection reset")
        return b"partial"


def failed_transfer_leaves_no_part_file(downloader, real_download, root: pathlib.Path) -> None:
    destination = root / "transfer" / "asset.glb"
    original_urlopen = downloader.urllib.request.urlopen
    downloader.urllib.request.urlopen = lambda request, timeout: failing_response()
    try:
        real_download("https://assets.invalid/asset.glb", destination)
    except OSError:
        pass
    else:
        raise AssertionError("the failed transfer did not raise")
    finally:
        downloader.urllib.request.urlopen = original_urlopen
    leftovers = sorted(path.name for path in destination.parent.iterdir())
    assert leftovers == [], f"files left behind: {leftovers}"


def manifest_names_match_urls() -> None:
    manifest = json.loads(REAL_MANIFEST.read_text(encoding="utf-8"))
    for asset in manifest["assets"]:
        if asset.get("disabled"):
            continue
        url_suffix = pathlib.PurePosixPath(urllib.parse.urlsplit(asset["url"]).path).suffix.lower()
        name_suffix = pathlib.PurePosixPath(asset["name"]).suffix.lower()
        assert url_suffix == name_suffix, f"{asset['name']} is saved from a {url_suffix or 'suffix-less'} URL"


def manifest_has_no_placeholder() -> None:
    manifest = json.loads(REAL_MANIFEST.read_text(encoding="utf-8"))
    disabled = [asset["name"] for asset in manifest["assets"] if asset.get("disabled")]
    assert not disabled, f"disabled placeholder entries: {disabled} (add a real asset or remove the entry)"


def main() -> int:
    downloader = load_downloader()
    real_download = downloader.download
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="download_test_assets_") as directory:
        temporary = pathlib.Path(directory)
        cases = [
            ("download_test_assets - an optional download failure keeps the stamp of complete required assets",
             lambda: optional_failure_keeps_stamp(downloader, temporary / "optional")),
            ("download_test_assets - a missing required asset removes the stamp",
             lambda: required_failure_removes_stamp(downloader, temporary / "required")),
            ("download_test_assets - --verify with a missing required asset fails without a stamp",
             lambda: verify_with_missing_required_asset_fails(downloader, temporary / "verify")),
            ("download_test_assets - --force with --verify is a usage error",
             lambda: force_with_verify_is_usage_error(downloader)),
            ("download_test_assets - a failed transfer leaves no .part file",
             lambda: failed_transfer_leaves_no_part_file(downloader, real_download, temporary)),
            ("test_assets - every enabled entry is saved under the extension of its URL", manifest_names_match_urls),
            ("test_assets - no entry is a disabled placeholder", manifest_has_no_placeholder),
        ]
        for title, case in cases:
            try:
                case()
            except Exception as failure:
                failures.append(f"{title}: {type(failure).__name__}: {failure}")
                continue
            print(f"ok: {title}")

    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"test_assets.downloader_tool: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
