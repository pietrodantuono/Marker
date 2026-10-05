# Tasks

Legend: [ ] not started; [~] in progress; [x] completed; [!] blocked.

## Planning gate

- [x] Inspect build/window/project/session/navigation/editor/preview/outline/preferences/export/tests/diff/instructions.
- [x] Confirm Move means rail ordering and CSS preserves standard important semantics.
- [x] Populate the five user-selected .codex documents before production edits.
- [x] Review size/state/widgets/APIs/migration/lifecycle/responsive/export risks a second time.

## Phase 0

- [x] Repair invalid Flatpak JSON and stale GTK3 package metadata.
- [x] Record SDK build instructions; remove obsolete mesontest requirement if safe.
- [x] Inventory/remove verified dead GTK3 UI/helpers/sketcher/extensions; update po/POTFILES and resource/package references.
- [x] Replace active string/path helpers with GLib; remove their unused exports.
- [x] Fix global stylesheet path resolution and validate actual theme loading.
- [x] Build/test/warnings/metadata/diff validation and record phase exit.

## Phase 1

- [x] Extend MarkerProject appearance with six modes, color normalization, text/contrast derivation and notify.
- [x] Extend session appearance metadata and legacy palette/icon migration; add regression cases.
- [x] Implement reusable book widget and register Folio symbol assets with attribution.
- [x] Implement adaptive live-preview notebook editor with draft Apply/Cancel.
- [x] Add row-targeted context gestures/menu; reordering and reveal; dirty-safe Remove; preserve Locate recovery.
- [x] Validate modes/colors/persistence/lifetimes/actions and record phase exit (environment limits in PROGRESS).

## Phase 2

- [x] Extract notebook rail and page sidebar from window ownership of UI construction (rail extracted in Phase 1).
- [x] Add cancellable recursive notebook discovery/search with stale-result protection.
- [x] Implement Pages/Files, deduplicated open/disk rows, dirty/close state, relative subtitles and CSS filtering.
- [x] Associate drafts/saved editors with notebooks and preserve standalone Open Files navigation.
- [x] Distribute headers/menu controls; dirty Save remains discoverable (toolbar refinement in Phase 3).
- [x] Add native responsive splits with coordinated overlays and 1200x800 fresh defaults.
- [x] Persist per-window desired sidebar/outline visibility and mode without transient state writes.
- [x] Validate nested/loose/draft navigation, widths/restore/focus/ownership and record phase exit.

## Phase 3

- [x] Consolidate pure Markdown structure parser for headings/spans/titles; exclude code/frontmatter, support Setext.
- [x] Cache text parse separately from caret presentation; preserve source/undo and UTF-8 offsets.
- [x] Add reusable H1-H6 gutter and independent line numbers; shared heading chooser/edit operation.
- [x] Set centered writing defaults and comfortable padding/light/dark typography (640px/Monospace 11 after the reference follow-up).
- [x] Replace popover with right hierarchical debounced outline and generation-safe editor/preview jumps.
- [x] Compact formatting toolbar and adapt relevance in all modes.
- [x] Validate parser/editor/navigation/gutters/view modes and record phase exit.

## Phase 4

- [x] Add Edit Notebook Styles root creation/open workflow; existing save handling and source-only CSS mode.
- [x] Compose shared style cascade with native stylesheet origins and no forced app important declarations.
- [x] Restore global theme chooser/path support and preserve settings.
- [x] Add root stylesheet monitoring/debounce/refresh with clean disposal and missing-file recovery.
- [x] Reuse style context for preview/HTML/gnuplot staging/PDF/print; wait for correct render readiness.
- [x] Retain originating export document across async dialogs.
- [x] Validate conflicts/relative assets/live edits/nested files/HTML/PDF and record phase exit.

## Phase 5

