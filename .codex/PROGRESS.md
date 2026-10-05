# Current status

Current phase: Installation and commit handoff.
Current task: Commit the reviewed parent feature and record the final handoff.
Last completed task: Committed Charter and SciDown, including the updated nested pointer, on feature/folio-renderer.
Next task: Record the parent ID and verify clean trees after committing.

# Completed

- Planning: inspected the current build, architecture, project/session persistence,
  navigation, editor/preview, preferences, exports, tests, contributor instructions
  and inherited diff. Populated the five initially empty user-selected .codex files
  and performed the second planning pass before production changes.
- Phase 0: repaired Flatpak JSON and GTK4 Debian metadata/configure; documented SDK
  builds. Removed verified unused GTK3 helpers, sketcher, UI resources and WebKit
  extensions after caller/dynamic/resource/packaging review. GLib replaces the
  active path/string helper. Global absolute CSS paths load correctly.
- Phase 1: one notifying six-mode notebook appearance model, opaque RGB colors and
  contrast-aware foregrounds, legacy migration, transactional appearance writes,
  reusable Folio book widget and attributed symbol assets, live draft editor,
  row-targeted context actions, safe ordering/removal/Locate. Open windows share
  notebook objects; a new window does not overwrite live appearance from disk.
- Phase 2: extracted rail/page sidebar, editor-owned notebook context, deduplicated
  recursive Pages and lazy nested Files, mode-aware recursive search, dirty/close
  state, regional headers, three native responsive splits, 1200x800 defaults and
  per-window desired navigation state. New Notebook creates without overwriting;
  Remove preserves folders and handles dirty saved pages/drafts before closing.
- Phase 3: shared Unicode ATX/Setext heading/block discovery and cached presentation,
  independent line numbers/H1-H6 gutter, one undoable heading operation, live right
  outline with stable marks, compact toolbar and centered writing measure (640px
  after the final reference correction).
  Cursor anchors stay outside code/frontmatter; all five view modes remain.
- Phase 4: .marker.css create/edit/source-only workflow and replacement-safe root
  monitor. One author cascade supplies preview, HTML, scientific staging, print/PDF;
  native stylesheet/base URIs retain relative assets. Export retains its originating
  editor. Readiness includes fonts/math/diagrams/gnuplot, rejects stale renders and
  bounds waiting. Global theme chooser follows installed/custom path preferences.
- Phase 5: removed superseded window/editor/source/preferences APIs, old completion
  signal/app-menu stub, unregistered GTK3 theme test and unused shell selectors.
  Corrected SciDown signature/return warnings and overlapping Charter appends.
  Layered metadata styles; fixed generic CSS fonts, CSS Save As language transitions,
  sidebar sorting/title search, outline window controls, toolbar icon and visible
  structural list punctuation. Source column guides remain in source modes.
  Page menus dismiss before opening can destroy their row; teardown disconnects
  shared-model/editor handlers and async Locate checks for a closed window.
- Captured and reviewed 16 application-only screenshots and the bounded confirmation
  round. Synthetic fixtures/profile paths were isolated; no user notebooks changed.
- Follow-up implementation: transparent sidebar children reveal native column
  dividers; rail uses sidebar colors, Pages/Files and Outline use window colors,
  document chrome uses view colors. Fresh width/font are 640px/Monospace 11 and
  previews use the shared base. Explicit settings and optional themes are retained.
- Installation guide: README now covers local Flatpak installation/update/removal,
  exact native dependency versions and a separate SDK development prefix. Generated
  build/cache directories are ignored and excluded from local package sources.
- Optimized-build corrections: invalid script modes disable both loader and init;
  Charter compares axis values with LINEAR. SciDown uses native relative path
  resolution instead of fixed CWD buffers, checks file reads/stat and frees include
  buffers, including empty includes. Added actual include/mode regression cases.
- Renderer commits are preserved on feature/folio-renderer in each nested repository,
  with Charter committed before the SciDown pointer. No push was performed.
- Final staged parent review passes: 130 source/resource/metadata/test/feature-doc
  files, synthetic application captures and the updated SciDown pointer. Generated
  build/cache/package artifacts are excluded. Root and nested diff checks pass.

# In progress

- The user requests committing the current feature and installation instructions.
  Documentation/package validation is in progress before committing the nested
  renderer dependencies and parent. Other validation deferrals remain in TASKS.

# Discovered issues

- Actual Flatpak build found libspelling in /app/lib64 while the next module's
  dependency lookup uses /app/lib. Set the manifest's Meson libdir consistently
  instead of falling back to a network fetch inside the build sandbox. The
  temporary unprivileged builder also needs its ELF/debug helper dependencies.
