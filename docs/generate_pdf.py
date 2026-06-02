#!/usr/bin/env python3
"""Convert FROM_SCRATCH.md to a beautiful colored PDF via HTML + WeasyPrint."""

import markdown
from weasyprint import HTML
import re, os, sys

md_path = os.path.expanduser('~/chromium-android/patches/docs/FROM_SCRATCH.md')
output_path = os.path.expanduser('~/chromium-android/From_Zero_to_Chromium_APK.pdf')

with open(md_path) as f:
    md_text = f.read()

# Strip markdown link syntax for cleaner output: [text](url) → text
md_text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', md_text)

# Convert markdown to HTML
html_body = markdown.markdown(
    md_text,
    extensions=['fenced_code', 'codehilite', 'tables', 'nl2br', 'sane_lists'],
    extension_configs={
        'codehilite': {
            'css_class': 'highlight',
            'linenums': False,
        }
    }
)

# Build full HTML with color-themed CSS
html_full = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
@page {{
  size: A4;
  margin: 2cm 2.2cm;
  @bottom-center {{
    content: counter(page);
    font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif;
    font-size: 9pt;
    color: #999;
  }}
}}

@page:first {{
  @bottom-center {{
    content: none;
  }}
}}

body {{
  font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif;
  font-size: 10.5pt;
  line-height: 1.65;
  color: #1a1a1a;
}}

/* ── COVER PAGE ───────────────────────────────────────────── */
h1:first-of-type {{
  text-align: center;
  font-size: 28pt;
  margin-top: 5cm;
  margin-bottom: 0.3cm;
  color: #1a1a2e;
  letter-spacing: -0.5pt;
}}

h1:first-of-type + p {{
  text-align: center;
  font-size: 14pt;
  color: #445;
  margin-bottom: 0.5cm;
}}

h1:first-of-type + p + hr {{
  width: 40%;
  margin: 1.5cm auto;
  border: none;
  border-top: 2px solid #e94560;
}}

h1:first-of-type + p + hr ~ p {{
  text-align: center;
  font-size: 10pt;
  color: #888;
  margin: 0.3cm 0;
}}

h1:first-of-type + p + hr ~ p:last-of-type {{
  font-size: 10.5pt;
  color: #555;
  max-width: 65%;
  margin: 2cm auto;
  line-height: 1.6;
}}

/* ── TABLE OF CONTENTS ────────────────────────────────────── */
h2:first-of-type {{
  text-align: center;
  font-size: 20pt;
  margin-top: 3cm;
  margin-bottom: 0.8cm;
  color: #1a1a2e;
  page-break-before: always;
}}

h2:first-of-type + hr {{
  border: none;
  border-top: 1px solid #ccc;
  margin-bottom: 0.8cm;
}}

/* ── CHAPTER HEADINGS ─────────────────────────────────────── */
h2 {{
  font-size: 17pt;
  color: #1a1a2e;
  margin-top: 1.5cm;
  margin-bottom: 0.4cm;
  page-break-before: always;
  border-bottom: 3px solid #e94560;
  padding-bottom: 6px;
}}

h3 {{
  font-size: 13pt;
  color: #16213e;
  margin-top: 0.8cm;
  margin-bottom: 0.3cm;
  border-bottom: 1px solid #ddd;
  padding-bottom: 3px;
}}

h4 {{
  font-size: 11.5pt;
  color: #0f3460;
  margin-top: 0.5cm;
  margin-bottom: 0.2cm;
}}

/* ── BODY TEXT ────────────────────────────────────────────── */
p {{
  margin: 0.3em 0;
  text-align: justify;
}}

strong {{
  color: #0f3460;
}}

