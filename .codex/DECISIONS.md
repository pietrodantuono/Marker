# Decisions

## Decision: native rendered-region overlays over the canonical buffer

Status: accepted
Date: 2026-10-05

### Context

Formatted Markdown currently applies text tags only. Real images/math/tables/plots
already render in MarkerPreview. GtkSourceView invisible ranges have caused gutter
annotation faults; inserted child anchors would change canonical source offsets.

### Decision

Keep GtkSourceBuffer/SourceView editing and undo. A dedicated presentation controller
discovers whole renderable regions with the existing protected-block structure,
renders an annotated copy in one offscreen MarkerPreview, and displays bounded
textures as native scrolling overlays. Non-invisible tags reserve layout space.
Click/Source reveals original source; Render switches back. Inline math/images use
their containing paragraph. Rendering failures retain source. Exports remain normal
canonical-source renders; appearance and scientific runtimes are shared.

### Alternatives considered

WebKit contenteditable document (replaces native editing/undo/spelling/gutters);
projection buffer with anchors (duplicates document synchronization); one WebKit
per element (heavy process/widget lifecycle); raw invisible tags (known bug).

### Consequences

Rendered output is a visual surface with an accessible Source control, not a second
text editor. Whole-paragraph source editing keeps inline content coherent. Snapshot
memory is bounded; async generations reject stale ranges after edits/mode changes.
Unsupported nested syntax retains editable source rather than guessing boundaries.
The renderer is created only when a formatted document has renderable regions.
GTK overlay removal requires one zero-measure layer that owns normal widget children;
its empty shell stays with SourceView until disposal. A formatted-only native number
gutter suppresses numbers on compressed ranges and restores them in source context.
Snapshot coordinates account for display scale, with a 64 MiB readback budget.
Shared math-fence normalization and notebook-root draft asset resolution also fix
normal preview/export behavior without adding separate rendering paths.

## Decision: native column dividers and shared monospace defaults

Status: accepted
Date: 2026-10-05

### Context

The final Folio reference calls for softly tinted Pages/Files/Outline, a darker
rail, thin column dividers and a narrower monospace document. Opaque child surfaces
currently cover native split borders; Swiss overrides the preview base font.

### Decision

Keep the native split architecture and let pane styling own column boundaries.
Use named window/sidebar/view colors and transparent child/header surfaces; retain
native overlay shadows and high-contrast support. Change only fresh defaults to
640px and Monospace 11, with the shared preview base used without an optional theme.

### Alternatives considered

Manually draw borders on each child; fixed light/dark colors; introduce another
default theme file; force monospace over chosen themes; reset stored preferences.

### Consequences

The latest reference supersedes the earlier borderless-sidebar requirement.
No new widget/state or migration format. Explicit settings and notebook CSS still
win; Swiss and other existing themes remain selectable with the preview theme toggle.

## Decision: editor association and sidebar rows reuse existing objects

Status: accepted
Date: 2026-10-05

### Context

Pages combines open drafts/documents with recursive disk discovery. Unsaved drafts
need a notebook identity for safe removal and the later stylesheet context.

### Decision

An editor holds its MarkerProject; saved files use the most specific open root.
Pages holds editor or existing workspace item objects, deduplicated by GFile.
Files uses the existing lazy tree when browsing and a cancellable recursive snapshot
when searching. Discovery results use weak widget references and generation checks.

### Alternatives considered

Parallel Page/document stores; disable nested navigation; search expanded rows only.

### Consequences

The document buffer remains authoritative. Closing a page does not delete it from
disk. Draft Save As during removal can be cancelled without closing any pages.
Legacy one-level tree tasks retain their store after temporary root disposal;
recursive notebook scans are cancelled when navigation changes or is disposed.

## Decision: reuse open notebook objects across windows

Status: accepted
Date: 2026-10-05

### Context

Separate models for one root can overwrite newer appearance during session capture.
The rail needs context gestures and recycled-row lifetime management.

