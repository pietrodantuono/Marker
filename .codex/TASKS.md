# Tasks

Legend: [ ] not started; [~] in progress; [x] completed; [!] blocked.

## Rendered font scale correction

- [x] Inspect current fonts, cascade, formatted scaling, preview zoom and user's
  active settings: saved preview zoom is 2.1, prose is Monospace 11, overrides off.
- [x] Reset preview zoom to 1.0 and verify persisted value; preserve typography,
  writing width, notebook styles and existing documents.
- [x] Validate the existing native typography workflow and update handoff/diff;
  document live preview Ctrl+0 reset and restart behavior without a code rewrite.

## Planning gate

- [x] Inspect build/window/project/session/navigation/editor/preview/outline/preferences/export/tests/diff/instructions.
- [x] Confirm Move means rail ordering and CSS preserves standard important semantics.
- [x] Populate the five user-selected .codex documents before production edits.
- [x] Review size/state/widgets/APIs/migration/lifecycle/responsive/export risks a second time.

## Find and Replace shortcut

- [x] Inspect current shortcuts/search bar/search context, editor/window ownership,
  tests, inherited diff and repository instructions; record the small shared-bar plan.
- [x] Add Ctrl+H, replacement controls/operations and document-menu discovery;
  preserve Ctrl+F, original-buffer undo and safe selected-match replacement.
- [x] Add focused GTK regression for replace/replace-all, undo, repeated opening,
  Find-only transition, no matches/deletion and Preview-only editing.
- [x] Build/test, reinstall the native user app and restore the explicit desktop
  launcher; update README and final tracking, inspect ownership and task diff.

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
- [x] Commit Charter, SciDown and Marker in dependency order: 35e95a1, 995fba6, 6c51fe11. Commit the final handoff notes separately, verify the clean trees and retain the built local Flatpak repository for installation. No push performed.

## Phase 6: inline rich elements

- [x] User-visible installation proof: rebuild/install native checkout in ~/.local,
  explicitly target it from the user desktop launcher, capture the installed app's
  scientific Render and Source views and retain commands/evidence in this workspace.
- [x] Run build-user's seven suites on isolated Xvfb; record the first live-desktop
  failure and review actual installed-app MathJax/Mermaid/Charter/gnuplot captures.
  Released Source click stays raw without selection; a second click resumes renders.
- [x] Complete Charter discovery/snapshots and usable local MathJax fallback;
  verify Mermaid and mathematical inline/display/fenced rendering.
- [x] Add one page-wide Source/Render control; remove per-region buttons and dead
  cursor-toggle/pinning APIs; preserve same-buffer editing, undo and view modes.
- [x] Source pauses/disposes rendering and monitoring; skip hidden duplicate
  preview conversion in Formatted mode; prepare Print explicitly and preserve exports.
- [x] Update real-pointer, scientific/error/refresh tests and README; run native/
  modern suites, inspect ownership/diff and update final .codex handoff.
- [x] Fix stale title/Save after Source edit/undo: use GtkTextBuffer's actual
  modified-changed signal and assert the visible document title after undo.
- [x] Supersede authenticated system-reinstall handoff with a native user install:
  build-user targets ~/.local; user desktop Exec and shell marker select it.
  Earlier /usr/local matched build-native at inspection. No system files or user
  settings were changed; see INSTALLATION-PROOF.md for commands and identity.

Earlier per-region completed tasks below are historical; page-wide control now
supersedes their buttons and explicit region-pinning state (see DECISIONS).

- [x] Reproduce and fix real-pointer Source/Render interaction: test native press
  and release without selection, trace ancestor gestures and layout, verify repeated
  toggles/persistence and run native/modern suites. Earlier signal-only tests missed
  the user-reported behavior; do not treat them as mouse validation.

- [x] Fix Source/Render choice persistence: reproduce no-selection refresh failure,
  preserve explicit per-region Source across caret/refresh/resize/edit/undo, and
  verify Render resets only its chosen region on native and modern builds.
