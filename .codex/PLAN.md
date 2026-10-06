# Implementation plan

## Rendered font scale correction (2026-10-06)

Inspect actual font/override/theme and saved zoom before changing shared typography.
The user's preview zoom is 2.1; source/prose is Monospace 11 and font/theme overrides
are disabled. Reset only preview zoom to its existing 1.0 default. Preserve the
640px writing measure, heading hierarchy, fonts and notebook CSS. Verify readback,
existing native typography workflow and task diff; no production change/rebuild is
needed for a persisted preference correction. Record how to reset zoom in a live pane.

Exit met: saved zoom reads 1.0; prose remains Monospace 11 and width 640. Existing
native /window/default-typography passes (2.17s) on isolated Xvfb; installed binary
matches build-user. README documents focused-preview Ctrl+0 reset. No production
code, schema or font defaults changed; task-owned display stopped after validation.

## Find and Replace shortcut (2026-10-06)

- Reuse the editor's GtkSearchBar and GtkSourceSearchContext; add an optional
  replacement row with Replace and Replace All, rather than a separate dialog.
- Register win.replace on Ctrl+H and expose Find and Replace in document overflow.
  Ctrl+H always opens/focuses the controls; Ctrl+F retains Find-only navigation.
  Keep replacement in the original buffer with native undo and dirty tracking.
- Check selected-match safety, next-match progress, empty replacement, no matches,
  Replace All/undo, repeated opening and Preview-only switching to editable source.
- Exit: focused GTK regression and native suites pass, native user installation
  updated with its explicit launcher, README/tracking/diff reviewed. No persistence
  or schema migration is needed.

Exit met: actual Ctrl+H input and replacement workflow pass; optimized build-user
and all seven native suites pass (17 window workflows, 89.24s, no skips). The full
run uses timeout multiplier 2 for isolated portal startup. Native install/launcher,
schema and diff checks pass. Existing GTK test deprecation warnings remain.

## Page-wide Source/Render and scientific coverage

1. Complete discovery/snapshot support for Charter; use the existing math/Mermaid/
   gnuplot renderer and readiness. Supply a bundled local MathJax SVG fallback
   because this host lacks the existing system MathJax path. Retain system MathJax
   2 when available, existing KaTeX and disabled-renderer preferences.
2. Replace per-region controls/cursor toggle API with one editor-owned Source/Render
   action at the right of the formatting toolbar. Default Render; Source shows raw
   Markdown in the same buffer. Remember choice per live editor, without settings
   migration. Rendered-element clicks request page Source through a controller signal.
3. Disable/dispose inline renderer on Source; pause the hidden full preview and
   skip scientific conversion on edits, resize, preferences, CSS/data refresh. Render
   only the inline renderer in Formatted mode. Explicit Print prepares its full
   preview on demand and pauses again afterwards; exports retain their own pipeline.
4. Replace obsolete per-region tests with real-pointer page-toggle coverage;
   verify all engines, source fidelity/undo, stopped rendering through edits/refresh,
   resume, disabled engines, errors, CSS/data updates, view modes and export. Build/
   test native and modern stacks, document behavior and reconcile final handoff.
   Screenshot review additionally found a stale title/Save after undo: connect
   GtkTextBuffer's actual modified-changed signal and assert visible clean state.

Review gate: one editor choice, one buffer, existing parser/cascade/backend. Remove
superseded region-button/pinning APIs instead of parallel controls. Dispose monitors,
timeouts and WebKit readiness; generation checks reject stale callbacks. Pausing
must destroy page workers, not only hide snapshots. No notebook/source migration;
explicit rendering/print/export may evaluate graphics after user requests it.

Exit (2026-10-06): all four implementation units completed. Native 7/7 and modern
11/11 suites pass; actual science/disabled prefs/pause/unload/undo/title/pointer/
CSS/data/HTML/PDF checks, screenshots, resources and diff review pass. The later
user installation/proof follow-up below supersedes the system-reinstall deferral.

## User installation and visible proof (2026-10-06)

- Inspect installed binary/assets, launcher, saved view and sudo availability.
  Earlier /usr/local already contains the code; the saved Dual Pane view explains
  the missing Formatted-only toggle. Preserve the user's existing preferences.
- Configure build-user with ~/.local prefix, build/install all native assets and
  explicitly target its executable from the user desktop entry. No new app ID,
  data migration, production code or system modifications are required.
- Run all seven native suites on a task-owned Xvfb display; desktop-focus-dependent
  failures must be recorded rather than counted as passes.
- Capture the actual installed executable with isolated settings/session state
  and synthetic MathJax/Mermaid/Charter/gnuplot Markdown. Use a separate app launch
  from the test harness; review both page-wide toggle states and rendered charts.
- Exit: matching build/install hash, validated launcher, seven passing suites,
  reviewed direct captures and updated TASKS/PROGRESS/INSTALLATION-PROOF handoff.

## Historical inline Source/Render follow-up (superseded by the page control)

- Reopened after user report: signal-emitted clicks bypass native pointer dispatch.
  Reproduce press/release on the actual Source button through XTest on an isolated
  X11 display before further production changes. Inspect ancestor gestures, caret
  reveals and button layout between press and release. Keep pointer dependencies
  optional and test-only; preserve the existing semantic/refresh regression.