### Decision

Reuse an existing window's MarkerProject for the same root; no parallel registry.
Extract the rail during Phase 1 so rows, appearance bindings and menus have one owner.

### Alternatives considered

Global registry; reload every window after edits; temporarily put gestures in the window.

### Consequences

Appearance notifications update every open surface. Dialog drafts remain independent.
Phase 2 adds region headers to the existing rail instead of replacing a second factory.

## Decision: use the user-selected .codex feature workspace

Status: accepted
Date: 2026-10-05

### Context

The user supplied five empty plan files in .codex and asked implementation to follow them.

### Decision

Populate and continuously update these files instead of duplicating them under docs/features.

### Alternatives considered

Create another feature workspace; use a single running log.

### Consequences

One specification/plan/task/decision/journal trail remains authoritative across sessions.

## Decision: retain MarkerProject and extend its appearance

Status: accepted
Date: 2026-10-05

### Context

Project root identity, appearance and session metadata already exist.

### Decision

Keep the internal type and GKeyFile persistence, use Notebook in UI, extend with
notifying appearance properties and share one book widget between rail/editor.

### Alternatives considered

Parallel Notebook/appearance stores; rename every internal API; notebook metadata files.

### Consequences

No competing persisted model. New optional keys migrate legacy colors/icons safely.
Temporary editor drafts use the same appearance representation; Apply is atomic.

## Decision: Move means notebook ordering

Status: accepted
Date: 2026-10-05

### Context

The context-menu specification was ambiguous; current Marker supports rail ordering.

### Decision

The user selected Move Up/Down. Preserve Locate for externally moved folders.

### Alternatives considered

Physical filesystem move; both physical move and ordering.

### Consequences

No folder-moving/data-migration workflow. Remove never deletes a notebook directory.

## Decision: native responsive splits with separately persisted intent

Status: accepted
Date: 2026-10-05

### Context

Existing show-sidebar writes can persist responsive auto-hide/focus behavior.

### Decision

Use native split views; store desired page/outline visibility per window. Transient
overlay dismissal/resizing/focus do not write that intent. One narrow overlay at a time.

### Alternatives considered

Manual reparenting/duplicate sidebars; global actual-widget visibility settings.

### Consequences

Less custom navigation logic and no false closed default at 1200x800; migration uses
legacy sidebar preferences where present rather than resetting explicit choices.

## Decision: normal notebook precedence with standard CSS importance

Status: accepted
Date: 2026-10-05

### Context

Preview injects user-origin important dark/fonts; HTML export omits those preferences.
Some existing global themes intentionally use important declarations.

### Decision

The user selected standard CSS semantics. Share author-origin style composition;
normal layer precedence is base/theme/preferences/notebook. Remove app-forced
important overrides. Keep native CSS importance semantics and real file origins.

### Alternatives considered

Force/normalize all CSS; independent WebKit injection/export CSS; handwritten URL rewriting.

### Consequences

Normal notebook rules win normal global conflicts. Important follows browser rules,
including layer importance reversal. Relative assets work through native file URI
loading; portable HTML asset bundling is expressly out of scope.

## Decision: consolidate Marker structure discovery, preserve scientific renderer

Status: accepted
Date: 2026-10-05

### Context

Formatted source, outline and page titles separately scan headings. SciDown remains
needed for includes, numbering, Beamer, charts and export.

### Decision

Extract pure structure discovery reused by source/gutter/outline/title. Keep one
GtkSourceBuffer per document and the existing presentation-only delimiter technique.

### Alternatives considered

New rich-text representation; parser per surface; replace SciDown.

### Consequences

Source remains authoritative. Navigation must handle UTF-8 source offsets and
correct preview render readiness without relying on non-unique scientific TOC IDs.

## Decision: share cached structure and native heading gutter

Status: accepted
Date: 2026-10-05

### Context