- [x] Inspect current editor/preview/parser/export/tests/diff; record specification,
  architecture and second planning pass before production edits.
- [x] Add Unicode rich-region discovery/annotation using shared protected blocks;
  cover images, math, tables and closed scientific fences with parser regressions.
- [x] Implement native source-preserving overlays with one scientific render
  backend, bounded snapshots and stale-result rejection.
- [x] Integrate render default, click/Source/Render, cursor/selection reveal, edits,
  preferences/notebook CSS/data refresh and view switches.
- [x] Exercise actual renders, editing/undo/save fidelity, missing assets, invalid
  plots, Unicode, incomplete syntax, stale callbacks, scrolling and disposal.
- [x] Build/install SDK assets, run suites, inspect warnings/resources, capture
  light/dark/responsive rich editing states and review ownership/final diff.
- [x] Validate lazy renderer creation, 2x display scale, filtered formatted line
  numbers, actual image click, scrolling and unsaved notebook draft assets.
- [x] Build the optimized Flatpak and reinstall the user's existing local app;
  preserve running documents and stop the task-owned test display.
- [x] Reconcile tracking documents and final handoff with tested limitations.

## Documentation and fork merge follow-up

- [x] Add dedicated README uninstall instructions for Flatpak/Flathub and Meson;
  cover installation scope, original build/install log, desktop caches and data
  retention. Verified generated uninstall target/dry run, Flatpak flags and 18
  README shell examples without uninstalling the user's app.
- [x] Load documentation-writer; verify README/build files, current captures,
  original creator/license, repository remotes, fork master and nested pointers.
- [x] User authorized preparing the outlined documentation as a PR for review.
  Rewrite README with current installation/workflow/reference sections, screenshots
  and credits; remove donations and its unused image.
- [x] Validate 15 local links/anchors, 11 shell examples and current images; compare
  commands/dependencies with build files and review the diff. SDK rebuild passes.
- [x] Commit the tested inline-rich feature separately from documentation updates:
  144927f2; validated production/resource/test diff and the previous 11/11 suite run.
- [x] Verify fork/submodule reachability and push 144927f2 to the feature branch
  over SSH. SciDown/Charter/tinyexpr pointers are ancestors of their fork masters.
- [x] Commit/push rewritten README and PR description as ebecce3f. Verify GitHub
  renders the document and loads both current screenshots; SSH publication succeeds.
- [x] User created and merged PR #1 into fork master; verified merge commit
  40246940 and current master checkout on 2026-10-06.
- [x] Reconcile PROGRESS with documentation publication and user merge evidence.

## Local installation follow-up

- [x] Verify failed command, host tools, GNOME 50 runtime/SDK, existing user app
  and merged checkout. flatpak-builder is absent; the build never ran.
- [x] Record that the user replaced the Flatpak installation request with native
  Meson installation; the host flatpak-builder setup/rebuild is no longer requested.
- [x] Check host tools, development packages, APT candidates, pinned submodules and
  sudo access. Zorin 18.1 lacks development packages and supplies GTK/adwaita/
  sourceview below the current minimums; sudo requires interactive authentication.
- [x] Attempt native Meson setup in build-native; it fails at missing gtk4.
- [x] Make README native-first with version checks, safe chained install/update
  commands, explicit Zorin/Ubuntu dependency caveat and Flatpak as an alternative.
- [x] User selected adapting Marker for Zorin, conditional on preserved behavior;
  audit and validate compatibility rather than modify minimums blindly.
- [x] Build and test native compatibility against extracted noble development
  packages and installed runtime libraries; Phase 7 records the actual validation.
- [x] Prior styling/source-intent fixes are present in /usr/local from the user's
  installation. Native development packages/build-native are available. New
  page-wide feature installation is tracked separately under Phase 6 above.

## Phase 7: Zorin native compatibility

- [x] Correct reported styling regression: use Libadwaita widget styling instead of
  importing the full Zorin theme; restore sidebar dividers and flat icon controls.
  Build native/modern, exercise visual matrix and inspect light/dark/responsive
  captures; native 7/7 and modern 11/11 suites pass. Native flat classes retain
  hover/focus/checked feedback; Pages/Files and heading controls keep their fills.
