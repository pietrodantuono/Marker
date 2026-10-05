# Folio-inspired Marker redesign

## Problem

Marker's GTK4 notebook prototype still concentrates controls in one large header,
uses square notebook badges, splits open documents from notebook files, and has
an editor-only outline popover. Appearance and rendering have duplicated logic.
The interface should support focused writing without losing scientific or nested
project workflows.

## Goals

- A Folio-style notebook rail, page column, document surface, and right outline.
- A single notebook appearance model, shared by icons, editor, and persistence.
- Responsive native GTK/Libadwaita navigation with reliable restored choices.
- Formatted Markdown with a centered measure, heading gutter, and source fidelity.
- Notebook-local CSS shared by preview, HTML/PDF export, and printing.
- Verified removal of obsolete GTK3 complexity and a durable implementation trail.

## Non-goals

- Replacing SciDown or removing scientific Markdown, nested assets, or view modes.
- Physically moving/deleting notebook folders, adding a trash system, or autosaving.
- A new rich-text document format; Markdown remains the authoritative source.
- Portable HTML asset bundling or CSS parity for LaTeX/office formats.
- Committing or pushing without a separate instruction.

## UX and functional requirements

### Notebooks

- Approximately 48px rail: flat full-width rows, left accent, selection tint,
  26x34px book covers with a spine and page edge, matching Folio geometry.
- User-facing name: Notebook; internal MarkerProject name is retained.
- First characters, initials, camelCase initials, snake_case initials, predefined
  symbol, and custom-text icon modes; UTF-8-safe text and arbitrary opaque colors.
- Contrast-aware cover foregrounds and theme-aware notebook/page accents.
- Right-click/long-press Edit, Reveal in Files, Move Up/Down, Remove.
- Removal never deletes the folder and respects dirty-document cancellation.
- Adaptive editor: live book preview, name, icon mode, conditional symbol/custom
  controls, color chooser, Apply. Cancellation changes no notebook appearance.
- Names are display aliases, not folder renames. Folder URI remains identity.

### Navigation and controls

- Approximately 240px page column with New Page/title/Search header and Pages/Files.
- Pages shows recursively discovered Markdown and associated open drafts, with
  useful subtitles, open/dirty state, and appropriate close affordances.
- Files preserves nested assets and includes .marker.css; Pages excludes CSS.
- Search operates across nested notebook content, including unopened directories.
- Standalone documents remain accessible through an Open Files navigation entry.
- Rail owns application menu and Add/Open Notebook; document header owns title,
  view mode, Outline, overflow, and a discoverable dirty-document Save action.
- Save As, export, print, source search, fullscreen, focus, and secondary actions
  remain available through overflow/shortcuts rather than a cluttered header.
- Formatting toolbar is visible only when Markdown editing is relevant.
- Pages/Files and Outline use the soft window background, matching their headers;
  the notebook rail is slightly darker. Single neutral column dividers follow the
  Folio reference, with native shadows reserved for navigation overlays.

### Writing and outline

- Default formatted writing measure approximately 640px, centered and shrinking
  to fit available space; comfortable vertical padding and light/dark typography.
- Default formatted and rendered text is Monospace 11. Fresh previews use the
  shared base style; explicitly selected fonts, themes and widths remain respected.
- H1-H6 gutter, interactive heading chooser, independent optional line numbers.
- Delimiters hidden outside active editing context without changing source text.
- A right outline approximately 240px wide: hierarchy, indentation, title, close,
  debounced live updates, and navigation of the active editor/preview.
- One Marker heading discovery implementation feeds gutter, outline, and titles;
  SciDown's scientific document renderer remains separate.
- HTML preview matches writing width/typography where practical, without a gutter.

### Notebook styles

- Optional .marker.css at notebook root. Edit Notebook Styles creates it only if
  absent and opens it in source-only CSS mode through the normal save workflow.
- Normal-rule precedence: renderer base, global theme, dark/font preferences,
  notebook CSS. Standard !important semantics remain (confirmed by user).
- Existing scientific metadata `style:` files remain supported between global
  themes and preferences, retaining their document-relative URL origin.
- Marker-generated dark/font styles must not forcibly outrank notebook styles.
- Relative URLs/imports resolve against the notebook. Creation/change/replacement/
  deletion refresh relevant previews after a debounce.
- HTML/PDF/print share the same style composition. Existing local asset references
  remain valid; exported HTML is not guaranteed portable without its source assets.

