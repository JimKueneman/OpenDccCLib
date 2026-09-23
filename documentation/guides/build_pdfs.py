#!/usr/bin/env python3
"""
Build the OpenDccCLib PDF guides from the Markdown sources in this folder.

    python3 documentation/guides/build_pdfs.py            # all five
    python3 documentation/guides/build_pdfs.py DeveloperGuide_CommandStation

Each <name>.md here renders to ../<name>.pdf (documentation/<name>.pdf) with the
house style from PDF_Regeneration_Guide.md section 4: Helvetica/Courier, navy
titles, steel-blue headings and table header rows, blue note boxes, gray code
blocks, a title page, a table of contents, one section per page break, and a
per-page footer. Requires only ReportLab (pip install reportlab).

Markdown subset understood (deliberately small, so this file stays small):

    ---                         front matter: title, subtitle, tagline, footer,
    title: ...                  footer2 (brochure), section_breaks (yes/no),
    ---                         toc (yes/no)
    ## 1. Section               H1  (page break before it when section_breaks)
    ### 1.1 Subsection          H2
    #### Heading                H3
    paragraph text              inline **bold**, *italic*, `code`
    - bullet / 1. numbered
    > note text                 blue italic callout (may span lines)
    ```                         fenced code block
    | a | b |  + |---|---|      table, first row is the header
    <<pagebreak>>               explicit page break
"""

import os
import re
import sys
import datetime

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (BaseDocTemplate, Frame, PageTemplate, Paragraph, Spacer,
                                Table, TableStyle, PageBreak, KeepTogether, XPreformatted)
from reportlab.platypus.tableofcontents import TableOfContents

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.dirname(HERE)                       # documentation/

NAVY, STEEL, NOTE_BG = colors.HexColor("#2C3E5A"), colors.HexColor("#3B6FA0"), colors.HexColor("#E8F0FE")
ALT_ROW, CODE_BG, BODY, FOOT = colors.HexColor("#F5F7FA"), colors.HexColor("#F5F5F5"), colors.HexColor("#333333"), colors.HexColor("#999999")

S = {
    "title":    ParagraphStyle("title", fontName="Helvetica-Bold", fontSize=28, leading=34, textColor=NAVY, spaceAfter=10),
    "subtitle": ParagraphStyle("subtitle", fontName="Helvetica", fontSize=14, leading=18, textColor=STEEL, spaceAfter=6),
    "tagline":  ParagraphStyle("tagline", fontName="Helvetica-Oblique", fontSize=10, leading=14, textColor=BODY, spaceAfter=4),
    "h1":       ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=20, leading=24, textColor=NAVY, spaceBefore=6, spaceAfter=10),
    "h2":       ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=14, leading=18, textColor=STEEL, spaceBefore=12, spaceAfter=6),
    "h3":       ParagraphStyle("h3", fontName="Helvetica-Bold", fontSize=11, leading=14, textColor=STEEL, spaceBefore=8, spaceAfter=4),
    "body":     ParagraphStyle("body", fontName="Helvetica", fontSize=10, leading=14, textColor=BODY, spaceAfter=6),
    "bullet":   ParagraphStyle("bullet", fontName="Helvetica", fontSize=10, leading=14, textColor=BODY, leftIndent=16, bulletIndent=4, spaceAfter=2),
    "cell":     ParagraphStyle("cell", fontName="Helvetica", fontSize=9, leading=11.5, textColor=BODY),
    "cellhead": ParagraphStyle("cellhead", fontName="Helvetica-Bold", fontSize=9, leading=11.5, textColor=colors.white),
    "code":     ParagraphStyle("code", fontName="Courier", fontSize=8, leading=10, textColor=BODY, backColor=CODE_BG,
                               borderPadding=(6, 8, 6, 8), leftIndent=6, rightIndent=6, spaceBefore=4, spaceAfter=10),
    "note":     ParagraphStyle("note", fontName="Helvetica-Oblique", fontSize=9, leading=12.5, textColor=BODY, backColor=NOTE_BG,
                               borderPadding=(6, 8, 6, 8), leftIndent=6, rightIndent=6, spaceBefore=4, spaceAfter=10),
    "h1toc":    ParagraphStyle("h1toc", fontName="Helvetica-Bold", fontSize=20, leading=24, textColor=NAVY, spaceBefore=6, spaceAfter=10),
    "toc0":     ParagraphStyle("toc0", fontName="Helvetica-Bold", fontSize=10, leading=15, textColor=NAVY),
    "toc1":     ParagraphStyle("toc1", fontName="Helvetica", fontSize=9, leading=13, textColor=BODY, leftIndent=18),
    "footer":   ParagraphStyle("footer", fontName="Helvetica", fontSize=8, textColor=FOOT),
    "footerc":  ParagraphStyle("footerc", fontName="Helvetica", fontSize=8, textColor=FOOT, alignment=TA_CENTER, leading=10),
}


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def inline(t):
    """Markdown inline -> ReportLab paragraph markup."""
    t = esc(t)
    t = re.sub(r"`([^`]+)`", r'<font face="Courier" size="8.5">\1</font>', t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"(?<![\w*])\*([^*]+)\*(?![\w*])", r"<i>\1</i>", t)
    return t


