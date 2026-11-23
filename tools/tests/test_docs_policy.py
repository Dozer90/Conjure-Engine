#!/usr/bin/env python3
"""Planning citations must not leak into published prose or regenerated source summaries."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import docs_policy


class PublicationPolicyTests(unittest.TestCase):
    def test_fenced_examples_inside_callouts_remain_literal(self):
        for prefix in ("> ", "> > "):
            for fence in ("```", "~~~~"):
                with self.subTest(prefix=prefix, fence=fence):
                    example = (prefix + fence + "bash\n"
                               + prefix + 'printf "two  spaces; (R7)"\n'
                               + prefix + "python tools/build.py --test\n"
                               + prefix + fence + "\n")
                    source = "> [!TIP]\n> Useful commands:\n>\n" + example + "\nUse the owner thread (R7)."
                    expected = source.replace("Use the owner thread (R7).", "Use the owner thread.")
                    cleaned = docs_policy.sanitize(source)
                    self.assertEqual(cleaned, expected)
                    self.assertEqual(docs_policy.sanitize(cleaned), expected)

    def test_public_architecture_and_design_paths_are_not_planning_citations(self):
        text = "[Architecture](architecture-and-design/architecture.md)"
        self.assertEqual(docs_policy.findings(text), [])
        self.assertEqual(docs_policy.sanitize(text), text)
        route = "redirect_to: architecture-and-design/data-oriented-design.md"
        self.assertEqual(docs_policy.sanitize(route), route)
        self.assertTrue(docs_policy.findings("[Internal](../design/12_audio.md)"))

    def test_backend_description_retains_the_contract(self):
        text = ("The backend interface (docs/design/12_audio.md 4.3): the piece that owns a device "
                "(or a virtual clock) and drives the mix callback. Implementations: miniaudio "
                "(real devices, device thread) and null (headless, main thread). "
                "Every method is main-thread only except state().")
        cleaned = docs_policy.sanitize(text)
        self.assertIn("The backend interface: the piece that owns a device", cleaned)
        self.assertIn("(or a virtual clock)", cleaned)
        self.assertIn("Every method is main-thread only except state().", cleaned)
        self.assertEqual(docs_policy.findings(cleaned), [])

    def test_wave_and_cross_area_references_are_removed(self):
        text = ("The module never includes ecs: the engine's audio_subsystem (wave 4) registers "
                "the components and owns the engine.audio_sync and engine.audio_finish_one_shots "
                "systems (09_cross_area_resolutions.md R7).")
        cleaned = docs_policy.sanitize(text)
        self.assertIn("audio_subsystem registers the components", cleaned)
        self.assertIn("engine.audio_finish_one_shots systems.", cleaned)
        self.assertEqual(docs_policy.findings(cleaned), [])

    def test_mixed_parenthesis_keeps_technical_qualification(self):
        text = "The callback is stable (R31; only the owner thread can replace it)."
        cleaned = docs_policy.sanitize(text)
        self.assertIn("only the owner thread can replace it", cleaned)
        self.assertEqual(docs_policy.findings(cleaned), [])

    def test_multiline_citation_does_not_leave_empty_parentheses(self):
        text = "Registers components into the ECS (wave 4,\ndocs/design/06_engine_loop.md 3.4)."
        self.assertEqual(docs_policy.sanitize(text), "Registers components into the ECS.")

    def test_normal_runtime_and_section_names_are_preserved(self):
        text = "Use a .NET 8 host and read section 4.2 for the state() contract."
        self.assertEqual(docs_policy.sanitize(text), text)

    def test_public_architecture_link_is_preserved(self):
        text = "See [Architecture](../architecture.md) for the module map."
        self.assertEqual(docs_policy.sanitize(text), text)
        self.assertEqual(docs_policy.findings(text), [])

    def test_source_comparisons_become_current_contracts(self):
        for text, expected in (
            ("The only glm include (design rule C6); use explicit constructors.",
             "The only glm include; use explicit constructors."),
            ("Bake sizes; the defaults are the design's (R13). Tests use small sizes.",
             "Bake sizes and their defaults. Tests use small sizes."),
            ("Uses the viewport's width over height (the design's signature omitted it, but a\n"
             "non-square viewport cannot be unprojected without it).",
             "Uses the viewport's width over height (a non-square viewport requires the aspect ratio for unprojection)."),
            ("Startup order (06 3.4): built-ins, then plugins.",
             "Startup order: built-ins, then plugins."),
        ):
            cleaned = docs_policy.sanitize(text)
            self.assertEqual(cleaned, expected)
            self.assertEqual(docs_policy.findings(cleaned), [])
            self.assertEqual(docs_policy.sanitize(cleaned), cleaned)

    def test_technical_code_and_ai_control_features_are_preserved(self):
        text = ('The editor AI control protocol supports `format::R8` and `D3D12`.\n\n'
                '```cpp\nconst char *value = "two  spaces";\nif (true) {\n    run();\n}\n```')
        self.assertEqual(docs_policy.sanitize(text), text)
        self.assertEqual(docs_policy.findings(text), [])

    def test_citations_in_code_spans_and_links_are_rejected(self):
        for text in ("`docs/design/12_audio.md`", "(R7)", "(wave-4)",
                     "[Notes](../design/12_audio.md)", "`AGENTS.md`", "`CLAUDE.md`",
                     "`.claude/skills/write-docs/SKILL.md`", "(08_testing)"):
            self.assertTrue(docs_policy.findings(text), text)
            self.assertFalse(docs_policy.findings(docs_policy.sanitize(text)), text)


if __name__ == "__main__":
    unittest.main()
