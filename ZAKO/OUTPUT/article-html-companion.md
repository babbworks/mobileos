              # ZAKO Article — HTML Companion Guide

This document specifies the interactive HTML format for the ZAKO article, adapted from the Clark Field Report pattern (clarkinc/clark-report). It covers layout, annotation behavior, sidebar structure, and content mapping.

---

## Design Reference

The Clark Field Report establishes the pattern:
- Three-column grid: left rail (TOC/spine), center article, right rail (sources)
- Inline annotation markers (`n.01`, `n.02`) that toggle technical depth blocks below the paragraph
- Scrollspy on the left rail for section tracking
- Dialog overlay for working papers / extended documents
- Typography: display serif for headings, body serif for prose, monospace for metadata/labels

---

## Adaptations for ZAKO

### Color System

Replace Clark's "surveyor orange" with ZAKO's palette:

```css
:root {
  --paper: #f5f3ee;
  --ink: #1a1a18;
  --ink-soft: #3d3d38;
  --ink-faint: #7a7a70;
  --accent: #2a6b4f;          /* sovereign green — trust, sovereignty */
  --accent-deep: #1d4d38;
  --rule: #d4d0c6;
  --note-bg: #eae8e0;
}
```

Rationale: green signals sovereignty and trust without the urgency of orange. ZAKO is not a call to action — it's a presentation of an architecture.

### Typography

```css
:root {
  --serif: "Newsreader", Georgia, serif;
  --display: "Instrument Serif", Georgia, serif;
  --mono: "IBM Plex Mono", "Courier New", monospace;
}
```

Same font stack as Clark. Proven readable at long-form length.

---

## Three-Column Layout

### Left Rail — Article Spine

Section index tracking the article's movements:

```
ARTICLE INDEX
00  The Locked Phone
01  The Reversed Premise
02  Lineage
03  Five Principles
04  Three Layers
05  The Roadmap Ahead
```

Below the index, a "Source Corpus" block linking to the ZAKO document categories:

```
SOURCE CORPUS
CORE        7 documents
PROTOCOLS   3 documents
SERVICES    4 documents
PACKAGES    4 documents
RESEARCH    14 documents
DISTRIBUTIONS 7 documents
```

These are informational — no click-through unless we publish the specs separately.

### Center Column — Article Body

The prose from `zako-article-draft.md`, formatted with:
- Opening line in display serif (large)
- Section headings with numeric prefix in accent color
- Annotation markers inline as superscript buttons
- Figures for architecture diagrams (ASCII → SVG conversion)

### Right Rail — Technical References

Grouped by domain:

```
I · Architecture Sources
  ZAKO-Architecture-and-Vision.md
  BabbCat-Implementation-Plan.md
  
II · Protocol Sources
  BitPads v2.0 specification
  BitLedger v3.0 specification
  Wire Conventions v1

III · Hardware
  02-architecture.md (briefing)
  Vendor blob inventory
  Partition table reference

IV · Context
  01-vision.md (briefing)
  03-zambia.md (briefing)
  05-full-brief.md
```

---

## Annotation System

### Types of Annotations

Adapted from Clark's single annotation type to three for ZAKO:

1. **Technical Depth** (most common) — Wire formats, byte layouts, hex values, protocol specifics
   - Label: `TECHNICAL · [topic]`
   - Border-left color: accent-deep
   - Contains code blocks, diagrams, exact spec language

2. **Historical Context** — Unix history, Plan 9 background, CICS lineage, cuneiform references
   - Label: `CONTEXT · [topic]`
   - Border-left color: ink-faint
   - Prose-heavy, may include quotes from original papers

3. **Roadmap Note** — Experiments to run, open questions, what hasn't been tested yet
   - Label: `ROADMAP · [topic]`
   - Border-left color: accent (lighter, forward-looking)
   - Honest about unknowns, names specific experiments

### Markup Pattern

Same as Clark:

```html
<p>Five bytes. That's what it takes to encode a conserved scalar exchange.<button class="note-ref mono" data-note="n1" aria-expanded="false">n.01</button></p>

<aside class="annotation" id="n1" hidden>
  <p class="annotation-label mono">Technical · BitLedger Wire Format</p>
  <p>The 40-bit BitLedger record encodes a complete double-entry transaction:</p>
  <pre><code>Bits 0–4:   Source account (5 bits, 0–31)
Bits 5–9:   Sink account (5 bits, 0–31)
Bits 10–14: Amount class (5 bits)
Bits 15–29: Amount value (15 bits)
Bits 30–34: Status/metadata (5 bits)
Bits 35–39: Domain qualifier (5 bits)</code></pre>
  <p>Conservation is structural: the encoding format makes it impossible to construct a record where source ≠ sink in quantity. A malformed record fails at construction time, not at validation time.</p>
</aside>
```

### Toggle Behavior

Identical to Clark's app.js pattern — click marker, annotation slides open below paragraph with a subtle animation. Click again to close. `aria-expanded` tracks state.

---

## Diagrams

### Architecture Diagram (Three Layers)

Convert the ASCII diagram from the source document to an inline SVG:

