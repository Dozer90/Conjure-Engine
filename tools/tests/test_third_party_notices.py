#!/usr/bin/env python3
"""Self-test for tools/third_party_notices.py.

Every case builds its own manifest and licence files in a temporary directory, the way a configure writes them
(cmake/ConjureNotices.cmake), runs the tool's main() on them and reads the document it wrote: the table and the
sections, the normalisation of licence text, the merge with a tracked document (kept, missing, stale and dropped
sections), --check, software found on the build machine, and the manifests and arguments the tool refuses. One
case reads the repository's own THIRD_PARTY_NOTICES.md, LICENSE and NOTICE and holds them to the same rules without
a build tree. Runs as the CTest entry `notices.tool`; no third-party test framework is needed.
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
TOOL = HERE.parent / "third_party_notices.py"
REPOSITORY = HERE.parent.parent
MIT_TEXT = "MIT License\n\nCopyright (c) 2020 Somebody\n\nPermission is hereby granted, free of charge."


def load_tool():
    specification = importlib.util.spec_from_file_location("third_party_notices", TOOL)
    module = importlib.util.module_from_spec(specification)
    assert specification.loader is not None
    specification.loader.exec_module(module)
    return module


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def notice(directory: pathlib.Path, identifier: str, name: str, *, version: str = "1.0", licence: str = "MIT",
           scope: str = "runtime", url: str = "https://example.org/source", note: str = "", used: bool = True,
           files: dict[str, bytes] | None = None, host_provided: bool = False,
           host_files: dict[str, bytes] | None = None) -> dict:
    """One manifest entry; its licence files are written under <directory>/deps/<identifier>/."""
    entries: dict[str, list[dict]] = {"files": [], "hostFiles": []}
    for group, contents in (("files", files), ("hostFiles", host_files)):
        for label, data in (contents or {}).items():
            path = directory / "deps" / identifier / label
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            entries[group].append({"label": label, "path": path.as_posix()})
    return {"id": identifier, "name": name, "version": version, "license": licence, "url": url, "note": note,
            "scope": scope, "used": used, "shipped": used and scope != "build", "hostProvided": host_provided,
            "files": entries["files"], "hostFiles": entries["hostFiles"]}


def write_manifest(directory: pathlib.Path, notices: list[dict], name: str = "manifest.json") -> pathlib.Path:
    path = directory / name
    path.write_text(json.dumps({"version": 1, "sourceDirectory": directory.as_posix(), "notices": notices}),
                    encoding="utf-8")
    return path


def run_tool(tool, arguments: list[str]) -> tuple[int, str, str]:
    output = io.StringIO()
    errors = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
        try:
            code = tool.main(arguments)
        except SystemExit as stop:
            code = int(stop.code or 0)
    return code, output.getvalue(), errors.getvalue()


def generate(tool, directory: pathlib.Path, notices: list[dict], *extra: str) -> tuple[int, str, str]:
    manifest = write_manifest(directory, notices)
    return run_tool(tool, ["--manifest", str(manifest), "--output", str(directory / "NOTICES.md"), *extra])


def document(directory: pathlib.Path) -> str:
    return (directory / "NOTICES.md").read_bytes().decode("utf-8")


def writes_a_table_and_a_section_per_notice(tool, directory: pathlib.Path) -> None:
    notices = [
        notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.encode()}),
        notice(directory, "beta", "Beta Tools", version="v2", licence="Apache-2.0 OR MIT", scope="editor",
               note="Two licences.", files={"LICENSE.md": b"Choose one.", "LICENSES/MIT.txt": b"MIT words"}),
        notice(directory, "gamma", "Gamma", version="", licence="Vendor terms", scope="build",
               note="A tool the build runs."),
    ]
    code, output, errors = generate(tool, directory, notices)
    check(code == 0 and not errors, f"a complete manifest is written: {code} {errors}")
    check("wrote" in output and "3 notices" in output, f"the tool says what it wrote: {output}")
    text = document(directory)
    check(text.startswith("# Third-party notices\n"), "the document opens with its title")
    check("\r" not in text and text.endswith("\n"), "the document has LF line ends and a final one")
    rows = [line for line in text.split("\n") if line.startswith("| [")]
    check(rows == [
        "| [Alpha](#notice-alpha) | `1.0` | MIT | Runtime |",
        "| [Beta Tools](#notice-beta) | `v2` | Apache-2.0 OR MIT | Editor |",
        "| [Gamma](#notice-gamma) |  | Vendor terms | Build |",
    ], f"the table lists the notices in the manifest's order: {rows}")
    for heading in ("## Alpha", "## Beta Tools", "## Gamma"):
        check(f"\n{heading}\n" in text, f"{heading} has a section")
    check('<a id="notice-beta"></a>' in text, "a section carries the anchor the table links to")
    check("- Version: `v2`\n- Licence: Apache-2.0 OR MIT\n- Source: <https://example.org/source>\n"
          "- Ends up in: Editor\n" in text, "a section states version, licence, source and scope")
    check(f"`LICENSE`:\n\n```text\n{MIT_TEXT}\n```\n" in text, "the licence text is printed whole in a fence")
    check(text.index("`LICENSE.md`:") < text.index("`LICENSES/MIT.txt`:"), "several files keep their order")
    check("Two licences.\n" in text and "A tool the build runs.\n" in text, "notes are printed")
    gamma = text[text.index("## Gamma"):]
    check("- Version" not in gamma.split("<!--")[0], "a notice without a pin prints no version")
    check("No licence text is reproduced here" in gamma, "a notice without licence files says so")
    sections = tool.parse_sections(text)
    check(list(sections) == ["alpha", "beta", "gamma"], "every section is found again")
    check(sections["beta"][0] == tool.metadata_of(notices[1]), "a section remembers the notice it was written for")
    check(text.rstrip("\n").endswith("`samples/assets/README.md`."), "the closing paragraph follows the sections")


def licence_text_is_normalised(tool, directory: pathlib.Path) -> None:
    raw = b"\xef\xbb\xbf\r\n  \r\n      Centred Title\r\nline two  \rline three\r\n\r\n \t\r\n"
    code, _, errors = generate(tool, directory, [notice(directory, "alpha", "Alpha", files={"LICENSE": raw})])
    check(code == 0, f"a file with a byte order mark and mixed line ends is read: {errors}")
    text = document(directory)
    check("```text\n      Centred Title\nline two  \nline three\n```\n" in text,
          "the mark, the blank lines around the text and CR are gone, the first line keeps its indentation")
    accented = "Copyright Camilla Löwy".encode("utf-8")
    code, _, _ = generate(tool, directory, [notice(directory, "alpha", "Alpha", files={"LICENSE": accented})])
    check(code == 0 and "Camilla Löwy" in document(directory), "UTF-8 text stays UTF-8")
    before = (directory / "NOTICES.md").read_bytes()
    code, _, errors = generate(tool, directory, [notice(directory, "alpha", "Alpha", files={"LICENSE": b"L\xf6wy"})])
    check(code == 1 and "not UTF-8" in errors, f"a file in another encoding is refused: {code} {errors}")
    code, _, errors = generate(tool, directory, [notice(directory, "alpha", "Alpha", files={"LICENSE": b" \r\n\n"})])
    check(code == 1 and "empty" in errors, f"an empty licence file is refused: {code} {errors}")
    check((directory / "NOTICES.md").read_bytes() == before, "a refused manifest leaves the document alone")


def fence_is_longer_than_any_backtick_run(tool, directory: pathlib.Path) -> None:
    licence = b"Example:\n```\nSPDX-License-Identifier: MIT\n```\nand ```` four."
    code, _, _ = generate(tool, directory, [notice(directory, "alpha", "Alpha", files={"LICENSE.md": licence})])
    check(code == 0, "a licence text with code fences is written")
    check(f"`````text\n{licence.decode()}\n`````\n" in document(directory),
          "the fence is one backtick longer than the longest run in the text")
    check(tool.fence_for("plain") == "```" and tool.fence_for("a ` b") == "```", "three backticks are the least")


def check_follows_the_licence_files(tool, directory: pathlib.Path) -> None:
    notices = [notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.encode()})]
    code, _, errors = generate(tool, directory, notices, "--check")
    check(code == 1 and "out of date" in errors, f"--check fails while there is no document: {code} {errors}")
    check(not (directory / "NOTICES.md").exists(), "--check writes nothing")
    check(generate(tool, directory, notices)[0] == 0, "the document is written")
    code, output, errors = generate(tool, directory, notices, "--check")
    check(code == 0 and "up to date" in output and not errors, f"--check passes after a write: {code} {errors}")
    code, output, _ = generate(tool, directory, notices)
    check(code == 0 and "up to date" in output, "a second write has nothing to do")

    written = (directory / "NOTICES.md").read_bytes()
    changed = [notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.replace("2020", "2021").encode()})]
    code, _, errors = generate(tool, directory, changed, "--check")
    check(code == 1 and "out of date" in errors, f"a changed licence file fails --check: {code} {errors}")
    check("-Copyright (c) 2020 Somebody" in errors and "+Copyright (c) 2021 Somebody" in errors,
          f"--check shows what changed: {errors}")
    check("--manifest" in errors, "--check names the command that regenerates the document")
    check((directory / "NOTICES.md").read_bytes() == written, "--check leaves the document alone")
    for label, other in (("pin", {"version": "1.1"}), ("note", {"note": "New."}), ("licence", {"licence": "Zlib"})):
        varied = [notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.encode()}, **other)]
        check(generate(tool, directory, varied, "--check")[0] == 1, f"a changed {label} fails --check")
    check(generate(tool, directory, changed)[0] == 0, "the document is written again")
    check(generate(tool, directory, changed, "--check")[0] == 0, "and --check passes again")


def unused_notice_keeps_its_section(tool, directory: pathlib.Path) -> None:
    files = {"LICENSE": MIT_TEXT.encode()}
    everything = [notice(directory, "alpha", "Alpha", files=files),
                  notice(directory, "beta", "Beta", version="v7", note="Fetched elsewhere.", files=files)]
    check(generate(tool, directory, everything)[0] == 0, "a tree that fetches everything writes the document")
    written = (directory / "NOTICES.md").read_bytes()
    (directory / "deps" / "beta" / "LICENSE").unlink()

    for label, version in (("with the pin", "v7"), ("without knowing the pin", "")):
        partial = [notice(directory, "alpha", "Alpha", files=files),
                   notice(directory, "beta", "Beta", version=version, note="Fetched elsewhere.", used=False)]
        code, _, errors = generate(tool, directory, partial, "--check")
        check(code == 0, f"a tree that does not fetch Beta passes --check ({label}): {errors}")
        check(generate(tool, directory, partial)[0] == 0, f"and writes ({label})")
        check((directory / "NOTICES.md").read_bytes() == written,
              f"the section and the table row of Beta are kept byte for byte ({label})")


def unused_notice_needs_a_matching_section(tool, directory: pathlib.Path) -> None:
    files = {"LICENSE": MIT_TEXT.encode()}
    unused = [notice(directory, "alpha", "Alpha", files=files), notice(directory, "beta", "Beta", used=False)]
    for arguments in ((), ("--check",)):
        code, _, errors = generate(tool, directory, unused, *arguments)
        check(code == 1 and "Beta" in errors and "no section" in errors,
              f"a notice the tree does not use and the document does not have is an error: {code} {errors}")
        check(not (directory / "NOTICES.md").exists(), "and nothing is written")

    everything = [notice(directory, "alpha", "Alpha", files=files), notice(directory, "beta", "Beta", files=files)]
    check(generate(tool, directory, everything)[0] == 0, "the document is written with both")
    written = (directory / "NOTICES.md").read_bytes()
    for label, other in (("pin", {"version": "2.0"}), ("note", {"note": "Changed."}), ("licence", {"licence": "Zlib"}),
                         ("scope", {"scope": "build"}), ("name", {"name": "Beta 2"}),
                         ("source", {"url": "https://example.org/elsewhere"})):
        fields = {"name": "Beta", **other}
        stale = [notice(directory, "alpha", "Alpha", files=files),
                 notice(directory, "beta", fields.pop("name"), used=False, **fields)]
        for arguments in ((), ("--check",)):
            code, _, errors = generate(tool, directory, stale, *arguments)
            check(code == 1 and "written for another" in errors,
                  f"a kept section written for another {label} is an error: {code} {errors}")
        check((directory / "NOTICES.md").read_bytes() == written, f"and the document is untouched ({label})")


def unregistered_section_is_dropped(tool, directory: pathlib.Path) -> None:
    files = {"LICENSE": MIT_TEXT.encode()}
    both = [notice(directory, "alpha", "Alpha", files=files), notice(directory, "beta", "Beta", files=files)]
    check(generate(tool, directory, both)[0] == 0, "the document is written with both")
    one = [notice(directory, "alpha", "Alpha", files=files)]
    code, _, errors = generate(tool, directory, one, "--check")
    check(code == 1 and "out of date" in errors, "a section whose notice is gone fails --check")
    code, output, _ = generate(tool, directory, one)
    check(code == 0 and "dropped the section of Beta" in output, f"a write drops it and says so: {output}")
    text = document(directory)
    check("Beta" not in text and "## Alpha" in text, "the section and the table row are gone")
    check(generate(tool, directory, one, "--check")[0] == 0, "and --check passes")


def host_provided_text_is_never_written(tool, directory: pathlib.Path) -> None:
    host = notice(directory, "host", "Host Runtime", version="", licence="MIT, binaries under other terms",
                  note="Found on the build machine.", host_provided=True,
                  host_files={"LICENSE.txt": b"TEXT OF THIS MACHINE"})
    check(generate(tool, directory, [host])[0] == 0, "software found on the build machine is written")
    text = document(directory)
    check("TEXT OF THIS MACHINE" not in text, "the licence text of the build machine is not in the document")
    check("| [Host Runtime](#notice-host) | installed | MIT, binaries under other terms | Runtime |" in text,
          "the table says the version is the installed one")
    check("- Version: the one installed on the machine that builds the engine" in text, "and so does the section")
    check("Its licence text is not part of this document" in text and "`THIRD_PARTY_NOTICES.txt`" in text,
          "the section says where the text is written instead")
    written = (directory / "NOTICES.md").read_bytes()
    elsewhere = notice(directory, "host", "Host Runtime", version="", licence="MIT, binaries under other terms",
                       note="Found on the build machine.", host_provided=True, used=False)
    check(generate(tool, directory, [elsewhere])[0] == 0, "a tree without it keeps the section")
    check((directory / "NOTICES.md").read_bytes() == written, "which reads the same on every machine")


def markdown_is_escaped(tool, directory: pathlib.Path) -> None:
    note = "vcruntime140*.dll and msvcp140*.dll, stb_image [1] <b> a|b `c` \\d"
    odd = notice(directory, "odd", "snake_case*", licence="A_B", note=note, files={"LICENSE": b"text"})
    check(generate(tool, directory, [odd])[0] == 0, "a note with Markdown characters is written")
    text = document(directory)
    check("vcruntime140\\*.dll and msvcp140\\*.dll, stb\\_image \\[1\\] \\<b\\> a\\|b \\`c\\` \\\\d\n" in text,
          "the note is shown literally")
    check("## snake\\_case\\*\n" in text and "- Licence: A\\_B\n" in text, "so are the name and the licence")
    check(tool.parse_sections(text)["odd"][0]["note"] == note, "the marker keeps the text as it was registered")
    dashes = notice(directory, "odd", "Odd", note="a -- b", files={"LICENSE": b"text"})
    code, _, errors = generate(tool, directory, [dashes])
    check(code == 1 and "'--'" in errors, f"a text the section marker cannot carry is refused: {code} {errors}")


def bad_manifests_are_refused(tool, directory: pathlib.Path) -> None:
    output = directory / "NOTICES.md"
    good = notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.encode()})

    def refused(label: str, content: str, expected: str) -> None:
        manifest = directory / "bad.json"
        manifest.write_text(content, encoding="utf-8")
        code, _, errors = run_tool(tool, ["--manifest", str(manifest), "--output", str(output)])
        check(code == 1 and expected in errors, f"{label}: expected '{expected}', got {code} {errors}")
        check(not output.exists(), f"{label}: nothing is written")

    code, _, errors = run_tool(tool, ["--manifest", str(directory / "absent.json"), "--output", str(output)])
    check(code == 1 and "configure the build tree first" in errors, f"a missing manifest is named: {errors}")
    refused("not JSON", "{", "not a notices manifest")
    refused("another version", json.dumps({"version": 2, "notices": []}), "version 1")
    refused("no list", json.dumps({"version": 1, "notices": {}}), "'notices' is not a list")
    without_scope = {key: value for key, value in good.items() if key != "scope"}
    refused("a missing member", json.dumps({"version": 1, "notices": [without_scope]}), "lacks one of")
    refused("an unknown scope", json.dumps({"version": 1, "notices": [{**good, "scope": "server"}]}), "unknown scope")
    refused("a notice twice", json.dumps({"version": 1, "notices": [good, good]}), "listed twice")
    gone = {**good, "files": [{"label": "LICENSE", "path": (directory / "deps" / "nowhere" / "LICENSE").as_posix()}]}
    refused("a licence file that is gone", json.dumps({"version": 1, "notices": [gone]}),
            "configure the build tree again")

    check(generate(tool, directory, [good])[0] == 0, "a good manifest is written")
    for label, damage in (("a marker that is not JSON", lambda text: text.replace('{"hostProvided"', '{hostProvided')),
                          ("a marker without an id", lambda text: text.replace('"id": "alpha", ', "")),
                          ("two sections of one notice", lambda text: text + text[text.index("<!-- notice: "):])):
        output.write_text(damage(document(directory)), encoding="utf-8", newline="\n")
        code, _, errors = generate(tool, directory, [good])
        check(code == 1 and "NOTICES" in errors.upper(), f"a document with {label} is refused: {code} {errors}")
        output.unlink()
        check(generate(tool, directory, [good])[0] == 0, "and a fresh document is written")


def arguments_are_checked(tool, directory: pathlib.Path) -> None:
    code, _, errors = run_tool(tool, [])
    check(code == 2 and "--build-dir" in errors, f"a manifest or a build tree is required: {code} {errors}")
    code, _, _ = run_tool(tool, ["--build-dir", str(directory), "--manifest", str(directory / "m.json")])
    check(code == 2, "both at once are refused")
    tree = directory / "tree"
    (tree / "notices").mkdir(parents=True)
    notices = [notice(directory, "alpha", "Alpha", files={"LICENSE": MIT_TEXT.encode()})]
    write_manifest(tree / "notices", notices, "third_party_notices.json")
    code, _, errors = run_tool(tool, ["--build-dir", str(tree), "--output", str(directory / "NOTICES.md")])
    check(code == 0 and "## Alpha" in document(directory), f"--build-dir finds the manifest of the tree: {errors}")
    code, _, errors = run_tool(tool, ["--build-dir", str(directory / "no_tree"), "--check"])
    check(code == 1 and "third_party_notices.json" in errors, f"a tree without a manifest is named: {errors}")


def the_repository_documents_are_consistent(tool, directory: pathlib.Path) -> None:
    licence = (REPOSITORY / "LICENSE").read_text(encoding="utf-8")
    check("Apache License" in licence and "Version 2.0, January 2004" in licence, "LICENSE is the Apache licence 2.0")
    check("END OF TERMS AND CONDITIONS" in licence, "LICENSE holds the whole text")
    notice_text = (REPOSITORY / "NOTICE").read_text(encoding="utf-8")
    check("the Conjure contributors" in notice_text, "NOTICE names the Conjure contributors")
    check("Apache License, Version 2.0" in notice_text, "NOTICE names the licence")

    tracked = (REPOSITORY / tool.DOCUMENT_NAME).read_bytes().decode("utf-8")
    check("\r" not in tracked, f"{tool.DOCUMENT_NAME} has LF line ends")
    sections = tool.parse_sections(tracked)
    check(len(sections) >= 10, f"{tool.DOCUMENT_NAME} has a section per dependency, found {len(sections)}")
    # A manifest of a tree that uses none of them: the tool has to reproduce the document from its own sections,
    # which fails when the table, the order or the text around the sections was edited by hand.
    unused = [{**metadata, "used": False, "shipped": False, "files": [], "hostFiles": []}
              for metadata, _ in sections.values()]
    rebuilt, problems = tool.build_document({"version": 1, "notices": unused}, tracked)
    check(not problems, f"every section matches its own marker: {problems}")
    check(rebuilt == tracked, f"{tool.DOCUMENT_NAME} is what the tool writes; regenerate it instead of editing it")
    for identifier, (metadata, text) in sections.items():
        pinned = not metadata["hostProvided"] and "No licence text is reproduced here" not in text
        if pinned:
            check("```text\n" in text, f"the section of {identifier} carries a licence text")
        if metadata["scope"] != "build":
            check(metadata["url"] != "", f"{identifier} ships with binaries and names its source")
    # Code under a copyright line of its own inside a pinned source keeps that line (06_engine_loop.md D15): the
    # jsmn tokenizer cgltf.h embeds, and the ProggyClean font Dear ImGui embeds.
    check("cgltf" in sections and "imgui" in sections, f"{tool.DOCUMENT_NAME} has the cgltf and imgui sections")
    check("\nCopyright (c) 2010 Serge A. Zaitsev\n" in sections["cgltf"][1],
          "the cgltf section carries the licence text of the jsmn it embeds, under its copyright line")
    check("Copyright (c) 2004, 2005 Tristan Grimmer" in sections["imgui"][1],
          "the imgui section names the copyright of the ProggyClean font it embeds")
    del directory


def main() -> int:
    tool = load_tool()
    cases = [
        ("a table and a section per notice", writes_a_table_and_a_section_per_notice),
        ("licence text is normalised and must be UTF-8", licence_text_is_normalised),
        ("the fence is longer than any run of backticks", fence_is_longer_than_any_backtick_run),
        ("--check follows the licence files and the registered facts", check_follows_the_licence_files),
        ("a notice the tree does not use keeps its section", unused_notice_keeps_its_section),
        ("a notice the tree does not use needs a section written for it", unused_notice_needs_a_matching_section),
        ("a section whose notice is gone is dropped", unregistered_section_is_dropped),
        ("licence text of the build machine is never written", host_provided_text_is_never_written),
        ("Markdown characters are shown literally", markdown_is_escaped),
        ("bad manifests and documents are refused", bad_manifests_are_refused),
        ("arguments are checked", arguments_are_checked),
        ("the repository's LICENSE, NOTICE and notices document are consistent",
         the_repository_documents_are_consistent),
    ]
    failures: list[str] = []
    for title, case in cases:
        with tempfile.TemporaryDirectory(prefix="conjure_notices_") as scratch:
            try:
                case(tool, pathlib.Path(scratch))
            except Exception as failure:
                failures.append(f"third_party_notices.py - {title}: {type(failure).__name__}: {failure}")
                continue
        print(f"ok: third_party_notices.py - {title}")
    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures))
        return 1
    print(f"notices.tool: {len(cases)} case(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
