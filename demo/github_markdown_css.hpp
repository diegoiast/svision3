// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Pre-baked GitHub Markdown CSS adapted for litehtml.
//
// Origin:  https://github.com/sindresorhus/github-markdown-css
// License: MIT — Copyright (c) Sindre Sorhus <sindresorhus@gmail.com>
//
// litehtml does not support CSS custom properties (var()), so all CSS variables
// from the original stylesheet have been substituted with their resolved values
// for the respective light and dark themes.  Properties that litehtml ignores
// silently (box-sizing, overflow, word-wrap) have been removed to keep the
// strings compact.
//
// Usage (demo code only — not part of the svision3 library):
//   view->set_markdown_css(GITHUB_MARKDOWN_CSS_LIGHT, GITHUB_MARKDOWN_CSS_DARK);
//   view->set_markdown(text);  // picks the right variant automatically

#pragma once

// ── Light theme ─────────────────────────────────────────────────────────────
// Variable mapping (github-markdown-light.css):
//   --color-canvas-default         → #ffffff
//   --color-fg-default             → #1f2328
//   --color-fg-muted               → #656d76
//   --color-border-default         → #d0d7de
//   --color-border-muted           → #d8dee4
//   --color-neutral-muted          → rgba(175,184,193,0.2)
//   --color-accent-fg              → #0969da
//   pre background                 → #f6f8fa

static constexpr auto GITHUB_MARKDOWN_CSS_LIGHT = R"css(
.markdown-body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    font-size: 16px;
    line-height: 1.5;
    color: #1f2328;
    background-color: #ffffff;
    padding: 16px 32px;
}

.markdown-body h1,
.markdown-body h2,
.markdown-body h3,
.markdown-body h4,
.markdown-body h5,
.markdown-body h6 {
    margin-top: 24px;
    margin-bottom: 16px;
    font-weight: 600;
    line-height: 1.25;
    color: #1f2328;
}

.markdown-body h1 {
    font-size: 2em;
    padding-bottom: 0.3em;
    border-bottom: 1px solid #d8dee4;
}

.markdown-body h2 {
    font-size: 1.5em;
    padding-bottom: 0.3em;
    border-bottom: 1px solid #d8dee4;
}

.markdown-body h3 { font-size: 1.25em; }
.markdown-body h4 { font-size: 1em; }
.markdown-body h5 { font-size: 0.875em; }
.markdown-body h6 { font-size: 0.85em; color: #656d76; }

.markdown-body p { margin-top: 0; margin-bottom: 16px; }

.markdown-body a { color: #0969da; text-decoration: none; }

.markdown-body ul,
.markdown-body ol {
    margin-top: 0;
    margin-bottom: 16px;
    padding-left: 2em;
}

.markdown-body li { margin-top: 0.25em; }

.markdown-body code {
    font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
    font-size: 85%;
    padding: 0.2em 0.4em;
    background-color: rgba(175, 184, 193, 0.2);
    border-radius: 6px;
}

.markdown-body pre {
    padding: 16px;
    font-size: 85%;
    line-height: 1.45;
    background-color: #f6f8fa;
    border-radius: 6px;
    margin-top: 0;
    margin-bottom: 16px;
}

.markdown-body pre code {
    font-size: 100%;
    padding: 0;
    background-color: transparent;
    border-radius: 0;
}

.markdown-body blockquote {
    margin: 0 0 16px;
    padding: 0 1em;
    color: #656d76;
    border-left: 4px solid #d0d7de;
}

.markdown-body hr {
    height: 2px;
    padding: 0;
    margin: 24px 0;
    background-color: #d8dee4;
    border: 0;
}

.markdown-body table { border-collapse: collapse; margin-bottom: 16px; }

.markdown-body table th {
    font-weight: 600;
    padding: 6px 13px;
    border: 1px solid #d0d7de;
    background-color: #f6f8fa;
}

.markdown-body table td {
    padding: 6px 13px;
    border: 1px solid #d0d7de;
}

.markdown-body table tr {
    background-color: #ffffff;
    border-top: 1px solid #d8dee4;
}

.markdown-body img { max-width: 100%; }
.markdown-body strong { font-weight: 600; }
.markdown-body em { font-style: italic; }
.markdown-body del { text-decoration: line-through; color: #656d76; }
)css";

// ── Dark theme ──────────────────────────────────────────────────────────────
// Variable mapping (github-markdown-dark.css):
//   --color-canvas-default         → #0d1117
//   --color-fg-default             → #e6edf3
//   --color-fg-muted               → #7d8590
//   --color-fg-subtle              → #9198a1
//   --color-border-default         → #3d444d
//   --color-border-muted           → #21262d
//   --color-neutral-muted          → rgba(110,118,129,0.4)
//   --color-accent-fg              → #2f81f7
//   pre background                 → #151b23

static constexpr auto GITHUB_MARKDOWN_CSS_DARK = R"css(
.markdown-body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    font-size: 16px;
    line-height: 1.5;
    color: #e6edf3;
    background-color: #0d1117;
    padding: 16px 32px;
}

.markdown-body h1,
.markdown-body h2,
.markdown-body h3,
.markdown-body h4,
.markdown-body h5,
.markdown-body h6 {
    margin-top: 24px;
    margin-bottom: 16px;
    font-weight: 600;
    line-height: 1.25;
    color: #e6edf3;
}

.markdown-body h1 {
    font-size: 2em;
    padding-bottom: 0.3em;
    border-bottom: 1px solid #3d444d;
}

.markdown-body h2 {
    font-size: 1.5em;
    padding-bottom: 0.3em;
    border-bottom: 1px solid #3d444d;
}

.markdown-body h3 { font-size: 1.25em; }
.markdown-body h4 { font-size: 1em; }
.markdown-body h5 { font-size: 0.875em; }
.markdown-body h6 { font-size: 0.85em; color: #9198a1; }

.markdown-body p { margin-top: 0; margin-bottom: 16px; }

.markdown-body a { color: #2f81f7; text-decoration: none; }

.markdown-body ul,
.markdown-body ol {
    margin-top: 0;
    margin-bottom: 16px;
    padding-left: 2em;
}

.markdown-body li { margin-top: 0.25em; }

.markdown-body code {
    font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
    font-size: 85%;
    padding: 0.2em 0.4em;
    background-color: rgba(110, 118, 129, 0.4);
    border-radius: 6px;
}

.markdown-body pre {
    padding: 16px;
    font-size: 85%;
    line-height: 1.45;
    background-color: #151b23;
    border-radius: 6px;
    margin-top: 0;
    margin-bottom: 16px;
}

.markdown-body pre code {
    font-size: 100%;
    padding: 0;
    background-color: transparent;
    border-radius: 0;
}

.markdown-body blockquote {
    margin: 0 0 16px;
    padding: 0 1em;
    color: #9198a1;
    border-left: 4px solid #3d444d;
}

.markdown-body hr {
    height: 2px;
    padding: 0;
    margin: 24px 0;
    background-color: #3d444d;
    border: 0;
}

.markdown-body table { border-collapse: collapse; margin-bottom: 16px; }

.markdown-body table th {
    font-weight: 600;
    padding: 6px 13px;
    border: 1px solid #3d444d;
    background-color: #151b23;
}

.markdown-body table td {
    padding: 6px 13px;
    border: 1px solid #3d444d;
}

.markdown-body table tr {
    background-color: #0d1117;
    border-top: 1px solid #21262d;
}

.markdown-body img { max-width: 100%; }
.markdown-body strong { font-weight: 600; }
.markdown-body em { font-style: italic; }
.markdown-body del { text-decoration: line-through; color: #9198a1; }
)css";