Caret movement reparsed all spans, heading controls changed literal code, and
outline offsets became stale while waiting for a refresh.

### Decision

SourceView owns the structure/span cache. Text changes invalidate it; caret changes
only update presentation and chooser state. A native GtkSourceGutterRendererText
owns heading interaction; built-in line numbers stay independent. Outline rows use
GtkTextMarks until their 180ms debounced rebuild. Both heading controls use one
source edit operation with grouped undo.

### Alternatives considered

Second heading models/parsers; window-specific gutter drawing; absolute outline
offsets; an additional rich-text document representation.

### Consequences

Presentation keeps source bytes unchanged and native gutter layout places H markers
beside the centered measure. Source/preview navigation shares character offsets;
render anchors are inserted outside protected blocks to preserve scientific code.

## Decision: one native author cascade and render completion contract

Status: accepted
Date: 2026-10-05

### Context

Inline stylesheet copies lose relative asset origins, preview's user-important
styles bypass notebook declarations, and printing could precede page loading.

### Decision

One render-style helper composes named author layers and native URI imports in
base/theme/preferences/notebook order. Both HTML paths are replaced by one renderer.
Document base URI preserves relative Markdown assets. Root directory monitoring
covers atomic stylesheet replacement; render version query prevents stale CSS cache.
PDF renders an independent originating-document snapshot. WebKit readiness waits
for page/style/font/diagram work and gnuplot, rejects stale render serials and times
out at 30 seconds. CLI exports find the nearest ancestor .marker.css.

### Alternatives considered

Inline notebook CSS with URL rewriting; separate export CSS; user-origin overrides;
print the active preview irrespective of render generation.

### Consequences

Normal notebook rules win regardless of selector specificity in earlier layers.
Important declarations follow standard layer importance reversal. HTML keeps local
asset dependencies (portable bundling is out of scope). Export dialogs retain the
originating editor without changing user selection; source buffers remain authoritative.

## Decision: layer scientific document styles within the shared cascade

Status: accepted
Date: 2026-10-05

### Context

SciDown's YAML `style:` previously emitted an unlayered link, which would outrank
normal notebook rules despite the new preview/export layer order.

### Decision

Preserve metadata styles as native URL imports in a `scidown-document` author layer,
after the global theme and before preferences/notebook rules. The host header emits
its base URI and layer declaration before the metadata import. Escape CSS strings.

### Alternatives considered

Ignore existing document styles; copy/rewrite their CSS; postprocess rendered HTML;
introduce a second preview/export composition path.

### Consequences

Scientific metadata remains supported, relative URLs retain their document origin,
and normal notebook declarations win. Standalone SciDown still loads its metadata
stylesheet; standard important semantics apply. No Markdown/metadata migration.
## Decision: install this checkout through a local Flatpak

Status: accepted
Date: 2026-10-05

### Context

The host GTK headers are below the branch minimum and its other native development
dependencies are incomplete. The user requests a committed, documented installation
route. GNOME 50 Platform/SDK are available; a development binary alone is not a
desktop installation. The actual optimized package exposed errors missed by the
debug build.

### Decision

Recommend the existing local-source Flatpak manifest with a user installation.
Use `/app/lib` consistently for its Meson modules. Exclude generated build/cache
directories from sources and Git. Keep native and SDK development instructions
separate; validate an actual optimized package, launch and export before committing.
Commit renderer submodules before their parents without pushing.

### Alternatives considered

Upgrade the host's GNOME libraries; add a wrapper that runs an SDK binary; document
installation without testing the package. None provides as direct and reproducible
a desktop installation on this host as the existing manifest.

### Consequences

Flatpak has its own settings/state profile. It bundles the required runtime and
libspelling; optional Pandoc office export still needs Pandoc inside that environment.
Local renderer commits must be published to reachable remotes before fresh clones
on another machine can retrieve the updated submodule pointers. No user profile or
notebook is changed during package validation.
