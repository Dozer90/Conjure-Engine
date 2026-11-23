Editable documentation assets
============================

Edit these source files. The build copies them into site/assets/ without replacing
your changes. Files under site/ are build output and are replaced on the next build.

Stylesheets
-----------
stylesheets/conjure-docs.css       Conjure layout, tables, images, and code links.
stylesheets/material-theme.css     The full Material theme stylesheet.
stylesheets/material-palette.css   Light/dark palettes and colour schemes.

JavaScript
----------
javascripts/material-theme.js             Navigation, theme controls, and UI behaviour.
javascripts/workers/search-worker.js      Search indexing and query worker.

The imported theme files use readable names and four-space formatting. Existing
copyright/license comments are retained. Minified source maps are not distributed, because their
line/column mappings would no longer match the formatted files. material-version.txt names the
Material version they come from; the build stops when another version is installed.

Search is English only. Material's lunr language and segmenter files are not vendored, and the
build stops when the search plugin asks for another language.

Third-party licences
--------------------
Every vendored file is registered in third_party/CMakeLists.txt and listed in
THIRD_PARTY_NOTICES.md. The licence texts are published with the site:

licenses/mkdocs-material.txt         Material for MkDocs (MIT): the four theme files above and
                                     tools/docs_theme/partials/nav.html and nav-item.html.
licenses/lunr.txt                    lunr.js (MIT), in workers/search-worker.js.
licenses/apache-2.0.txt              RxJS (Apache-2.0), in material-theme.js, and the
                                     Material Design Icons below.
licenses/material-design-color.txt  material-design-color (MIT): its colour variables in
                                     material-theme.css.
licenses/clipboard-js.txt            clipboard.js (MIT), in material-theme.js.
licenses/escape-html.txt             escape-html (MIT), in material-theme.js.
licenses/tslib.txt                   tslib (0BSD), in material-theme.js.
licenses/material-design-icons.txt   Material Design Icons by Pictogrammers (Apache-2.0):
                                     icons/callout-*.svg.
licenses/font-awesome-free.txt       Font Awesome Free (CC BY 4.0): one icon embedded in
                                     material-theme.css.

lunr.txt, clipboard-js.txt and escape-html.txt are the MIT terms under the copyright lines the
bundled files carry.

Material's bundle also carries the focus-visible polyfill, under the W3C Software and Document
License, whose full text would have to ship with it. The import leaves it out of
material-theme.js and rewrites the .focus-visible selectors of material-theme.css as the
:focus-visible pseudo-class, which every supported browser matches. When you import or merge a
newer Material by hand, keep it out the same way.

Commands, run from the repository root:
  python tools/docs.py assets           Create missing source assets only.
  python tools/docs.py assets --format  Reformat source CSS/JS while keeping edits.
  python tools/docs.py build            Rebuild site/ using the edited source files.
  python tools/docs.py serve            Preview and reload source changes locally.

Asset preparation never overwrites an existing file. It imports a missing theme
asset from the pinned Material package, only the four files listed above. The CSS/JS
format command changes formatting only; it does not restore vendor content over your
customisations.