- [x] Remove verified superseded header/outline/widgets/APIs and complete cleanup reference review.
- [x] Replace/register relevant GTK4 regression checks; inspect compiler/runtime warnings.
- [x] Inspect SciDown/Charter warnings; fix signatures/returns/overlapping appends and exercise actual SVG composition.
- [x] Exercise available acceptance matrix in isolated GTK/WebKit profiles: defaults/restore, appearance, navigation, editor, CSS, HTML/PDF (desktop checks deferred below).
- [x] Capture and review 16 light/dark wide/medium/narrow application-only screenshots; confirm window controls and toolbar/list fixes.
- [x] Fix review findings: outline window controls, missing toolbar icon, structural list punctuation, source guide confined to source modes.
- [x] Consolidate stylesheet creation; expose sidebar refresh/sort/styles through its title menu and remove duplicate notebook actions.
- [x] Preserve sorting through nested Files and sorted Pages; refresh active search on page title changes.
- [x] Include scientific metadata styles in the shared cascade; validate four font roles/KaTeX/Mermaid/generic CSS fonts and CSS Save As mode.
- [x] Review teardown/async targets; dismiss a page context menu before opening can replace its row, and add a real menu-open regression.
- [x] Full build/tests/metadata/ownership/diff review and reconcile all five documents.
- [x] Add Final handoff with completed phase exits and explicit validation/conformance deferrals.

## Follow-up: Folio surfaces and default typography

- [x] Inspect actual Folio CSS/measure, current native split styling and effective preview defaults; refine the existing plan before edits.
- [x] Correct Pages/Files/Outline backgrounds and single column dividers; keep overlay shadows and light/dark contrast native.
- [x] Narrow fresh writing/preview defaults to 640px and Monospace 11 without overwriting explicit preferences.
- [x] Exercise actual rendered defaults and custom width/font/theme precedence; regenerate and inspect all 16 light/dark responsive screenshots.
- [x] Build/install, run suites, validate metadata/resources/diff and reconcile final handoff.

## Installation and commit handoff

- [x] Inspect current dependencies, README, manifest, installed runtimes, dirty files and nested repositories before committing.
- [x] Document a runnable local Flatpak installation, updates/removal, native requirements and the SDK-only development workflow.
- [x] Exclude build/cache directories from Git and local Flatpak sources; validate the optimized package build, isolated installation, 1200x800 launch and CSS/gnuplot HTML/PDF export.
- [x] Fix the verified Flatpak library-directory mismatch: install libspelling in /app/lib so Marker discovers and loads it without a sandbox network fallback.
- [x] Validate optimized-build warning fixes: disabled fallback for invalid script modes, correctly typed Charter axis constants and checked include-file I/O. Absolute/relative/long-CWD, empty/missing/non-file includes and invalid modes pass; full build/tests pass.
- [x] Correct Xvfb selection to set host DISPLAY before flatpak run (Flatpak replaces an --env=DISPLAY override); all 11 suites pass on the verified isolated display.
- [x] Fix UI checks that equate requested width with client allocation under X11 decorations; verify the fresh size before mapping and keep responsive assertions/captures against actual widgets.
- [x] Validate metadata and relevant tests; inspect staged diffs including renderer changes. Build/tests/metadata pass; staged review/checks pass with generated artifacts excluded.
- [~] Commit Charter, SciDown and the Marker feature branch in dependency order; verify the final tree and record the handoff. Renderer commits are complete; parent staging/commit remains.

## Explicit validation deferrals

- [ ] **Deferred: desktop integration acceptance.** Select/accept native Open/New/Locate/Save As chooser paths, Reveal in Files and a physical print dialog on the user's desktop. Xvfb/SDK has no desktop portal, file manager or printer. Impact: the native integration layer is unverified; captured targets, file creation/save/removal and PDF backend paths are exercised. Next: use a disposable notebook in a GNOME desktop session and exercise those actions, including draft Save As cancellation during removal.
- [ ] **Deferred: Debian package build/installation.** The host lacks the declared native development versions. Impact: Debian metadata validates, but its package artifact is not claimed tested. The local optimized Flatpak build/install/launch/export are now exercised. Next: build the Debian artifact in CI with the declared versions before publishing.
- [ ] **Deferred: broader renderer conformance.** The presentation lexer supports the agreed heading/code cases, rather than all CommonMark nesting. Impact: unusual nested list/quote or inline markup may retain source punctuation; source bytes remain authoritative. Next: add representative real documents and regressions before expanding parsing, without introducing another heading model.