class GuideDoc(BaseDocTemplate):
    """Two-pass build so the table of contents has page numbers."""

    def __init__(self, path, meta):
        self.meta = meta
        BaseDocTemplate.__init__(self, path, pagesize=letter,
                                 leftMargin=1 * inch, rightMargin=1 * inch,
                                 topMargin=0.8 * inch, bottomMargin=0.8 * inch,
                                 title=meta.get("title", ""), author="Jim Kueneman")
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="f")
        self.addPageTemplates([PageTemplate(id="p", frames=[frame], onPage=self._footer)])

    def _footer(self, canvas, doc):
        canvas.saveState()
        y = 0.45 * inch
        if self.meta.get("footer2"):
            p = Paragraph(esc(self.meta["footer"]) + "<br/>" + esc(self.meta["footer2"]), S["footerc"])
            w, h = p.wrap(self.width, 40)
            p.drawOn(canvas, self.leftMargin, y - 4)
        else:
            canvas.setFont("Helvetica", 8)
            canvas.setFillColor(FOOT)
            canvas.drawString(self.leftMargin, y, "OpenDccCLib | " + self.meta.get("title", ""))
            canvas.drawRightString(self.leftMargin + self.width, y, "Page %d" % doc.page)
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name in ("h1", "h2"):
            level = 0 if flowable.style.name == "h1" else 1
            text = re.sub(r"<[^>]+>", "", flowable.getPlainText())
            self.notify("TOCEntry", (level, text, self.page))