```
┌─────────────────────────────────────────┐
│           THE VISIBLE LAYER             │
│  Applications · Telephony · Query       │
├─────────────────────────────────────────┤
│          THE SUBMERGED LAYER            │
│  Outstack · Telux Triad · Identity      │
├─────────────────────────────────────────┤
│           THE BEDROCK LAYER             │
│  TrustZone · dm-verity · Telux-SEC      │
└─────────────────────────────────────────┘
```

Render as SVG with:
- Accent border on the Bedrock layer (strongest guarantee)
- Subtle fill gradient from dark (bedrock) to light (visible)
- Monospace labels
- Hover states that highlight layer descriptions

### Power Mode State Machine

Five states in a horizontal flow:

```
FULL → NORMAL → CONSERVE → CRITICAL → EMERGENCY
 80%    60%      20%         5%         <5%
```

With hysteresis arrows showing the higher thresholds for recovery.

### Daemon Triad Diagram

Three boxes with their process class labels and relationships:

```
telux-ledgerd [CRITICAL]  ←→  telux-identd [CRITICAL]
                    ↕
            telux-sharedb [INTERACTIVE]
                    ↓
         SMS | QR | BLE | IP
```

---

## Content Mapping: Draft → HTML Sections

| Draft Section | HTML Section ID | Annotations Needed |
|---|---|---|
| The Locked Phone | `#locked-phone` | 1 roadmap (Cat S22 Flip specs) |
| Every Phone Serves Its Manufacturer | `#manufacturer` | 1 technical (GMS power draw data) |
| The Reversed Premise | `#reversed` | 1 context (deployment scenarios) |
| Writing Was Invented for Ledgers | `#ledgers` | 1 context (cuneiform history) |
| From Unix | `#lineage-unix` | 1 context (Unix philosophy paper ref) |
| From Plan 9 | `#lineage-plan9` | 1 technical (namespace → Docker lineage) |
| From seL4 | `#lineage-sel4` | 1 technical (capability model specifics) |
| From CICS | `#lineage-cics` | 1 context (CICS transaction volume stats) |
| Every Interaction Is a Record | `#principle-record` | 1 technical (BitPads frame structure) |
| Conservation at the Wire Level | `#principle-conservation` | 1 technical (BitLedger 40-bit format) |
| Sovereignty Is the Design Constraint | `#principle-sovereignty` | 1 technical (DID method, key hierarchy) |
| Offline-First | `#principle-offline` | 1 roadmap (channel testing plan) |
| Power Is a Sovereignty Property | `#principle-power` | 1 technical (Outstack mode thresholds) |
| Bedrock Layer | `#layer-bedrock` | 1 technical (dm-verity, TrustZone specifics) |
| Submerged Layer | `#layer-submerged` | 1 technical (daemon triad IPC) |
| Visible Layer | `#layer-visible` | 1 technical (AOSP build specifics) |
| The Roadmap Ahead | `#roadmap` | 2–3 roadmap (per experiment) |

Estimated total: ~18–22 annotations for Part I.

---

## Overlay System — Working Papers

Equivalent to Clark's working papers, ZAKO can surface:

1. **The Architecture Vision** (ZAKO-Architecture-and-Vision.md) — full source document
2. **The Protocol Spec** (BitPads/BitLedger specs) — for deep readers
3. **The Hardware Brief** (02-architecture.md) — Babb Cat specifics
4. **The Implementation Plan** (BabbCat-Implementation-Plan.md) — roadmap detail

These open in a full-page overlay dialog (same `<dialog>` pattern as Clark) rendered from markdown or served as pre-rendered HTML.

---

## File Structure

```
MobileOS/ZAKO/OUTPUT/
├── zako-article-draft.md      ← source prose (exists)
├── article-html-companion.md  ← this file (build guide)
├── site/
│   ├── index.html             ← the article
│   ├── styles.css             ← adapted from Clark
│   ├── app.js                 ← annotation toggle + scrollspy
│   └── assets/
│       ├── img/               ← diagrams, figures
│       └── papers/            ← overlay content (rendered specs)
```

---

## Build Process

1. **Draft prose** in `zako-article-draft.md` (iterate in Babb voice)
2. **Mark annotation points** — identify where technical depth lives
3. **Write annotations** — pull from source corpus (ZAKO/CORE, PROTOCOLS, etc.)
4. **Build HTML** — assemble index.html following this companion's structure
5. **Style** — adapt Clark's CSS with ZAKO color/identity changes
6. **Diagrams** — convert ASCII to SVG for architecture, power modes, daemon triad
7. **Test** — standalone file, works offline, readable on mobile (responsive breakpoints from Clark)

---

## Responsive Behavior

Same breakpoints as Clark:
- **>1080px**: Full three-column layout
- **720–1080px**: Two columns (left rail + article), right rail flows below as grid
- **<720px**: Single column, left rail above article, stacked

---

## Tone in Annotations

Annotations can be slightly more technical and denser than the main text — the reader opted in. But they still follow Babb's voice:
- No jargon without grounding
- Specific over abstract
- Honest about what's spec vs. what's implemented vs. what's untested
- Code blocks and hex values welcome here — this is the "for those who want it" layer