- Flatpak replaces --env=DISPLAY with the host display when preparing the socket.
  Set DISPLAY=:92 before flatpak run and verify the window with xwininfo. Corrected
  PLAN's command and repeated all suites on that isolated display. Earlier captures
  are application widget snapshots and contain only synthetic fixtures.
- X11 without a window manager reports a 1190px client allocation for a 1200px
  toplevel because of GTK decorations. The UI test incorrectly equated allocation
  with requested size. Check the fresh 1200x800 default before mapping; retain the
  actual responsive state assertions and widget-bound snapshots after mapping.
- Optimized packaging exposed uninitialized script values for invalid renderer
  modes, cross-enum Charter comparisons and unchecked SciDown file I/O. Corrections
  and regression coverage pass. The optimized package has no Marker C warnings;
  SDK itstool emits external Python escape-sequence notices.

- Confirmed baseline faults: manifest literal newlines; stale GTK3 package/configure
  metadata; unregistered UI extraction entries; double-prefixed absolute CSS paths;
  unsafe notebook removal/async targets; incomplete nested search; duplicate heading
  scans; disabled formatted line numbers; preview/export styling divergence.
- Integration checks caught and fixed a consumed GtkColorDialog reference, recycled
  popover parenting, GTK allocation invalidation, stale scientific callbacks, generic
  font quoting, and metadata CSS escaping the author cascade.
- Old `build` is GTK3. Host GTK 4.14.5 development headers are below the branch's
  4.18 minimum; other native development packages remain incomplete. GNOME 50 SDK
  builds/install/tests succeed in `build-gtk4` with its isolated install prefix.
- Existing AppStream metadata has four informational findings; validation succeeds.
- Meson logs include inherited environment values. Omit that line from diagnostic
  scans or shared artifacts; the final text log has it redacted.
- Opaque sidebar children obscure the native split dividers. Applying one headerbar
  background to every column loses Folio's rail/sidebar/document surface hierarchy.
  Swiss is enabled by default and overrides the preview's base font.

# Validation performed

- Installation handoff: GNOME 50 local Flatpak builds/exports successfully with
  /app/libspelling and generated source exclusions. Installed into a task-owned
  FLATPAK_USER_DIR; actual platform app opens a 1200x800 window on :92. Exported
  notebook CSS and a linked CSV gnuplot chart to HTML (SVG, no gnuplot error) and
  a real one-page PDF. Settings/state/config/cache use disposable directories.
  The host's older Flatpak cannot supply WebKit's sandbox-spawn capability; WebKit
  warns and uses its own fallback. No production sandbox override was added.
- All 11 SDK suites pass again with include/invalid-mode regressions. Metadata/
  resource/schema/desktop/manifest/configure validation and nested diff checks pass.
  The corrected Xvfb invocation passes in 33s; bus connection notices come from the
  isolated SDK environment, with no GTK criticals or failing tests. Fresh defaults
  are checked before mapping to accommodate X11 decorations.
- Final SDK build and all 11 Meson suites pass; real window workflows finish in
  approximately 34s. Compiler/runtime logs contain no compiler warnings or GTK criticals.
  Earlier phase exits also passed their registered suites and metadata/diff checks.
- Pure/model tests: Unicode icon modes/colors/contrast, draft independence, old/new
  session appearance/navigation, malformed keys, identity/containment, stylesheet
  creation without overwriting/non-regular-file errors, recursive assets/search,
  protected headings/code/frontmatter/inline code/cursor anchors and source fidelity.
  An ordered list followed by a rule is excluded from Setext heading discovery.
- Real GTK: Apply/Cancel/live appearance, context target after selection changes,
  teardown notifications, fresh 1200px default, wide/medium/narrow overlays, saved
  visibility intent, focus/restore, dirty saved/draft removal Cancel/Save/Discard,
  folder preservation, Pages/Files/search/sort, page context opening during model
  replacement, all five modes, heading chooser/undo/gutter/live outline/navigation,
  CSS Save As language and action state, shared appearance across windows.
- Real WebKit: normal notebook rules beat global/dark/font/metadata rules; standard
  important reversal preserved; root-relative CSS assets; atomic replacement,
  deletion/recreation; four font overrides and generic families; KaTeX/Mermaid
  completion; light/dark/follow-system setting behavior; one cursor anchor.
- Export: standalone exported HTML loaded in WebKit with effective notebook styles;
  real PDF output; linked CSV-to-gnuplot SVG; superseded worker renders; staged HTML
  and scientific PDF. Charter SVG appends preserve prior content.
- Screenshots: .codex/screenshots contains light/dark 1440x800, 900x800 and 500x800
  states, Pages/Outline overlays, dark preview and dark preferences. Wide controls,
  notebook geometry, typography, list punctuation and navigation overlays inspected.