def make_table(rows, avail_width):
    ncol = max(len(r) for r in rows)
    rows = [r + [""] * (ncol - len(r)) for r in rows]
    # column widths proportional to the longest cell text, with a floor
    lens = [max(max(len(r[c]) for r in rows), 6) for c in range(ncol)]
    total = float(sum(lens))
    widths = [max(0.9 * inch, avail_width * L / total) for L in lens]
    scale = avail_width / sum(widths)
    widths = [w * scale for w in widths]
    data = [[Paragraph(inline(c), S["cellhead"]) for c in rows[0]]]
    data += [[Paragraph(inline(c), S["cell"]) for c in r] for r in rows[1:]]
    t = Table(data, colWidths=widths, repeatRows=1)
    style = [("BACKGROUND", (0, 0), (-1, 0), STEEL), ("VALIGN", (0, 0), (-1, -1), "TOP"),
             ("LINEBELOW", (0, 0), (-1, -1), 0.25, colors.HexColor("#D0D7E2")),
             ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    for i in range(2, len(data), 2):
        style.append(("BACKGROUND", (0, i), (-1, i), ALT_ROW))
    t.setStyle(TableStyle(style))
    return t


def parse(md_text):
    """Front matter + body lines -> (meta, flowables)."""
    meta, lines = {}, md_text.splitlines()
    if lines and lines[0].strip() == "---":
        end = lines.index("---", 1)
        for l in lines[1:end]:
            if ":" in l:
                k, v = l.split(":", 1)
                meta[k.strip()] = v.strip()
        lines = lines[end + 1:]
    return meta, lines


def build_story(meta, lines, avail_width):
    story = []
    breaks = meta.get("section_breaks", "yes").lower() != "no"
    # title page
    story += [Spacer(1, 2.2 * inch), Paragraph(esc(meta.get("title", "")), S["title"])]
    if meta.get("subtitle"):
        story.append(Paragraph(esc(meta["subtitle"]), S["subtitle"]))
    if meta.get("tagline"):
        story.append(Paragraph(esc(meta["tagline"]), S["tagline"]))
    story.append(Spacer(1, 0.3 * inch))
    story.append(Paragraph(esc("Generated %s from the repository sources by documentation/guides/build_pdfs.py"
                               % datetime.date.today().isoformat()), S["tagline"]))
    if meta.get("toc", "yes").lower() != "no":
        story.append(PageBreak())
        story.append(Paragraph("Table of Contents", S["h1toc"]))
        toc = TableOfContents()
        toc.levelStyles = [S["toc0"], S["toc1"]]
        story.append(toc)
    first_h1 = True
    para, i = [], 0

    def flush():
        if para:
            story.append(Paragraph(inline(" ".join(para)), S["body"]))
            para.clear()

    while i < len(lines):
        l = lines[i]
        s = l.strip()
        if not s:
            flush(); i += 1; continue
        if s == "<<pagebreak>>":
            flush(); story.append(PageBreak()); i += 1; continue
        if s.startswith("## "):
            flush()
            if breaks and not first_h1:
                story.append(PageBreak())
            elif first_h1 and meta.get("toc", "yes").lower() != "no":
                story.append(PageBreak())
            first_h1 = False
            story.append(Paragraph(inline(s[3:]), S["h1"])); i += 1; continue
        if s.startswith("### "):
            flush(); story.append(Paragraph(inline(s[4:]), S["h2"])); i += 1; continue
        if s.startswith("#### "):
            flush(); story.append(Paragraph(inline(s[5:]), S["h3"])); i += 1; continue
        if s.startswith("```"):
            flush(); i += 1; buf = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                buf.append(lines[i]); i += 1
            i += 1
            story.append(XPreformatted(esc("\n".join(buf)), S["code"])); continue
        if s.startswith(">"):
            flush(); buf = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip()); i += 1
            story.append(Paragraph(inline(" ".join(buf)), S["note"])); continue
        if s.startswith("|"):
            flush(); rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
                    rows.append(cells)
                i += 1
            story.append(make_table(rows, avail_width)); story.append(Spacer(1, 8)); continue
        m = re.match(r"^(-|\d+\.)\s+(.*)", s)
        if m:
            flush()
            while i < len(lines):
                m = re.match(r"^(-|\d+\.)\s+(.*)", lines[i].strip())
                if not m:
                    break
                bullet = "•" if m.group(1) == "-" else m.group(1)
                story.append(Paragraph(inline(m.group(2)), S["bullet"], bulletText=bullet)); i += 1
            story.append(Spacer(1, 4)); continue
        para.append(s); i += 1
    flush()
    return story


def build(name):
    src = os.path.join(HERE, name + ".md")
    out = os.path.join(OUT_DIR, name + ".pdf")
    meta, lines = parse(open(src, encoding="utf-8").read())
    doc = GuideDoc(out, meta)
    story = build_story(meta, lines, doc.width)
    doc.multiBuild(story)
    return out


if __name__ == "__main__":
    names = sys.argv[1:] or sorted(f[:-3] for f in os.listdir(HERE) if f.endswith(".md"))
    for n in names:
        print("wrote", build(n))
