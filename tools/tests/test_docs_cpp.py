#!/usr/bin/env python3
"""Declaration inventory must retain members and qualifiers without leaking function bodies."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import docs_cpp


class DeclarationTests(unittest.TestCase):
    def test_complete_enum_keeps_written_values_order_and_conditional_branches(self):
        rows = docs_cpp.inventory('''namespace conjure::sample {
    enum class mode : unsigned;
    enum class mode : unsigned {
        Z = 0x10,
#if OPTIONAL
        A = Z << 1,
#else
        A = 20,
#endif
        COUNT
    };
    void run() { int hidden = 4; }
}''')
        definitions = [row for row in rows if row.name == "mode"]
        self.assertFalse(definitions[0].definition)
        self.assertIn("#if OPTIONAL\n    A = Z << 1,", definitions[1].definition)
        self.assertIn("#else\n    A = 20,\n#endif", definitions[1].definition)
        self.assertLess(definitions[1].definition.index("Z ="), definitions[1].definition.index("A ="))
        self.assertIn("COUNT\n}", definitions[1].definition)
        self.assertFalse(next(row for row in rows if row.name == "run").definition)

    def test_access_constructors_qualifiers_and_inline_bodies(self):
        source = '''namespace sample {
        class widget {
            int mPrivate = 0;
        protected:
            static constexpr int LIMIT = 8;
        public:
            widget() noexcept : mPrivate{2} {}
            explicit widget(int n) : mPrivate(n) { int local = n; }
            ~widget() = default;
            [[nodiscard]] int value() const noexcept { return mPrivate; }
            static void reset(widget &value);
            int &operator[](int index) { return mPrivate; }
            explicit operator bool() const { return true; }
        }; }'''
        rows = docs_cpp.inventory(source)
        names = {row.name for row in rows}
        self.assertIn('widget::mPrivate', names)
        self.assertNotIn('widget::local', names)
        self.assertEqual([r.access for r in rows if r.name == 'widget::mPrivate'], ['private'])
        limit = next(r for r in rows if r.name == 'widget::LIMIT')
        self.assertEqual(limit.access, 'protected')
        self.assertIn('static constexpr', limit.signature)
        constructors = [r for r in rows if r.kind == 'constructors']
        self.assertEqual(len(constructors), 3)
        self.assertEqual(constructors[0].signature, 'widget() noexcept')
        self.assertEqual(constructors[1].signature, 'explicit widget(int n)')
        self.assertTrue(all('mPrivate' not in r.signature for r in constructors))
        value = next(r for r in rows if r.name == 'widget::value')
        self.assertEqual(value.signature, '[[nodiscard]] int value() const noexcept')
        self.assertIn('widget::operator[]', names)
        self.assertIn('widget::operator bool', names)

    def test_nested_types_templates_specialisations_and_enumerators(self):
        source = '''namespace sample::detail {
        template <typename T> class box {
        public:
            struct entry { T value = {}; };
            using value_type = T;
            enum class mode : unsigned { FIRST = 1, SECOND = 2 };
        };
        template <> class box<void> { public: static void run(); };
        }'''
        rows = docs_cpp.inventory(source)
        names = {r.name for r in rows}
        self.assertTrue({'box', 'box::entry', 'box::entry::value', 'box::value_type',
                         'box::mode', 'box::mode::FIRST', 'box::mode::SECOND',
                         'box<void>', 'box<void>::run'} <= names)
        self.assertTrue(next(r for r in rows if r.name == 'box').signature.startswith('template <typename T>'))

    def test_summaries_comments_literals_and_preprocessor_branches(self):
        source = '''#pragma once
        namespace sample {
        /// <summary>A stable callback.</summary>
        using callback = void (*)(int value) noexcept;
        // A string containing braces is not a type body.
        inline const char *TEXT = R"tag({ "quoted"; })tag";
        #if ENABLED
        void enabled();
        #else
        void disabled();
        #endif
        }
        '''
        rows = docs_cpp.inventory(source)
        callback = next(r for r in rows if r.name == 'callback')
        self.assertEqual(callback.summary, 'A stable callback.')
        self.assertEqual(callback.kind, 'types')
        self.assertTrue({'TEXT', 'enabled', 'disabled'} <= {r.name for r in rows})
        self.assertEqual(callback.line, 4)

    def test_overloads_and_function_pointer_fields(self):
        rows = docs_cpp.inventory('struct api { void (*callback)(int n); int f() &; int f() const &; };')
        self.assertEqual([r.kind for r in rows if r.name == 'api::callback'], ['variables'])
        self.assertEqual([r.signature for r in rows if r.name == 'api::f'], ['int f() &', 'int f() const &'])

    def test_macro_invocations_do_not_consume_the_following_type(self):
        source = 'CONJURE_DECLARE_ERROR_CATEGORY(sample, sample_error)\nstruct value { int member; };'
        rows = docs_cpp.inventory(source)
        self.assertEqual([r.name for r in rows], ['CONJURE_DECLARE_ERROR_CATEGORY', 'value', 'value::member'])

    def test_names_in_function_bodies_are_never_declarations(self):
        source = '''inline void run() {
            struct local { int hidden; };
            auto callback = []() { return "}"; };
        }
        int next_value();'''
        self.assertEqual([r.name for r in docs_cpp.inventory(source)], ['run', 'next_value'])

    def test_qualified_specialisations_and_requires_clauses(self):
        source = '''template <> struct std::formatter<item> { int format() const; };
        template <typename T> requires allowed<T> struct traits<T> { static constexpr int SIZE = 4; };
        template <typename T> concept allowed = requires { { T::value } -> convertible<int>; };
        '''
        rows = docs_cpp.inventory(source)
        by_name = {r.name: r for r in rows}
        self.assertIn('std::formatter<item>', by_name)
        self.assertIn('std::formatter<item>::format', by_name)
        self.assertIn('traits<T>::SIZE', by_name)
        self.assertEqual(by_name['allowed'].kind, 'types')
        self.assertNotIn('{', by_name['traits<T>'].signature)

    def test_parenthesised_requires_constraints_do_not_swallow_the_next_member(self):
        source = '''class input {
        public:
            template <typename EventType>
                requires std::constructible_from<input_event, const EventType &> &&
                    (!std::same_as<EventType, input_event>) && (!std::same_as<EventType, event>)
            void inject(const EventType &value) { consume(value); }
            /// <summary>Publishes the input snapshot.</summary>
            void end_frame();
        };'''
        rows = docs_cpp.inventory(source)
        functions = {row.name: row for row in rows if row.kind == 'functions'}
        self.assertEqual(set(functions), {'input::inject', 'input::end_frame'})
        self.assertIn('requires std::constructible_from', functions['input::inject'].signature)
        self.assertNotIn('{', functions['input::inject'].signature)
        self.assertEqual(functions['input::end_frame'].signature, 'void end_frame()')
        self.assertEqual(functions['input::end_frame'].summary, 'Publishes the input snapshot.')
        self.assertFalse([row for row in rows if row.kind == 'variables'])

    def test_preprocessor_continuations_and_literal_spacing_are_preserved(self):
        source = ('#define DECLARE(name) ' + chr(92) + '\n'
                  'struct name { int member; };\n'
                  'inline const char *TEXT = "http://example/two  spaces";\n')
        rows = docs_cpp.inventory(source)
        self.assertEqual([r.name for r in rows], ['TEXT'])
        self.assertIn('"http://example/two  spaces"', rows[0].signature)

    def test_c_abi_typedefs_are_types(self):
        source = 'extern "C" { typedef const info *(*describe_fn)(void); typedef unsigned long word; }'
        rows = docs_cpp.inventory(source)
        self.assertEqual([(r.kind, r.name) for r in rows], [('types', 'describe_fn'), ('types', 'word')])


if __name__ == '__main__':
    unittest.main()