- Reference correction: rebuilt/installed and reran all 11 suites; inspected all
  16 refreshed captures. Actual GTK font and centered measure plus WebKit body/
  paragraph/max-width checks pass for 640px monospace defaults; explicit Serif/800px
  and selected Swiss theme still apply. Native column dividers remain continuous,
  with matching soft sidebar/header surfaces and native responsive overlay shadows.
- Strict schemas, resource inputs/compilation, Flatpak JSON, desktop/AppStream,
  configure shell and gnuplot JavaScript syntax, root and nested-repository diffs
  validate. SDK install uses `build-gtk4/install`; no host installation performed.
- WebKit sandbox override is used only in isolated nested-SDK tests because its
  flatpak-spawn portal is unavailable. Production sandbox configuration is unchanged.
- Task-only diff uses `/tmp/marker-folio-baseline-eW1R7A/before.tar`, including its
  symlinks/submodule contents; inherited work is preserved and excluded from counts.

# Remaining risks

- Desktop native chooser/Files/physical-printer integration and a Debian package
  artifact need their normal environments; see the explicit deferrals below.
- Presentation parsing is intentionally narrower than full CommonMark. Unusual
  nesting may retain punctuation; Markdown remains the authoritative source.
- Exported HTML retains local stylesheet/asset dependencies; portability bundling
  and CSS parity for non-web export formats are outside this feature's scope.

# Final handoff

## Implemented

All six implementation phases are complete. FEATURE/PLAN/TASKS/DECISIONS describe
this checkout. Notebook rail/editor, Pages/Files, responsive regional shell,
formatted writing/gutters, live right outline and shared notebook CSS/export are
implemented; obsolete verified code is removed. The final Folio reference correction
uses soft sidebar surfaces, native column dividers and 640px/Monospace 11 defaults
without resetting explicit choices. Installation instructions are in README;
the local optimized Flatpak package now builds, installs, launches and exports.
Only the requested commit handoff is currently pending.

## Important architectural decisions

Keep MarkerProject and additive GKeyFile metadata; share existing objects across
windows. Reuse source buffers/workspace items rather than parallel page models.
Use native split views with explicit visibility intent, one structure cache, native
heading gutter and text-mark outline targets. Move means ordering, never folder
movement/deletion. Native author layers preserve URL origins and normal notebook
precedence while respecting standard importance. Scientific metadata styles remain
supported. Export/render generations retain their actual originating documents.
Native pane styling owns dividers; transparent region surfaces avoid covering them.
Fresh preview uses the shared base, with optional global themes still selectable.

## Validation performed

Warning-free GNOME 50 SDK build, isolated install, 11/11 suites, strict metadata/
resource/script checks, ownership/reference/task-diff review and 16 isolated GTK
screenshots. Actual HTML styles and PDF/scientific export paths are exercised.
Commands and environment details are in PLAN; the acceptance evidence is above.
The reference correction reran the full suite and refreshed every capture; both
default typography/measure and persisted custom preference precedence are verified.
The installation handoff also tests the real optimized Flatpak artifact, isolated
user installation and 1200x800 launch, linked CSV-to-SVG HTML and PDF with notebook
CSS. Includes/invalid-mode regressions pass; all suites pass again on verified Xvfb.

## Known limitations

The host GTK headers alone cannot build this branch; use the SDK or all declared
native versions. Formatted Markdown is source presentation rather than a complete
rich-text renderer. Local assets must accompany exported HTML when moved.
AppStream's four pre-existing informational notices remain.
This host's Flatpak warns that sandbox-spawn is unavailable; WebKit uses its own
fallback. Production settings are unchanged; check that capability after upgrading
Flatpak/portal integration on a normal desktop.

## Deferred work

- Desktop chooser acceptance (Open/New/Locate/Save As/Export), draft Save As cancel
  during removal, Reveal in Files and physical printer dialog: no desktop portal,
  file manager or printer in Xvfb/SDK. Impact is limited to unexercised native
  integration; captured targets and underlying file/save/PDF logic are exercised.
- Debian package build/installation: its native dependency versions are unavailable
  here. Metadata passes; the local optimized Flatpak artifact is now exercised.
- Broader nested Markdown conformance: preserve source and expand only with real
  regression documents, without introducing a second heading parser.

## Recommended follow-up

Use a disposable notebook in a GNOME desktop session for the deferred integration
checks, then build Debian release artifacts in CI. Publish the local renderer
commits to reachable remotes before sharing the parent branch with another machine;
the parent alone cannot supply new submodule objects. Reuse these five documents for any follow-up;
do not rebuild another parallel plan or restore inherited files over this work.
