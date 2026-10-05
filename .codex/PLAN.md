# Implementation plan

## Working rules and gate

Use these five .codex documents as the sole feature workspace (user-selected).
Preserve inherited changes. Work in coherent, independently reviewable chunks.
Update TASKS/PROGRESS after each chunk, DECISIONS for meaningful choices, PLAN when
discoveries alter the route, FEATURE only for agreed semantic clarification.
No automatic commits/pushes. Deletions require caller, dynamic, resource,
translation, packaging checks and a subsequent build.

The repository inspection and second planning pass are complete before production
edits. Phase 0 begins with the observed passing GTK4 build and 9 passing suites;
the Flatpak JSON and Debian GTK3 metadata still need repair.

## Phase 0: baseline

- Objective: reliable current build/package and independently verified cleanup.
- Modules: Meson, manifest, Debian metadata, configure, resources, po/POTFILES,
  unused GTK3 helpers/sketcher/WebKit extensions, string/path helpers, renderer.
- Changes: repair JSON/dependencies, document SDK build, remove dead modules and
  their extraction references, use GLib paths, fix double-prefixed stylesheet path.
- Dependencies: feature documents and recorded second-pass gate.
- Risks: resource/translation omissions, old package names, unintended parser churn.
- Migration: retain persisted schemas/keys and scientific renderer semantics.
- Validation: build after removals, tests, JSON/schema/resource/desktop/AppStream,
  compiler warnings, diff. Do not claim full package installation from JSON checks.
- Exit: baseline builds/tests/metadata checks pass, removals have reference evidence.

## Phase 1: notebook appearance

- Objective: one appearance model, reusable book icon, adaptive editor, safe actions.
- Modules: project/session, new book/editor widgets, resources, window actions/tests.
- Changes: notifying properties, six text/symbol modes, arbitrary color with contrast;
  atomic Apply from a draft; row-targeted context menu; dirty-safe removal/reordering.
  Extract the rail now because it owns recycled rows and context gesture lifetimes.
- Dependencies: baseline; no physical folder move is required.
- Risks: recycled rows, CSS provider/signals, async dialog targets, dirty close flow.
- Migration: additive session appearance keys; old icons/palette colors preserved;
  new names are display aliases. Invalid optional values fall back safely.
- Validation: legacy/new round trips, Unicode/modes/colors, draft Cancel/Apply,
  right-click/long-press, ordering, dirty removal cancellation, missing roots.
- Exit: appearance surfaces and restore agree; notebook actions preserve user data.

## Phase 2: shell/navigation

- Objective: Folio columns with region-owned controls and responsive native layout.
- Modules: window, extracted rail/page widgets, workspace scans, editor association,
  session/settings, application actions, CSS/resources.
- Changes: 48px rail, 240px pages, recursive Pages/lazy Files and mode-aware search,
  Open Files entry, draft root association, distributed headers, native split views.
- Dependencies: appearance model/widgets.
  Rail construction/gestures are already extracted; this phase adds its region header.
- Risks: duplicate page rows, stale async scans, association on Save As, split-view
  auto-hide overwriting desired state, narrow overlay coordination, multiple windows.
- Migration: session per-window state authoritative; legacy show-sidebar used for
  default/migration only. Update fresh defaults without rewriting explicit settings.
- Validation: 1200x800 fresh; 1100/700 boundaries, restored explicit hidden state,
  nested assets/search, draft save, dirty/open rows, loose files, focus/fullscreen.
- Exit: all navigation/control actions accessible across widths and sessions.

## Phase 3: document/outline

- Objective: focused writing, shared heading structure, independent gutters/outline.
- Modules: formatted parser, source view, editor, reusable gutter/outline, shell CSS.
- Changes: pure shared structure discovery, cached spans/headings, H1-H6/Setext,
  fenced/indented code/frontmatter exclusions; centered measure (640px after the
  reference follow-up); independent gutters;
  shared heading edit/undo; compact relevant toolbar; right debounced outline.
- Dependencies: shell with right split surface and editor notebook context.
- Risks: Unicode byte/character offsets, stale parse generations, gutter allocation,
  preview readiness/duplicate cursor markers, detached previews, source timers.
- Migration: preserve Markdown bytes, view enums/CLI/shortcuts and font preferences;
  no new rich-text format or SciDown heading/numbering rewrite.
- Validation: parser edge cases, source/undo preservation, caret/selection delimiter
  context, numbers independent of markers, outline jumps in all five view modes.
- Exit: writing and navigation work without modifying source during presentation.