## Responsive behavior

- Fresh-profile window defaults: 1200x800; rail/pages open, outline closed.
- Wide >=1100px: rail/pages/document; optional outline alongside document.
- Medium 700-1099px: rail available, page/outline overlays.
- Narrow <700px: notebook/page/outline overlay surfaces, one overlay at a time.
- Automatic dismissal, resizing, and focus mode do not overwrite explicit choices.
- Native gestures, keyboard focus, accessible names, empty/loading/error states.

## Persistence and compatibility

- Extend the existing XDG-state GKeyFile session; no second metadata store.
- Preserve old project URI/name/icon/color records; migrate named palette colors.
- Add icon mode/custom text and per-window page mode/desired sidebar/outline state.
- Preserve explicit window size, fonts, view mode, and visibility preferences.
- Missing folders, invalid optional keys, standalone selection, and multiple windows
  must restore safely. Unsaved text remains protected by the existing close flow.
- Preserve all five view modes, CLI switches, spelling, math, Mermaid, gnuplot,
  includes, scientific metadata, Beamer, export formats, and existing shortcuts.

## Verified existing architecture

- AdwApplication startup in marker.c; AdwApplicationWindow in marker-window.c.
- Window owns project/document stores and selections, an editor stack, and three
  native overlay splits; reusable rail/page/outline widgets own their region UI.
- MarkerProject owns root identity, appearance and stylesheet monitoring;
  marker-session.c extends the existing session.ini with additive keys.
- MarkerWorkspaceItem provides lazy children/metadata plus cancellable recursive
  discovery for Pages and search. Sidebar rows reuse workspace items or editors.
- MarkerEditor owns one authoritative GtkSourceBuffer, notebook context, source
  surface, preview, formatting toolbar and all five view modes.
- MarkerSourceView caches the shared structure/formatted spans; native heading
  gutter and live outline reuse that structure and one undoable heading operation.
- MarkerRichView owns ephemeral rich-region presentation over that same buffer:
  one lazy offscreen preview, bounded textures, scrolling overlays and source marks.
  A formatted-only number gutter omits rendered ranges; source mode remains native.
- SciDown renders scientific HTML/LaTeX. One render-style helper supplies preview,
  HTML and WebKit print/PDF; scientific readiness includes gnuplot/diagram/font work.
- GTK4 >=4.18, Libadwaita >=1.7, GtkSourceView5 >=5.14, WebKitGTK6 >=2.50.

## Acceptance criteria and definition of done

### Inline rich elements in Formatted Markdown (follow-up)

- Images, mathematical expressions, Markdown tables and scientific fences render
  in the writing page by default. Inline images/math retain their surrounding
  paragraph; clicking its rendered region exposes that paragraph's source.
- Click or activate Source to edit the original region in the existing editor;
  Render switches back. Keyboard navigation/search into a region exposes source.
- The GtkSourceBuffer remains authoritative. Presentation must not insert object
  characters, rewrite source, change dirty state, or create a separate undo history.
- Use the existing scientific renderer and effective notebook/global styles.
  Refresh after edits, notebook CSS changes and linked gnuplot data changes.
- Failed/unavailable/oversized rendering leaves readable, editable source and a
  local explanation/retry control. Other regions remain usable.
- Rendered regions are visual snapshots; links and interactive chart controls use
  the regular preview. Complex list/quote nesting keeps source presentation until
  representative documents establish safe rendering boundaries.
- Preserve headings/gutters, outline, search, selection, exports and all view modes.
- Exercise source fidelity, undo, Unicode, incomplete syntax, stale asynchronous
  results, teardown, light/dark and responsive writing measures with real GTK/WebKit.

- All requirements above work in light/dark and wide/medium/narrow states.
- Fresh defaults and persisted explicit choices are exercised in isolated profiles.
- Notebook appearance, safe removal, nested navigation/search, dirty state,
  formatted editing, gutters, outline, live CSS, HTML/PDF export are exercised.
- Builds/tests pass; schemas/resources/manifests validate; ownership and diff review
  complete; obsolete superseded code removed where verified safe.
- All tasks complete or explicitly deferred with rationale, impact, and next step.
- PLAN/DECISIONS reflect the actual implementation; PROGRESS ends with Final handoff.
- Compilation alone does not establish completion; unavailable checks are recorded.