/* ── CODE BLOCKS ──────────────────────────────────────────── */
pre {{
  background: #f7f7f9;
  border: 1px solid #e1e1e8;
  border-left: 4px solid #e94560;
  padding: 10px 14px;
  font-family: 'Consolas', 'Cascadia Code', 'Courier New', monospace;
  font-size: 8pt;
  line-height: 1.4;
  overflow-x: auto;
  white-space: pre-wrap;
  word-break: break-word;
  margin: 0.5em 0;
  border-radius: 3px;
}}

code {{
  font-family: 'Consolas', 'Cascadia Code', 'Courier New', monospace;
  font-size: 8.5pt;
  background: #f0f0f5;
  padding: 1px 5px;
  border-radius: 3px;
  color: #c7254e;
}}

pre code {{
  background: none;
  padding: 0;
  border-radius: 0;
  color: #333;
}}

/* Colors for code with syntax highlighting */
.highlight .c1, .highlight .cm, .highlight .cp, .highlight .c {{ color: #999988; font-style: italic; }}
.highlight .k, .highlight .kd, .highlight .kn {{ color: #d73a49; font-weight: bold; }}
.highlight .s, .highlight .s2, .highlight .s1 {{ color: #032f62; }}
.highlight .n, .highlight .nx {{ color: #333; }}
.highlight .nf {{ color: #6f42c1; }}
.highlight .nc {{ color: #e36209; }}
.highlight .p {{ color: #666; }}
.highlight .mi {{ color: #0550ae; }}
.highlight .nd {{ color: #6f42c1; }}
.highlight .o {{ color: #d73a49; }}

/* ── TABLES ───────────────────────────────────────────────── */
table {{
  border-collapse: collapse;
  width: 100%;
  margin: 0.6em 0;
  font-size: 9.5pt;
  page-break-inside: auto;
}}

tr {{
  page-break-inside: avoid;
  page-break-after: auto;
}}

thead {{
  display: table-header-group;
}}

th {{
  background: linear-gradient(180deg, #1a1a2e 0%, #16213e 100%);
  color: white;
  padding: 7px 10px;
  text-align: left;
  font-weight: 600;
  font-size: 9pt;
  letter-spacing: 0.3pt;
}}

td {{
  padding: 5px 10px;
  border-bottom: 1px solid #e0e0e0;
  vertical-align: top;
}}

tr:nth-child(even) td {{
  background: #f5f7fa;
}}

tr:nth-child(odd) td {{
  background: #fafbfc;
}}

/* Prevent wide tables from overflowing */
table {{
  table-layout: fixed;
}}

td, th {{
  word-wrap: break-word;
  overflow-wrap: break-word;
}}

/* ── LISTS ────────────────────────────────────────────────── */
ul, ol {{
  margin: 0.3em 0;
  padding-left: 1.5em;
}}

li {{
  margin: 0.12em 0;
}}

/* ── HORIZONTAL RULES ─────────────────────────────────────── */
hr {{
  border: none;
  border-top: 1px solid #ddd;
  margin: 0.6em 0;
}}

/* ── BLOCK QUOTES ─────────────────────────────────────────── */
blockquote {{
  border-left: 4px solid #e94560;
  margin: 0.5em 0;
  padding: 0.3em 1em;
  color: #555;
  background: #fafafa;
  font-style: italic;
}}

/* ── SPECIAL FORMATTING ───────────────────────────────────── */
/* Make diff +/- stand out */
del {{ color: #cb2431; background: #ffeef0; }}
ins {{ color: #22863a; background: #f0fff4; }}
</style>
</head>
<body>
{html_body}
</body>
</html>"""

# Write HTML temp
html_path = '/tmp/guide.html'
with open(html_path, 'w') as f:
    f.write(html_full)

# Generate PDF
HTML(filename=html_path).write_pdf(output_path)

pdf_size = os.path.getsize(output_path) / 1024

# Count pages
try:
    import fitz
    doc = fitz.open(output_path)
    page_count = doc.page_count
    doc.close()
except:
    page_count = "?"
    
print(f"✓ PDF: {output_path}")
print(f"  {pdf_size:.0f} KB, {page_count} pages")