## Phase 4: notebook CSS

- Objective: root stylesheet workflow and one preview/export/print cascade.
- Modules: project context/monitor, workspace filtering, CSS document mode,
  shared render style helper, renderer/preview/exporter/preferences.
- Changes: .marker.css create/open, source-only CSS, normal author cascade layers,
  no forced app !important, real file URI origins; monitor directory for replacement;
  shared style context and render readiness; capture original export document.
  Phase 5 reference review also layers existing scientific metadata styles within
  this cascade and consolidates safe stylesheet creation on MarkerProject.
- Dependencies: notebook association and reusable document structure.
- Risks: author-layer important reversal, file origin/cache, absent/read-only files,
  monitor replacement/disposal, PDF printing before resources complete.
- Migration: global themes keep names/paths and existing choices; HTML local assets
  retained, no asset bundler. LaTeX/office exports keep existing behavior.
- Validation: CSS normal conflicts/important semantics, fonts/dark, relative assets,
  nested notes, creation/edit/save/replacement/delete, HTML/PDF and gnuplot staging.
- Exit: effective cascade agrees across supported web render/export paths.

## Phase 5: final cleanup/acceptance

- Objective: remove superseded complexity and verify FEATURE acceptance matrix.
- Modules: obsolete header/outline APIs, regression tests/build, docs and all changed UI.
- Changes: remove verified unused leftovers, port/register GTK4 checks, warnings,
  isolated profile/fixture validation and application-only screenshots, final handoff.
  Review fixes include native outline window controls, visible structural list
  punctuation, source column guides confined to source modes, generic CSS fonts,
  sidebar title options/sorting/search updates and CSS Save As language transitions.
- Dependencies: all prior exit criteria.
- Risks: overstating manual/package coverage, unrelated diff churn, lifetime regressions.
- Migration: exercise old/new sessions and explicit preference choices before closure.
- Validation: every FEATURE acceptance item, light/dark wide/medium/narrow and keyboard,
  compiler/runtime output, full tests, metadata, export, final ownership/diff review.
- Exit: all work complete or expressly deferred; documents match actual implementation.

### Follow-up: Folio surfaces and default typography

- Objective: apply the user's final reference correction to column backgrounds,
  dividers, default measure and monospace typography.
- Modules: shell CSS/native split classes, settings schema, preview base, UI checks.
- Changes: let native split panes own their backgrounds/dividers instead of opaque
  child surfaces; use window colors for Pages/Files and Outline, sidebar colors for
  the rail, view colors for document chrome. Narrow fresh writing width to 640px
  and prose to Monospace 11; fresh previews use the shared base instead of Swiss.
- Dependencies: completed Phase 5; existing shared rendering and preference paths.
- Risks: nested pane selectors, borders obscured by child backgrounds, overlay
  shadows, chosen theme fonts inadvertently overridden, differing writing measures.
- Migration: change schema defaults only; retain explicit font/width/theme choices
  and notebook CSS precedence. No appearance/session format or widget duplication.
- Validation: actual GTK/WebKit defaults and explicit overrides, regenerate and
  inspect the bounded light/dark/responsive screenshot matrix, full suites,
  warning/schema/resource/diff checks and isolated installation.
- Exit: reference correction works in both themes and responsive states; update
  all five documents and final handoff with the new evidence.

### Installation and commit handoff

The user now explicitly requests committing the current changes and documenting
installation. README will distinguish a local GNOME 50 Flatpak from a native build
requiring all declared host development versions, and from SDK-only development.
Validate an actual optimized local package using flatpak-builder, excluding generated
build/cache inputs; fix package library paths and any optimized-build warnings.
Exercise installed launch and HTML/PDF/scientific export with isolated state and an
isolated Flatpak installation directory. Review relevant metadata/tests and staged
changes. Commit the modified nested Charter repository,
then SciDown and its updated Charter pointer, then Marker on the existing feature
branch. Preserve user data; no format migration, extra launcher or direct push.
Local submodule commits must be published to reachable repositories before another
machine can clone the parent feature commit. Full Debian release builds remain deferred.

## Phase 6: inline rich elements (2026-10-05 follow-up)

- Objective: rendered scientific/image/math/table regions inside the existing
  Formatted Markdown writing page, with in-place source editing.
- Modules: shared Markdown structure, new rich-region presentation controller,
  SourceView/Editor integration, existing Preview and GTK regression tests.
