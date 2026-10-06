# Decisions

## Decision: reuse the editor search bar for Ctrl+H replacement

Status: accepted
Date: 2026-10-06

### Context

Ctrl+F already uses an editor-owned GtkSearchBar and GtkSourceSearchContext, but
there are no replacement controls. The user requests Ctrl+H Find and Replace.

### Decision

Extend the same bar with an optional replacement row. Use GtkSourceView's match
replacement and Replace All on the canonical buffer with native undo. Register a
window action/shortcut and document-menu item. Repeated Ctrl+H keeps the bar open;
Ctrl+F returns to Find-only. Preview-only switches to editable source for replacement.
Find navigation starts after the selected match instead of selecting it repeatedly.

### Alternatives considered

Add a separate dialog/search model; implement manual string replacement; expose
only a shortcut without replacement controls. These duplicate existing ownership,
lose native match/undo behavior, or fail the requested workflow.

### Consequences

No settings migration, dependency or extra buffer. Search/replace widgets and
signals follow existing editor ownership. Regression coverage must exercise actual
Ctrl+H input, both operations/undo and the Ctrl+F transition.
## Decision: install natively in the user's prefix and verify the installed executable

Status: accepted
Date: 2026-10-06

### Context

The user cannot see the feature and requests a build, installation and screenshots.
The /usr/local executable already matches build-native; saved Dual Pane hides the
Formatted-only control. sudo requires authentication. Native dependencies are now
installed and ~/.local/bin is already in this user's PATH.

### Decision

Configure build-user with ~/.local prefix and install all assets through Meson.
Keep the existing application ID and explicitly bind the user desktop entry to
the user executable. Verify build/install hashes and capture that executable on
an isolated X11 display with synthetic Markdown and MathJax settings, separately
from the test harness. Leave existing settings, notebooks and system files intact.

### Alternatives considered

Require an interactive sudo reinstall; return to Flatpak; copy only the binary;
reuse test-harness captures. These respectively add an avoidable password step,
contradict the selected native route, leave mismatched asset paths, or do not prove
the installed executable renders the feature.

### Consequences

This user's shell and launcher select ~/.local/bin/marker. Updates use build-user's
compile/install commands without sudo; /usr/local remains available unchanged.
The desktop Exec override is user-specific. Installed-app captures use a separate
temporary profile so selecting MathJax does not change the user's backend choice.

## Decision: preserve Libadwaita widget styling on Zorin

Status: accepted
Date: 2026-10-06

### Context

The user reports missing sidebar borders and unwanted filled buttons after native
compatibility work. Zorin's full GTK stylesheet sets sidebar inset shadows to
transparent and overrides widget geometry/typography as well as named colors.
Loading another complete Zorin variant therefore exceeds the color-scheme fix.

### Decision

When the optional vendor theme-path identifies Zorin, use the installed Libadwaita
base and light/dark color resources in the existing process-local provider. Other
themes/high contrast retain their own provider path. Replace the deleted CSS
custom properties with one @borders inset divider per sidebar, respecting end
position and RTL. Simple icon actions explicitly use GTK's flat class; native
hover/focus/checked states, Pages/Files and the heading chooser remain distinct.

### Alternatives considered

Keep the full theme import and override each changed control; restore unsupported
GTK CSS variables; change desktop settings. These retain broad side effects,
break the older GTK build, or change user preferences outside Marker.

### Consequences

This supersedes the earlier full matching-Zorin-variant strategy. The native app
keeps the incumbent Folio/Libadwaita widget language while following the effective
scheme. Dividers use one GTK 4.14-compatible inset shadow and leave navigation
overlay shadows/layout intact. Validate both native and modern responsive themes.

## Decision: support Zorin's native GNOME libraries without a parallel UI

Status: accepted
Date: 2026-10-06

### Context

The user requests native Meson installation on Zorin 18.1 and authorizes adapting
Marker only if its existing functionality is preserved. Actual noble development
headers contain every GTK/GDK/adwaita/sourceview/spelling function used by Marker.
libspelling 0.2 includes the checker, menu, adapter and GActionGroup APIs in use.
GTK CSS custom properties require 4.16, while noble ships GTK 4.14. Runtime tests
also identify older overlay allocation and breakpoint minimum requirements.

### Decision

Target the tested noble floors GTK 4.14, libadwaita 1.5, GtkSourceView 5.12 and
libspelling 0.2. Keep WebKitGTK >=2.50, available from noble updates. Remove the
two custom sidebar-border variables and let native split views draw their normal
dividers; retain all named-color surfaces and widgets. Pin the fallback to upstream
libspelling 0.2.1 (0e9b8b2187ecda4dc1ce48a9bcc9a9976490fda2), which supports
GTK >=4.8, rather than 0.4.10 which itself requires GTK >=4.15.5. The modern
Flatpak manifest continues to provide 0.4.10 independently.

### Alternatives considered

Upgrade host GNOME libraries; remove features; maintain duplicate older/newer
widgets; lower minimums without compiling or testing. The shared API audit avoids
those costs. Native source/build/tests must still establish runtime compatibility.

