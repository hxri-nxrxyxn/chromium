#!/usr/bin/env python3
"""Convert FROM_SCRATCH.md to a beautiful PDF via HTML + WeasyPrint."""

import markdown
from weasyprint import HTML
import re, os

md_path = '/home/hari/chromium-android/patches/docs/FROM_SCRATCH.md'

with open(md_path) as f:
    md_text = f.read()

# Strip the top section numbers from TOC entries in the body
# Also strip markdown link syntax for cleaner output
# The markdown still keeps [text](url) — we want just "text" for TOC
md_text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', md_text)

# Convert markdown to HTML
html_body = markdown.markdown(
    md_text,
    extensions=['fenced_code', 'codehilite', 'tables', 'nl2br'],
    extension_configs={
        'codehilite': {
            'css_class': 'highlight',
            'linenums': False,
        }
    }
)

# Build full HTML with CSS
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
    font-family: 'Segoe UI', system-ui, sans-serif;
    font-size: 9pt;
    color: #888;
  }}
}}

body {{
  font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif;
  font-size: 11pt;
  line-height: 1.6;
  color: #1a1a1a;
  max-width: 100%;
}}

/* Cover page */
h1:first-of-type {{
  text-align: center;
  font-size: 26pt;
  margin-top: 6cm;
  margin-bottom: 0.3cm;
  color: #111;
}}

h1:first-of-type + p {{
  text-align: center;
  font-size: 13pt;
  color: #555;
  margin-bottom: 1.5cm;
}}

h1:first-of-type + p + hr {{
  width: 50%;
  margin: 1cm auto;
  border: none;
  border-top: 1.5px solid #333;
}}

h1:first-of-type + p + hr ~ p {{
  text-align: center;
  font-size: 10pt;
  color: #777;
  margin: 0.2cm 0;
}}

h1:first-of-type + p + hr ~ p:last-of-type {{
  font-size: 10.5pt;
  color: #555;
  max-width: 70%;
  margin: 1.5cm auto;
  line-height: 1.5;
}}

/* Table of Contents */
h2:first-of-type {{
  text-align: center;
  font-size: 18pt;
  margin-top: 3cm;
  margin-bottom: 0.8cm;
}}

h2:first-of-type + hr {{
  border: none;
  border-top: 1px solid #ccc;
  margin-bottom: 0.6cm;
}}

/* Chapter headings */
h2 {{
  font-size: 16pt;
  color: #111;
  margin-top: 1.2cm;
  margin-bottom: 0.3cm;
  page-break-before: always;
  border-bottom: 2px solid #222;
  padding-bottom: 4px;
}}

/* Sub-headings */
h3 {{
  font-size: 12pt;
  color: #333;
  margin-top: 0.6cm;
  margin-bottom: 0.2cm;
}}

/* Body */
p {{
  margin: 0.3em 0;
  text-align: justify;
}}

/* Code blocks */
pre {{
  background: #f5f5f5;
  border: 1px solid #ddd;
  border-left: 4px solid #222;
  padding: 10px 14px;
  font-family: 'Consolas', 'Courier New', monospace;
  font-size: 8.5pt;
  line-height: 1.4;
  overflow-x: auto;
  white-space: pre-wrap;
  word-break: break-word;
  margin: 0.5em 0;
}}

code {{
  font-family: 'Consolas', 'Courier New', monospace;
  font-size: 9pt;
  background: #f0f0f0;
  padding: 1px 4px;
  border-radius: 2px;
}}

pre code {{
  background: none;
  padding: 0;
  border-radius: 0;
}}

/* Tables */
table {{
  border-collapse: collapse;
  width: 100%;
  margin: 0.5em 0;
  font-size: 10pt;
}}

th {{
  background: #222;
  color: white;
  padding: 6px 10px;
  text-align: left;
  font-weight: 600;
}}

td {{
  padding: 5px 10px;
  border-bottom: 1px solid #ddd;
}}

tr:nth-child(even) td {{
  background: #f9f9f9;
}}

/* Lists */
ul, ol {{
  margin: 0.3em 0;
  padding-left: 1.5em;
}}

li {{
  margin: 0.15em 0;
}}

/* Horizontal rule */
hr {{
  border: none;
  border-top: 1px solid #ddd;
  margin: 0.6em 0;
}}

/* Block quotes */
blockquote {{
  border-left: 4px solid #ccc;
  margin: 0.5em 0;
  padding: 0.3em 1em;
  color: #555;
  background: #fafafa;
}}

/* First page - no page number */
@page:first {{
  @bottom-center {{
    content: none;
  }}
}}
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
output_path = '/home/hari/chromium-android/From_Zero_to_Chromium_APK.pdf'
HTML(filename=html_path).write_pdf(output_path)

pdf_size = os.path.getsize(output_path) / 1024

# Count pages
import fitz
doc = fitz.open(output_path)
print(f"✓ PDF: {output_path}")
print(f"  {pdf_size:.0f} KB, {doc.page_count} pages")
doc.close()
