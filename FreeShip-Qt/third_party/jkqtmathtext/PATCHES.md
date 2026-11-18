# Local patch

Pinned revision and original upstream hashes are in UPSTREAM.json. Copyright
and LGPL-2.1-or-later notices are retained.

`jkqtmathtext/jkqtmathtexttools.cpp`, `JKQTMathTextEnvironment::toHtmlStart`:
the upstream HTML exporter inverted the resolved italic flag while inside math
mode. Emit explicit `font-style` from that flag instead. This preserves LaTeX's
italic variable and upright `\mathrm` subscript when the parsed tree is rendered
as Qt rich text. Native LaTeX parsing and numerical code are unchanged. The
integrated regression checks actual QTextCharFormat superscript/subscript and
emphasis, not just the presence of an underscore in a string.