### Consequences

No notebook/profile/data migration, identity change or editor rewrite. Native
divider colors follow the installed libadwaita theme. Validate spell checking,
light/dark/responsive UI, rich editing and scientific HTML/PDF against noble, and
retain a modern SDK build check. Missing host packages still require interactive
sudo installation; isolated extracted-package tests are not a system install.

## Decision: adapt allocation and the Zorin theme through existing public surfaces

Status: accepted
Date: 2026-10-06

### Context

GTK 4.14 redraws text overlays on scroll without reallocating their internal
container; rendered regions then remain at their previous coordinates. Adw 1.5
breakpoint windows need an explicit minimum. Zorin's patched style manager loads
a fixed Light/Dark theme from system settings even when Marker forces the opposite
scheme, producing dark navigation alongside a light editor. Changing application
GtkSettings does not control that vendor loader.

### Decision

Keep the shared rich-view layer and explicitly move/reallocate it on adjustment
changes, with object-bound signals connected after SourceView has its scroller.
Set the main breakpoint window minimum to 360x360. When the optional Zorin-only
style-manager theme-path property identifies a Zorin Light/Dark theme, load its
matching sibling stylesheet into one application-local provider above theme
priority. Derive the variant from the effective Adw scheme, including system-follow.
Remove that provider for other themes and high contrast; stock libadwaita needs
no override. Observe theme-path, dark and high-contrast changes.

### Alternatives considered

Versioned rich-view implementations; a new overlay widget; changing system theme
settings; ignoring Zorin's theme entirely; hardcoding every sidebar color. These
either duplicate architecture, change user preferences or lose native styling.

### Consequences

Modern GTK uses the same allocation path. Theme state remains in Adw and existing
Marker preferences; no additional persisted setting is introduced. Local theme
imports retain their asset origin. Native and modern visual tests assert matching
background/color-scheme behavior. System theme settings and user data remain intact.

## Decision: cancel readiness work when its WebKit document is replaced

Status: accepted
Date: 2026-10-06

### Context

Modern WebKit full-suite checks exposed a destroyed JavaScript completion context
during rapid notebook-style/preference reloads. Generation checks discard old
results, but a superseded load-finished event can schedule work using the current
generation before the next document starts. A queued buffer refresh also repeats
work already performed by an explicit refresh.

### Decision

One cancellable owns the current scientific/font readiness call. Cancel/clear it
on render replacement, navigation start, another completion request and disposal;
ignore cancellation while preserving genuine errors and the existing deadline.
Explicit editor refresh clears its pending buffer debounce. Keep generation checks.

### Alternatives considered

Retry all JavaScript errors; add fixed test sleeps; accept a failing modern run;
add separate old/new WebKit implementations. These obscure errors or duplicate work.

### Consequences

Superseded document work cannot fail the replacement render. No additional render
state, source model or export cascade is introduced. Existing rapid style/preferences
tests and scientific HTML/PDF exports exercise the shared path on both libraries.

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

Status: superseded
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

Superseded for the user's preferred installation route on 2026-10-06 by native
Zorin compatibility above. Flatpak remains a supported alternative.

## Decision: preserve explicit inline Source choices across presentation rebuilds

Status: superseded
Date: 2026-10-06

Superseded by the page-wide Source/Render choice below; per-region explicit state
and transfer logic are removed. Automatic caret/selection/error reveals remain.

### Context

Rich regions are recreated for refresh, resizing and editing. The previous boolean
was reconstructed only from caret position, losing a user's Source choice unless
the caret/selection stayed inside. Automatic caret, stale-text and failure reveals
must remain distinct from an explicit request to keep a region's source visible.

### Decision

Use one per-region display mode: rendered, automatic editing, or explicit Source.
Carry explicit Source ranges into replacement regions using current buffer marks
before disposing widgets/marks. Overlapping replacement ranges inherit that choice.
Render clears only its own region's choice. Keep the existing automatic reveal and
mode-switch reset behavior; no presentation settings or Markdown bytes are stored.

### Alternatives considered

Keep source visible only while focused; add a separate boolean alongside the
existing source state; retain a parallel long-lived list of user choices; recreate
regions without preserving intent. These either retain the bug or duplicate state.

### Consequences

A normal click stays in Source through refresh/resize, caret movement and edits/undo.
Multiple regions can stay open independently. Removing a region or leaving formatted
mode drops its ephemeral choice. Existing stale-result cancellation, source fidelity,
undo ownership and rendering backend remain unchanged. Tests must ensure automatic
stale-source reveals do not turn unrelated regions into sticky Source choices.

## Decision: rich controls own their pointer sequences from press

Status: accepted
Date: 2026-10-06

### Context