- [x] Audit noble library headers/GIR and actual Marker API/property/CSS usage;
  identify minimum versions and specific equivalent changes before modifying code.
- [x] Adjust verified dependency bounds and matching Debian metadata; remove GTK
  4.16 CSS variables and use a GTK-compatible pinned libspelling fallback.
- [x] Build against GTK 4.14/adwaita 1.5/sourceview 5.12/spelling 0.2 with extracted
  development packages. Marker compiles cleanly; GCC 13 emits pre-existing tinyexpr
  partial-allocation warnings and itstool Python emits escape-sequence notices.
- [x] Fix older breakpoint-window minimum requirements and overlay scroll allocation;
  notebook and targeted rich editing/undo/save/scroll tests pass.
- [x] Run all seven native suites, including GTK/scientific/editing/HTML/PDF export;
  inspect light/dark/wide/medium/narrow and rich-rendered application screenshots.
- [x] Exercise actual spell annotations/toggle/fidelity with a dictionary on noble
  and the modern SDK; settle loaded source/cursor before enabling the checker.
- [x] Fix Zorin's forced-color-scheme mismatch through its optional theme-path
  property and app-local Libadwaita base/color resources; verify explicit and system-follow
  schemes without writing system settings.
- [x] Verify the existing modern-library build remains supported: all eleven suites
  pass after the reload correction, including the newer libspelling fallback tests.
- [x] Fix the discovered reload race: cancel scientific/font readiness calls when
  their WebKit document is superseded; preserve real rendering errors and bounded
  waiting. Explicit editor refresh must consume its pending buffer debounce.
- [x] Update README dependency/install commands and durable decisions/validation;
  schema/resources/desktop/AppStream, 15 shell examples and final diff validate.
- [x] Native compatibility/styling and the subsequent page-wide feature are
  installed; see Phase 6's user-native installation proof.

## Fork identity and licensing clarification (2026-10-06)

- [x] Inspect application/Flatpak/schema/state identity and explain that a distinct
  fork ID (prefer io.github.pietrodantuono.marker) needs coordinated metadata and
  existing-profile migration; no rename performed.
- [x] Verify gnuplot worker/command-line/SVG integration, bundled licenses and
  provenance; correct README rendering credits and distinguish component licenses.
- [x] Compare bundled runtime hashes and check README links/diff. Preserve inherited
  installation tracking edits; no commit/push requested for this correction.

## Previous explicit validation deferrals

- [ ] **Deferred: desktop integration acceptance.** Select/accept native Open/New/Locate/Save As chooser paths, Reveal in Files and a physical print dialog on the user's desktop. Xvfb/SDK has no desktop portal, file manager or printer. Impact: the native integration layer is unverified; captured targets, file creation/save/removal and PDF backend paths are exercised. Next: use a disposable notebook in a GNOME desktop session and exercise those actions, including draft Save As cancellation during removal.
- [ ] **Deferred: Debian package build/installation.** The host lacks the declared native development versions. Impact: Debian metadata validates, but its package artifact is not claimed tested. The local optimized Flatpak build/install/launch/export are now exercised. Next: build the Debian artifact in CI with the declared versions before publishing.
- [ ] **Deferred: broader renderer conformance.** The presentation lexer supports the agreed heading/code cases, rather than all CommonMark nesting. Impact: unusual nested list/quote or inline markup may retain source punctuation; source bytes remain authoritative. Next: add representative real documents and regressions before expanding parsing, without introducing another heading model.
- [ ] **Deferred: nested rich elements and inline interactivity.** Rich snapshots cover complete top-level regions; complex list/quote content keeps editable source. Links and chart interactions remain in regular preview. Impact: those regions do not behave like interactive web content inside the editor. Next: collect real nested documents and add source-fidelity regressions before extending discovery; assess interactive widgets only against an actual user workflow.