- Changes: discover complete regions using shared protected-block structure;
  annotate a rendering copy retaining document references/context. One lazy
  offscreen MarkerPreview renders with existing styles/runtime; bounded snapshots
  supply native text-view overlays in one zero-measure owning layer. A formatted
  number gutter omits compressed source lines. Tags reserve space without anchors or source
  mutation. Source/Render edits original GtkTextBuffer ranges. Cursor/selection
  reveal source; edits invalidate results and debounce refresh.
  Shared math-fence normalization also serves normal preview/export. Unsaved notebook
  drafts use the notebook root for relative assets without creating a backing file.
- Dependencies: completed phases 0-5 and inspection of this current checkout.
- Risks: text layout/gutter validity, image completion, texture memory, asynchronous
  teardown, stale source offsets, complicated nested markup.
- Migration: none; presentation state ephemeral, persisted Markdown unchanged.
- Validation: Unicode/protected/incomplete discovery and annotation; real GTK/WebKit
  images/math/tables/gnuplot, toggle/edit/undo/selection, CSS/data refresh, modes,
  narrow layouts, scrolling, 2x display scale, disposal, missing-image/invalid-plot
  recovery, live CSS/CSV and real HTML/PDF export; full build/tests/resource/diff review.
- Exit: actual rendered output, source/save/undo fidelity, stale-result protection,
  existing suites pass, screenshots and handoff document validated limitations.
- Second pass: retain native editor/canonical buffer; no projection buffer or
  per-element WebKit views; no second heading parser; avoid invisible tags (known
  annotation issue). Separate discovery/presentation, bound memory and discard
  async results after edits/teardown. Export consumes canonical source normally.

## Build and validation commands

Host GTK 4.14.5 headers are installed, below the 4.18 branch minimum; the other
native development dependencies are incomplete. Use the installed GNOME 50 SDK.
`build-gtk4` is configured with an isolated prefix inside the ignored build tree:

```sh
flatpak run --filesystem="$PWD" --command=sh org.gnome.Sdk//50 \
  -c 'meson compile -C /home/pietro/Code/Misc/marker/build-gtk4'
flatpak run --filesystem="$PWD" --command=sh org.gnome.Sdk//50 \
  -c 'meson install -C /home/pietro/Code/Misc/marker/build-gtk4'
jq empty com.github.fabiocolacio.marker.json
glib-compile-schemas --strict --dry-run data
glib-compile-resources src/resources/marker.gresource.xml \
  --sourcedir=src/resources --generate-dependencies
desktop-file-validate data/com.github.fabiocolacio.marker.desktop
appstreamcli validate --no-net data/com.github.fabiocolacio.marker.appdata.xml
git diff --check
```

Use MARKER_STATE_DIR and isolated GSettings/XDG directories for UI tests. Never
write verification fixtures or test sessions into a user's notebook/profile.
The registered UI suites create their own isolated state/fixtures. They require a
display; without one they explicitly skip. Final checks used a task-owned Xvfb
display :92 and these SDK environment flags:

Start an Xvfb display with `Xvfb :92 -screen 0 1600x1000x24 -nolisten tcp`
when testing without a desktop; stop that task-owned server after validation.

```sh
DISPLAY=:92 flatpak run --filesystem="$PWD" --socket=x11 \
  --env=GDK_BACKEND=x11 --env=GSK_RENDERER=cairo \
  --env=LIBGL_ALWAYS_SOFTWARE=1 --env=GTK_A11Y=none \
  --env=GSETTINGS_BACKEND=memory \
  --env=WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 \
  --command=sh org.gnome.Sdk//50 \
  -c 'dbus-run-session -- meson test -C /home/pietro/Code/Misc/marker/build-gtk4 --print-errorlogs'
```

The WebKit override is confined to this isolated nested-SDK test process (its
flatpak-spawn portal is unavailable); production sandbox settings stay unchanged.
Set DISPLAY on the host invocation: Flatpak replaces --env=DISPLAY with the host's
display when setting up its socket. Verify the resulting window on :92 before
claiming a headless run. The installation handoff reruns all suites this way.
Set MARKER_SCREENSHOT_DIR to .codex/screenshots to capture the application-only
acceptance matrix. No screenshots contain the desktop or real notebook content.

Phase 6 used the same isolated SDK flags on task-owned display :93. The final full
run passed all 11 suites (window workflows: 44.60s). Repeat the rich-editing workflow
with GDK_SCALE=2 for texture/display scaling. Rich captures are rich-light.png,
rich-dark.png and rich-narrow.png. The optimized local Flatpak was rebuilt and
reinstalled in the user's existing installation; save and restart a running app
to load it. Test-only Xvfb is stopped after validation.
