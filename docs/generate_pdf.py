#!/usr/bin/env python3
"""Convert FROM_SCRATCH.md to PDF — fixed parser."""

import re, os
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from reportlab.lib.colors import black, HexColor
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, HRFlowable, PageBreak
from reportlab.platypus.flowables import HRFlowable

SF = 'Helvetica'
SFB = 'Helvetica-Bold'
CODE = 'Courier'

def S(name, font, size, leading, align=TA_JUSTIFY, sb=0, sa=0, color=black):
    return ParagraphStyle(name, fontName=font, fontSize=size, leading=leading,
                          alignment=align, spaceBefore=sb, spaceAfter=sa, textColor=color)

# --- Styles ---
st_ct  = S('ct', SFB, 22, 28, TA_CENTER, 60, 6)
st_cs  = S('cs', SF, 12, 16, TA_CENTER, 0, 4, HexColor('#444'))
st_meta = S('m', SF, 9, 13, TA_CENTER, 0, 2, HexColor('#666'))
st_toc_h = S('th', SFB, 14, 18, TA_CENTER, 30, 16)
st_toc_i = S('ti', SF, 9, 13, TA_LEFT, 0, 3, HexColor('#222'))
st_h2   = S('h2', SFB, 12, 16, TA_LEFT, 18, 6)
st_h3   = S('h3', SFB, 10, 14, TA_LEFT, 12, 3)
st_body = S('bd', SF, 9, 13, TA_JUSTIFY, 0, 4)
st_code = S('cd', CODE, 7, 10, TA_LEFT, 2, 2)
st_bul  = S('bl', SF, 9, 13, TA_LEFT, 0, 2)
st_desc = S('dc', SF, 9.5, 13, TA_CENTER, 6, 4, HexColor('#444'))

def fmt(t):
    """Inline formatting: **bold**, `code`."""
    t = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', t)
    t = re.sub(r'`([^`]+)`', r'<font face="Courier" size="7">\1</font>', t)
    return t

def esc(t):
    """Escape XML special chars for code blocks."""
    return t.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')

# Read
md = open('/home/hari/chromium-android/patches/docs/FROM_SCRATCH.md').read()
lines = md.split('\n')

story = []

# --- Cover ---
story.append(Spacer(1, 90))
story.append(Paragraph("From Zero to Custom Chromium APK", st_ct))
story.append(Spacer(1, 6))
story.append(Paragraph("A Complete Guide to Building Your Own Browser", st_cs))
story.append(Spacer(1, 18))
story.append(HRFlowable(width="60%", thickness=1, color=black))
story.append(Spacer(1, 12))
story.append(Paragraph("Chromium 149.0.7827.84 stable &mdash; June 2026", st_meta))
story.append(Paragraph("Lessons from 6 days of builds, v1 through v25", st_meta))
story.append(Spacer(1, 30))
story.append(Paragraph(
    "This guide assumes no prior Chromium experience. It explains every tool, every file, "
    "every error, and every decision made while building a custom Chromium for Android with "
    "content blocking, NTP stripping, and sign-in removal.", st_desc))
story.append(PageBreak())

# --- State machine ---
in_code = False
code_buf = []
phase = 'cover'  # cover → toc → body

for raw in lines:
    s = raw.strip()
    
    # Phase tracking
    if '## Table of Contents' in s:
        phase = 'toc'
        story.append(Spacer(1, 40))
        story.append(Paragraph("Contents", st_toc_h))
        story.append(Spacer(1, 4))
        story.append(HRFlowable(width="100%", thickness=0.5, color=black))
        story.append(Spacer(1, 10))
        continue
    
    # Cover phase: skip everything until we hit TOC
    if phase == 'cover':
        continue
    
    if phase == 'toc' and s.startswith('## ') and 'Table of Contents' not in s:
        phase = 'body'
        story.append(PageBreak())
        # Fall through to process this heading
    
    # --- Code blocks ---
    if s.startswith('```'):
        if in_code:
            in_code = False
            ct = '\n'.join(code_buf)
            story.append(Paragraph(esc(ct), st_code))
            story.append(Spacer(1, 3))
            code_buf = []
        else:
            in_code = True
            code_buf = []
        continue
    if in_code:
        code_buf.append(s)
        continue
    
    # --- Empty line ---
    if not s:
        continue
    
    # --- Skip cover & TOC content ---
    if phase == 'cover' and not s.startswith('#'):
        continue  # still in cover front matter
    
    # --- Process by phase ---
    if phase == 'toc':
        if s.startswith(('1.', '2.', '3.', '4.', '5.', '6.', '7.', '8.', '9.')):
            # TOC entry: "1. [Title](#link)" → show "1. Title"
            plain = re.sub(r'\[(.+?)\]\(.*?\)', r'\1', s)
            story.append(Paragraph(plain, st_toc_i))
        continue
    
    # --- Body phase ---
    
    # Skip remaining TOC items if we somehow get here
    if s.startswith(('10.', '11.', '12.', '13.', '14.', '15.')):
        continue
    
    # Cut rule line
    if s.startswith('---'):
        continue
    
    # Heading 2
    if s.startswith('## '):
        t = s[3:]
        if t in ('Table of Contents', '1. What Are We Building?'):
            continue
        story.append(Spacer(1, 6))
        story.append(Paragraph(fmt(t), st_h2))
        story.append(HRFlowable(width="100%", thickness=0.3, color=HexColor('#bbb')))
        story.append(Spacer(1, 2))
        continue
    
    # Heading 3
    if s.startswith('### '):
        story.append(Paragraph(fmt(s[4:]), st_h3))
        continue
    
    # Bullet
    if s.startswith(('- ', '* ')):
        text = fmt(s[2:])
        story.append(Paragraph(f"&bull; {text}", st_bul))
        continue
    
    # Skip table of contents in the guide body (numbered sections)
    if re.match(r'^\d+\.\s+\[', s):
        continue
    # Skip markdown link references
    if s.startswith('[') and s.endswith(')'):
        continue
    
    # Regular paragraph
    story.append(Paragraph(fmt(s), st_body))

def pgnum(c, doc):
    c.saveState()
    c.setFont('Helvetica', 7.5)
    c.setFillColor(HexColor('#888'))
    c.drawCentredString(letter[0]/2, 0.45*inch, f"&mdash; {doc.page} &mdash;")
    c.restoreState()

out = '/home/hari/chromium-android/From_Zero_to_Chromium_APK.pdf'
doc = SimpleDocTemplate(out, pagesize=letter,
    leftMargin=0.85*inch, rightMargin=0.85*inch,
    topMargin=0.75*inch, bottomMargin=0.7*inch)

doc.build(story, onFirstPage=pgnum, onLaterPages=pgnum)
print(f"✓ {os.path.getsize(out)/1024:.0f} KB, {doc.page} pages")