The user still reproduced hold-and-select after the source-intent fix. A real XTest
click demonstrates Source becoming Render on press and Source again on release.
GTK's native button gesture claims the sequence on release; until then the ancestor
text-view selection gesture moves the caret and automatically reveals source.
Verified against [GTK 4.14.5's button implementation](https://github.com/GNOME/gtk/blob/4.14.5/gtk/gtkbutton.c)
and native pointer dispatch on the installed library.

### Decision

Observe the native button controllers through GTK's public API and claim its click
gesture on press, retaining native release activation, cancellation and keyboard
behavior. Rendered-image gestures also claim on press. No alternate toggle handler,
press-state copy or text-view controller replacement is introduced. Controllers and
signals remain owned by their widgets; callbacks retain no RichItem pointers.

### Alternatives considered

Remember the mode at press and toggle from that copy; suppress all caret reveals
while a pointer is down; replace GtkButton's click logic; disable text-view selection.
These duplicate state or damage ordinary editing/activation behavior.

### Consequences

Source/Render activates once on release, without changing presentation or document
caret on press. Dragging away cancels normally. A dedicated native pointer test
exercises actual event dispatch rather than emitting clicked directly. GTK X11/X11/
XTest are optional test-only dependencies; unsupported builds/displays explicitly
skip this case while retaining platform-independent rich editing coverage.

The page-wide redesign removes buttons from the text-view hierarchy, so observing
their native controllers is no longer needed. Picture gestures still claim on press;
the real-pointer regression now exercises the ordinary toolbar button and images.

## Decision: one page-wide presentation choice belongs to the editor

Status: accepted
Date: 2026-10-06

### Context

The user requests optional scientific rendering and a single Source/Render control
to avoid plot evaluation while writing. Per-region controls/state are now redundant.

### Decision

Keep one live-editor boolean, initially Render, and one flat text action at the right
of the formatting toolbar. Source restores the full raw Markdown surface in the
canonical buffer; Render restores formatted presentation. Keep the choice through
view switches, without persisting it across close/reopen. Clicking a rendered image
requests page Source through the rich controller's signal. Automatic caret/selection
reveals in Render remain local editing behavior. Remove old region buttons, pinning
and the unused cursor-toggle API rather than maintaining two user-controlled modes.

### Alternatives considered

Keep both page/region toggles; add a second source buffer; persist a new preference;
place another action in the crowded document header. These add state, synchronization
or controls without serving the requested writing workflow.

### Consequences

Editing, undo, saving and dirty state retain their original ownership. The native
toolbar button sits outside the text-view gesture hierarchy and keeps standard
release/keyboard/cancellation behavior. Existing XTest coverage must run against it.

## Decision: suspend scientific documents and render full preview on demand

Status: accepted
Date: 2026-10-06

### Context

Formatted editing evaluates both a hidden full preview and the inline preview.
Hiding a WebView does not stop its document's workers or data-change monitoring.

### Decision

Formatted mode pauses the regular preview, avoiding duplicate SciDown/JavaScript
evaluation. Source invalidates requests, clears inline surfaces and disposes the
inline WebView; pause cancels readiness, releases data monitors, disables JavaScript
and navigates away to terminate document workers. Render recreates the inline
renderer lazily using existing preference/CSS/source context. Preview modes resume
the normal preview. Print prepares it explicitly and waits for WebKit's print job
completion before pausing again; exports keep their existing independent renderer.

### Alternatives considered

Hide widgets only; skip new refreshes while leaving workers running; introduce a
second scientific pipeline or global rendering preference. These fail true suspension
or duplicate the shared source/cascade/settings architecture.

### Consequences

Paused editing, CSS/preferences and data changes cannot evaluate plots. Explicit
Print/export still produces scientific output. Serial/generation guards remain;
signal lifetimes stay object-bound. Readiness fails explicitly for a paused preview.
Tests that inspect HTML must select Preview rather than rely on hidden rendering.
Pause completes pending readiness waiters with an explicit paused error; cancellation
must not strand a Print preparation loop. Print retains its editor across nested loops.
An old provisional WebKit load can commit after the blank request; the paused
load handler replaces such commits with blank again, without enabling JavaScript.

## Decision: bundle a self-contained local MathJax fallback

Status: accepted
Date: 2026-10-06

### Context

The local MathJax option points at system MathJax 2, absent from this native host and
the SDK. Charter also needs classification by the existing rich-region parser.

### Decision

Prefer installed system MathJax 2; otherwise load unmodified MathJax 3.2.2's combined
TeX/SVG component from the existing installed scripts tree. Retain its Apache 2.0
license and pinned provenance/hash. Disable its optional menu asset loading. Extend
shared readiness to await MathJax 3, configure inline delimiters on both runtimes,
and reuse existing inline/display/fenced math preprocessing. Add Charter to the
existing scientific-region classifier and disabled-source/error snapshot checks;
expose its existing preference in the rendering group.

### Alternatives considered

Require an extra host package; silently substitute KaTeX; use a CDN for local mode;
add another heading/science parser. These lose offline portability, backend intent
or architectural consistency.

### Consequences

No new host dependency or data migration. One 2.3 MB vendor asset is installed by
existing Meson/Flatpak data handling. Native/SDK tests must verify actual SVG output
for both math backends, Charter, Mermaid and gnuplot; system MathJax 2 remains an
explicit unexercised branch on environments without that package.