- Exit now additionally requires native press/release without dragging/selection,
  released-button Source persistence and repeated Source/Render on the same region.
- Implementation: claim existing native button/image gestures on press so ancestor
  selection cannot change the action before release. Retain GTK cancellation and
  keyboard handling. The expanded native window suite takes 58.78s, so increase its
  former 60s timeout to 90s to avoid turning pointer coverage into a timing failure.

- Objective: a normal Source click stays open during the formatted editing session,
  independent of caret selection, refresh, text edits, resizing and render callbacks.
- Modules: MarkerRichView presentation state and existing GTK rich-editing regression.
- Changes: distinguish explicit source choice from temporary automatic editing;
  transfer explicit choices using existing buffer marks when rebuilding regions.
  Render clears the choice for its region. Keep automatic stale-source/error reveal.
- Dependencies/migration: existing rich discovery/marks; no new persisted setting,
  source bytes, buffer, parser or render backend. Leaving formatted mode resets
  ephemeral region presentation as before.
- Risks: stale item/widget pointers during rebuild; marks shift during edits/undo;
  automatic buffer reveal must not pin every unrelated region open.
- Validation: first reproduce plain-click/focus-out/refresh failure; then test
  multiple source choices, resize, edits/undo, explicit Render, source fidelity and
  existing rich failure/refresh cases on native and modern stacks.
- Exit: regression and relevant suites pass; update tracking and reinstall handoff.

## Phase 7: Zorin 18 native compatibility follow-up

- Styling correction: replace the broad Zorin variant stylesheet with the installed
  Libadwaita base/color resources when its vendor loader is active. Restore one
  GTK 4.14-compatible inset divider at each sidebar edge; preserve overlay shadows
  and make simple shell icon actions explicitly flat. Validate native and modern
  light/dark wide/medium/narrow captures against the incumbent Folio surfaces.
- Objective: keep the existing redesign/scientific workflows while permitting
  native Meson installation on the user's Zorin 18.1/noble system.
- Modules: Meson dependency bounds, Debian metadata, rich-view scroll allocation,
  window minimums, preference theme integration, tests and README.
- Changes: audit actual shipped headers/API versions, reduce minimums only where
  compiled and exercised, prefer shared native APIs over versioned wrapper layers.
  Keep current WebKit minimum; noble updates already supply 2.52.6.
- Dependencies: user-approved compatibility work; native development packages are
  now installed, and system reinstall needs interactive sudo. The fallback remains available
  for environments without a system libspelling dependency.
- Risks: newer text-tag properties/GTK CSS and older spelling behavior may differ
  even when symbols compile; native GUI tests must detect warnings and regressions.
- Migration: no application identity, profile, notebook or Markdown format changes;
  preserve existing Flatpak packaging and modern-library builds.
- Validation: native configure/optimized build/tests on actual noble libraries,
  GTK warning review, rendered regions/source/undo, all view modes, responsive
  notebook UI, preferences/fonts/spelling, HTML/PDF and metadata/diff checks.
- Exit: native install/launch succeeds with maintained feature behavior. Record
  sudo/environment blockers and actual test limits; never equate compile with done.
- Second-pass review: retain widgets/models/cascade; avoid duplicate compatibility
  state, no profile migration or parser fork. Library floor changes need header and
  runtime evidence, and any essential newer API must get a narrow equivalent first.
- Runtime discoveries: GTK 4.14 needs explicit overlay allocation during scroll;
  connect object-bound adjustment signals after SourceView is parented. Adw 1.5
  needs an explicit breakpoint-window minimum. Zorin's vendor theme loader ignores
  application GtkSettings; an optional theme-path property identifies its loaded
  theme. The process-local provider now imports the installed Libadwaita base/color
  resources after the full matching-Zorin-variant import caused styling regressions.
  Do not write system theme settings or fork the UI. Regression cases cover actual
  spelling annotations, theme colors and existing rendered-region scrolling.
- Modern full-run discovery: pending buffer refreshes and document navigation can
  destroy a scientific/font readiness context. Explicit refresh consumes its
  debounce; cancel readiness calls on replacement, navigation start and disposal.
  Keep generation checks, actual failure reporting and the existing 30s deadline.
- Validation route: absent host development packages can be extracted into a
  temporary pkg-config prefix to compile against installed native runtime libraries.
  This establishes compatibility, but does not satisfy the installation exit.

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

## Documentation and fork merge follow-up

Rewrite README for Linux users installing/using this fork: overview, current
application captures, SSH recursive clone/local Flatpak how-to, notebook/editor/CSS/
export workflows, development requirements, known limits, creator/dependency credits
and license. The user has authorized preparing the outlined result as a PR for
their review; follow documentation-writer's audience/task/type separation.
Remove old screenshot/package advertising and donation links; retain upstream
attribution and existing licenses. Validate commands against current build files and
all local image/anchor links. Commit the already-tested inline-rich feature separately
from the README/tracking update. Fetch the fork, verify nested commits are reachable,
push the feature branch over SSH and create a pull request into fork master. The
latest instruction reserves approval and merge for the user; do not merge or push
directly to a default branch. If GitHub authentication is unavailable, retain the
exact PR title/body and a prefilled creation link; request an authenticated session
without claiming a PR was created.

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
