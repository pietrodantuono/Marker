# Current status

Current phase: Rendered font scale correction complete.
Current task: Complete; saved preview zoom reset from 210% to 100%.
Last completed task: Settings readback and native typography workflow pass; README and task diff reviewed.
Next task: Open Marker with normal zoom; focused HTML preview Ctrl+0 resets future zoom changes.

# Completed

- Font scale follow-up (2026-10-06): actual user preview-zoom-level was 2.1 with
  Monospace 11 prose, 640px measure and disabled theme/font overrides. Reset that
  one preference to its existing 1.0 default; font, hierarchy, notebook styles and
  source were preserved. Native /window/default-typography passes (2.17s); installed
  executable matches build-user. No production rebuild/change is required. README
  explains focused HTML preview Ctrl+0 and zoom adjustments. No Marker process was
  running at inspection; existing open panes require Ctrl+0 or a restart to update.
- Ctrl+H follow-up (2026-10-06): added editor-owned replacement row and window
  shortcut/menu action, Replace/Replace All with native match/undo support, empty
  replacement deletion and Preview-only transition to editable source. Ctrl+F
  returns to Find-only; search navigation advances past the selected match.
  Focused real-keyboard Ctrl+H regression and all seven native suites pass (17
  window workflows, 89.24s; no skips). README/feature/plan/decision/tasks agree;
  build-user is reinstalled and the explicit user desktop launcher restored.
- Installation proof (2026-10-06): optimized build-user and full ~/.local install
  succeed; shell marker/user desktop Exec target the installed executable and
  build/install SHA-256 match. Seven isolated native suites pass (86.92s window
  workflows). Three direct installed-app captures show MathJax/Mermaid/Charter,
  Charter/gnuplot and whole-page Source. Actual press/release, scroll without
  selection and second-click resume are visually checked. Synthetic demo and
  INSTALLATION-PROOF document commands, identity, saved Dual Pane explanation and
  evidence. No production source, user notebooks or existing settings changed.
- Page-wide Source/Render (2026-10-06): one editor-owned toolbar choice replaces
  per-region buttons/pinning/cursor-toggle API. Source restores raw Markdown and
  cancels/disposes inline rendering; hidden full preview navigates away, disables
  JavaScript and releases data monitors. Formatted Render avoids duplicate full
  preview evaluation. Print prepares the full preview on demand and waits for
  WebKit job completion before pausing again; export retains its existing shared path.
- Charter uses existing scientific-region discovery/snapshots. Local MathJax has
  an unmodified pinned 3.2.2 TeX/SVG fallback with Apache license/provenance when
  system MathJax 2 is absent. Targeted native rich-editing passes with eight actual
  regions (image, table, gnuplot, Mermaid, Charter, inline/display/fenced math), both
  math backends, Source editing/undo/view switches, cancellation and unchanged bytes.
  README usage describes whole-page pause/resume and explicit print/export.
- Final validation: native 7/7 (60.92s) and modern 11/11 (57.32s) pass, with all
  16 Marker UI/renderer workflows run. Native has no skips; modern's upstream
  libspelling skips one optional words-database case because its database is absent.
  Source readiness cancellation/stale commits
  and disabled scientific preferences pass. The modified-changed correction updates
  title/Save after undo; the visible title assertion and refreshed dark capture pass.
  Four tracked screenshots, usage guide, task/decision/plan and final handoff agree.

- Pointer correction (2026-10-06): XTest reproduced Source changing to Render on
  press, then back on release, despite the earlier source-intent fix. Native
  button/image gestures now claim on press, stopping ancestor caret selection
  before native release activation. Repeated clicks, refresh, drag cancellation,
  image activation, stable caret and unchanged bytes/dirty state pass on both stacks.
  Optional pointer dependencies are test-only; production remains GTK-based.

- Source/Render follow-up (2026-10-06): explicit Source choices survive caret
  movement, refresh, resize and edits/undo until Render for the chosen region.
  One per-region display mode replaces the boolean; existing live marks transfer
  choices across rebuilds. Automatic stale/error reveals do not pin unrelated
  regions. Deleted regions/mode switches reset ephemeral choices. Native 7/7 and
  modern 11/11 suites pass; tracking, lifetime and task-specific diff reviewed.

- Uninstall documentation: dedicated README section and navigation link cover
  user/system Flatpak, optional profile deletion, notebook retention, Meson's
  original build/install log, shared desktop caches and custom/user-owned prefixes.
  Verified local Flatpak flags, Meson's uninstall implementation/target, dry-run
  command and installed cache paths. All 18 README shell examples pass bash -n;
  final documentation diff/anchor checks pass. No app or user data was removed.

- Styling correction (2026-10-06): removed the full matching-Zorin theme import;
  the existing provider now uses installed Libadwaita base/color resources. Restored
  one @borders inset divider per sidebar with correct start/end and RTL orientation.
  Rail/header/formatting icon controls explicitly use GTK flat styling. Selected
  toggles, Pages/Files, heading chooser, native focus/hover and overlay shadows remain.
  Native packages and build-native are now available from the user's installation.

- Native installation checks (2026-10-06): current host is Zorin OS 18.1/noble.
  Build tools and pinned submodules are present; GTK/adwaita/sourceview/WebKit/
  spelling development packages are absent. APT candidates for GTK/adwaita/
  sourceview are 4.14.5/1.5.0/5.12.0, below 4.18/1.7/5.14. WebKit candidate
  2.52.6 meets the requirement; spelling 0.2.0 needs the pinned fallback.
  Native Meson setup fails at missing gtk4. README now leads with native Meson,
  version checks, conditional install/update commands and an explicit host caveat.

- Licensing/identity clarification (2026-10-06): gnuplot is explicitly credited
  as scientific rendering; README distinguishes GPLv3 application code from the
  separately executed gnuplot WASM program and its own redistribution terms.
  Worker code exchanges scripts/data and SVG through messages and calls gnuplot's
  command-line entry point. Bundled provenance records unmodified upstream source;
  original/runtime licenses are retained. Recommended io.github.pietrodantuono.marker
  per Flatpak's GitHub naming conventions, with coordinated metadata/schema/profile
  migration; no identity changes performed.

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
- Marker commit 6c51fe11 is on feature/workspace-theme-preferences. SciDown 995fba6
  and Charter 35e95a1 are on their feature/folio-renderer branches. Final tracking
  notes are a separate documentation commit; no push performed. The built ignored
  build-flatpak-repo is retained for immediate user installation.
- Phase 6: rendered image/math/table/gnuplot/Mermaid regions with click/Source/Render,
  original-buffer edit/undo/save, lazy shared rendering, bounded HiDPI textures,
  stale-result cancellation and editable failure/retry surfaces. Formatted numbers
  omit compressed ranges. Shared math fences and notebook-relative draft assets work
  in normal previews too. Three synthetic captures reviewed; final optimized package
  rebuilt and reinstalled into the user's existing app. Committed as 144927f2 and
  pushed to origin/feature/workspace-theme-preferences over SSH.
- README/PR preparation: ebecce3f rewrites the fork overview, SSH recursive clone,
  installation/update/uninstall, notebook/editor/styles/export usage, development,
  limits and creator/dependency credits. Current screenshots replace GTK3 captures;
  donation section and its unreferenced image are removed. Exact review description
  is in PULL_REQUEST.md. SSH push succeeds. User created and merged PR #1;
  merge commit 40246940 is checked out on master (2026-10-06).

# In progress

- None for this installation/proof request. Prior product deferrals remain in TASKS.

# Discovered issues

- First-launch profile initialization overwrites a preseeded math backend with
  KaTeX. Set MathJax after initialization, verify its value after launch and refresh
  the installed Render proof; do not label a KaTeX capture as MathJax evidence.
- The real saved view is Dual Pane. Page-wide Source/Render is visible only in
  Formatted Markdown (Ctrl+5); the user's persisted view was not changed.
- A first suite run on the live desktop passed 6/7 but failed window workflows
  with gdk_surface_get_device_position after two cases. Rerunning on task-owned
  Xvfb with cairo/local VFS passed all 7 (window workflows 86.92s, no skips).
  Live-desktop captures also became obscured by focus changes; final proof uses
  the installed executable on an isolated display, independently of the tests.
- The synthetic demo originally used set terminal, which the gnuplot adapter
  explicitly rejects because it owns terminal/output. Removed that demo command;
  no renderer behavior was changed.
- Installation proof follow-up: /usr/local/bin/marker now matches build-native
  exactly and the bundled MathJax assets exist; earlier reinstall deferral is stale.
  No Flatpak Marker or running Marker process was found. sudo -n still requires
  authentication. User requests installation and proof, so use a native user prefix
  and explicit user desktop Exec; capture the installed executable, not a test binary.

- Screenshot review: undo restores the clean buffer but the title/Save indicator
  stays dirty. GtkTextBuffer has a modified-changed signal, not a modified property;
  the old notify::modified connection never updated the UI. Use the actual signal
  and verify the visible title after Source editing/undo.

- Full-suite corrections: typography/CSS/preference/export DOM tests depended on
  a hidden preview in Formatted mode; they now explicitly select Preview. The
  stylesheet test's fixed 350ms wait was shorter than two 180ms debounces; it now
  waits for render completion. Source during explicit full-preview readiness must
  wake waiters with a paused error, rather than leave their nested loop blocked.
  The cancellation regression also awaits asynchronous blank navigation instead
  of assuming WebKit commits it within a fixed settle interval. Modern WebKit can
  commit a first provisional load after pause's blank request; paused load handling
  replaces that stale commit with blank again while JavaScript stays disabled.

- Charter fences are not classified as inline scientific regions. Local MathJax
  references a missing system asset on this host. Editor refresh renders both
  hidden full preview and inline preview on every edit; hiding a WebView alone
  does not cancel its workers. These are dependencies of page-wide Source pause.

- Native pointer regression failed before this correction despite the earlier fix:
  the Source button is correctly hit, but ancestor caret handling reveals source
  on press. The button's release activation then treats that as a Render request.
  Direct clicked-signal tests bypass this dispatch path and falsely appeared fixed.
  Optional gtk4-x11/X11/XTest dependencies now apply only to the UI test executable.

- Source/Render intent was inferred from caret position during every rich-region
  rebuild, so Source reverted on refresh/resize without a selection. Explicit
  choices now transfer from live marks; collapsed/deleted ranges are discarded.

- Phase 7: older GTK needs explicit internal overlay allocation on scroll, older
  Adw needs a breakpoint-window minimum, and Zorin's fixed vendor Light/Dark theme
  can disagree with the effective Adw scheme. Public allocation calls and an
  optional-property theme adapter resolve these without duplicate widgets/state.
  The system GTK theme remains ZorinGrey-Dark; only Marker's CSS provider changes.

- GTK's overlay child lives in its internal text-window container; the public
  TextView.remove handles anchored/direct children. Use one zero-measure owning
  layer, normal child disposal, and retain its empty shell until SourceView dies.
- requestAnimationFrame is suspended for offscreen WebKit. Use readiness promises,
  image completion and a queued task before measuring instead of waiting for frames.
- Native line numbers overlap compressed source lines. A formatted-only number
  gutter omits rendered source ranges and restores numbers on Source; the ordinary
  source-mode gutter remains unchanged. Three initial captures exposed this issue.

- The host/SDK lack Xvfb. An unprivileged task-only executable supplied isolated
  display :93 for actual GTK/WebKit checks; the server was stopped after validation.

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

- Page-wide follow-up: native 7/7 (60.92s, no skips) and modern 11/11 (57.32s) pass
  after the final modified-signal change. Tests exercise actual scientific output,
  both math backends, disabled preferences, Source readiness cancellation/unload,
  real pointer dispatch, editing/undo/title/save fidelity, mode switches, CSS/data
  pause/resume and real HTML/PDF export. Nineteen native captures generated; current
  light/dark/narrow/reference surfaces inspected and four tracked images refreshed.
  build-native and modern SDK compile/install pass; the native isolated asset prefix
  validates actual offline MathJax fallback installation. Strict schemas, resources,
  JS syntax/vendor hash/install paths, README links and task-specific diff/ownership
  pass. Existing test deprecations remain; no new production warnings/GTK criticals.

- Pointer correction: `/window/rich-pointer` fails before and passes after the
  gesture change using real XTest motion/press/release events. Verifies unchanged
  label/caret on press, one toggle on release, four toggles through refresh,
  drag-away cancellation, rendered-image click and no selection/dirty/text change.
  Native optimized build and 7/7 suites pass (16 window workflows, 58.78s, no skips);
  modern SDK build/assets and 11/11 suites pass (56.66s), including the pointer case.
  Suite timeout is now 90s because new pointer coverage approaches the old 60s.
  No GTK CSS errors/criticals. Existing tinyexpr/style-helper warnings and optional
  X11 test-backend deprecations on newer GTK remain; production adds no warnings.
  Schemas/resources/manifests are unchanged. Ownership and task-specific diff pass.

- Source/Render follow-up: the no-selection regression failed on the original
  controller, then passed after the fix. Extended native regression exercises two
  Source choices, refresh, 500px/1200px resizing, edits before regions, undo,
  independent Render, unrelated regions remaining rendered and source/save fidelity.
  Final native optimized build and all seven suites pass (51.78s, no skips).
  Modern SDK build/assets and all eleven suites pass (49.08s). Existing GTK
  style-context deprecation warnings are in the UI test helper, not this controller.
  No GTK CSS errors/criticals; isolated EGL/accessibility-bus notices remain.
  GTK button/gesture activation is automated under Xvfb; physical mouse interaction
  was not separately exercised. Schemas/resources are unchanged by this fix.

- Styling correction: normal native build-native optimized compilation and all
  seven suites pass (15 window workflows, 49.56s, no skips); modern SDK rebuild/
  asset install and all eleven suites pass (47.95s). Reviewed native light/dark
  wide, medium Pages and narrow Pages/Outline plus modern light/dark wide captures
  in two bounded inspection rounds. Borders are continuous and icon actions flat;
  selected states and native overlay shadows remain visible. Captures are under
  /tmp/marker-sidebar-styling-screenshots/native and ignored
  build-gtk4/sidebar-styling-screenshots. No compiler or GTK CSS errors; isolated
  WebKit accessibility-bus/EGL notices remain. Native test environment is explicitly
  whitelisted to avoid inherited credentials. Schemas/resources/diff checks and
  the one Impeccable mechanical scan pass. No new tests mirror CSS implementation.

- Phase 7 native: extracted noble development packages into a task-only prefix;
  optimized build links the host GTK 4.14.5/adwaita 1.5/sourceview 5.12/WebKit
  2.52.6 and extracted libspelling 0.2.0. Installed assets into the validation
  prefix. All seven suites pass, no skips (window workflows: 15 cases, final 52.46s).
  Native WebKit sandbox remains enabled. Actual spell annotations/toggle/source
  fidelity, rendered images/math/tables/gnuplot/Mermaid, scroll/undo/save, all view
  modes, responsive navigation, notebook CSS refresh and HTML/PDF export pass.
  Reviewed final native light-wide, dark-wide and rich-narrow captures from
  /tmp/marker-zorin-compat/final-screenshots; 19 synthetic captures generated.
  Marker compiles without C warnings; existing GCC 13 tinyexpr partial-allocation
  warnings and external itstool Python escape-sequence notices remain.
- Phase 7 fallback: pinned upstream libspelling 0.2.1 independently builds and
  passes its registered cursor test in GNOME 50 SDK. Stock modern libadwaita lacks
  Zorin's optional theme-path property and follows its original theme path.
- Phase 7 modern: optimized SDK build/install and all eleven suites pass after
  readiness cancellation (window workflows 47.09s). GTK 4.22/adwaita 1.9/sourceview
  5.20/WebKit 2.54 and cached libspelling 0.4.10 remain supported. No production
  sandbox override; the existing nested-SDK test override remains test-only.
- Strict schemas, resource dependencies, desktop/AppStream metadata, configure
  syntax, 15 README Bash examples and diff checks pass. AppStream retains four
  pre-existing informational findings. Final text logs have inherited environment
  lines redacted; never include those lines when reviewing or sharing diagnostics.
- Initial native setup in build-native still lacks system gtk4.pc; development
  packages are not installed on the host. sudo -n still requires a password.
- The validation-prefix native executable opens a real X11 window with an isolated
  memory-settings/profile and disposable Markdown. A private-bus launch accepts
  the quit action and exits zero, but portal secret-service startup delays mapping;
  a no-session-bus launch confirms the mapped window, then the task-owned process
  is stopped. Isolated bus/accessibility and software-renderer EGL notices remain.
  This is a validation-prefix launch, not /usr/local installation or desktop chooser
  acceptance. No user document/profile or running user app was touched.

- Licensing correction: inspected bundled Copyright/provenance, worker communication
  and packaging; checked upstream gnuplot terms and FSF aggregation guidance.
  This is an architecture-based licensing assessment, not a new source-build audit
  or definitive legal determination. Runtime hash and README link/diff checks pass.

- README follow-up: 15 local links/anchors exist; 11 shell snippets pass bash -n.
  Current light-wide/rich-dark captures reviewed; dependencies/labels/submodule
  chain checked against this checkout. Donation image has no code/resource/package
  references and is removed. SDK rebuild passes; final diff passes whitespace review.
  .codex/PULL_REQUEST.md contains the prepared review description for fork master.
- Published README renders on GitHub; logo and both screenshots load with their
  expected dimensions. GitHub comparison targets this fork master and reports
  Able to merge. No authenticated CLI/API/browser session is available to submit PR.

- Phase 6: Unicode/protected/incomplete/one-column-table/multiline-math discovery;
  real image/math/table/gnuplot/Mermaid output; Source/Render, native edit/undo,
  selection, exact save bytes, light/dark/narrow sizes, mode changes and teardown.
  Missing image/invalid plot recovery, math fences, notebook CSS and CSV monitors,
  actual HTML/PDF export with no inline UI annotations pass. All 11 suites pass
  in the final run on task-owned :93 (window workflows 44.60s); only isolated SDK
  bus notices, no GTK criticals or C compiler warnings. Rich editing also passes at
  2x display scale. Actual image click and unsaved notebook relative assets pass.
  JavaScript syntax, strict schemas, resource dependencies and diff checks pass.
  Final light/dark/narrow captures are rich-light.png, rich-dark.png, rich-narrow.png.
  The optimized Flatpak rebuilt successfully and was reinstalled in the user's
  existing installation. No live user documents were touched or app process closed.

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

# Previous handoff (phases 0–5)

## Implemented

All six implementation phases are complete. FEATURE/PLAN/TASKS/DECISIONS describe
this checkout. Notebook rail/editor, Pages/Files, responsive regional shell,
formatted writing/gutters, live right outline and shared notebook CSS/export are
implemented; obsolete verified code is removed. The final Folio reference correction
uses soft sidebar surfaces, native column dividers and 640px/Monospace 11 defaults
without resetting explicit choices. Installation instructions are in README;
the local optimized Flatpak package now builds, installs, launches and exports.
The feature is committed as 6c51fe11, with nested renderer commits 995fba6/35e95a1.
Final tracking notes are committed separately; git log -2 identifies both parent
commits. No changes were pushed.

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

From the repository root, install the ready package with
`flatpak install --user build-flatpak-repo com.github.fabiocolacio.marker`, then
`flatpak run --user com.github.fabiocolacio.marker`. README contains prerequisites,
rebuilding, updating/removing, native requirements and SDK development instructions.

Use a disposable notebook in a GNOME desktop session for the deferred integration
checks, then build Debian release artifacts in CI. Publish the local renderer
commits to reachable remotes before sharing the parent branch with another machine;
the parent alone cannot supply new submodule objects. Reuse these five documents for any follow-up;
do not rebuild another parallel plan or restore inherited files over this work.

# Previous handoff (phase 6)

## Implemented

The inline-rich follow-up is complete. Formatted Markdown renders top-level images,
math, tables, gnuplot charts and Mermaid diagrams in the writing page. Click or
Source exposes the original region; Render switches back. Caret/selection/search
reveal source, native edit/undo/save remain authoritative, and failures provide
editable source with local explanation/retry. Preferences, notebook CSS and linked
CSV changes refresh output. Normal preview/export also support math fences, and
unsaved notebook drafts resolve relative assets against their notebook root.
README documents use and limits. The optimized Flatpak is rebuilt and installed
in the user's existing local installation.

Subsequent documentation/merge request authorized committing the inline-rich work.
It is now 144927f2 on origin/feature/workspace-theme-preferences. README and PR
description are rewritten, validated and published as ebecce3f; PR submission awaits
GitHub authentication. No PR number or remote master merge is claimed.

## Important architectural decisions

Retain GtkSourceBuffer and existing heading/protected-block discovery. MarkerRichView
owns only ephemeral presentation: one lazy offscreen MarkerPreview, one scrolling
zero-measure GTK layer, marked source ranges and generation-checked snapshots.
No child anchors, projection buffer, duplicate undo model or per-region WebKit.
Readback is bounded at 64 MiB and scales with HiDPI. A formatted-only number gutter
omits compressed source ranges; source mode uses its ordinary gutter. Original
Markdown supplies exports, keeping rendering UI out of saved/exported documents.

## Validation performed

Final GNOME 50 SDK build and all 11 Meson suites pass, including 14 real window
workflows (44.60s). No C compiler warnings or GTK criticals; isolated SDK bus
connection notices remain. Pure tests cover Unicode, protected/incomplete markup,
references, one-column tables and multiline math. Real GTK/WebKit checks cover
image click, Source/Render, editing/undo/exact save bytes, selection, view changes,
scrolling, light/dark/narrow and 2x scale, failure/recovery, lazy renderer lifetime,
stale callbacks/close, live notebook CSS/CSV, unsaved draft assets and actual
HTML/PDF output. Synthetic application captures rich-light.png, rich-dark.png and
rich-narrow.png were reviewed in two bounded rounds; overlapping line numbers were
corrected. Strict schemas, resources, JavaScript, manifest and diff checks pass.
Optimized Flatpak build has no compiler warning/error findings and reinstall
succeeds. Task-owned Xvfb is stopped; tests never wrote into user notebooks.

## Known limitations

Inline regions are visual snapshots. Inline math/images edit their whole paragraph;
links and chart interactions use regular preview. Complex list/quote nesting keeps
source presentation. Oversized documents fall back to source and remain available
in regular preview. Mathematics/gnuplot/Mermaid need their existing Preview
preferences enabled. Existing local-asset export portability and host native-build
version constraints remain as documented in README and the previous handoff.

## Deferred work

- Nested rich-region conformance and interactive inline web content: guessing
  nested boundaries risks source structure; snapshots intentionally open source.
  Impact and next steps are recorded in TASKS. Add real-document regressions before
  extending discovery; assess interactive widgets against an actual workflow.
- The previous desktop chooser/Files/physical-printer and Debian package checks
  remain explicitly deferred for their required environments. Backend file/save,
  responsive UI and actual HTML/PDF behavior are exercised here.

## Recommended follow-up

Save any running documents, close Marker, then launch the installed build with
`flatpak run --user com.github.fabiocolacio.marker`. Use Formatted Markdown and click
a rendered region or Source to edit; Render restores its output. Review the README
usage section and test representative scientific notebooks, especially unusually
nested markup. Keep these five tracking files as the implementation source of
truth. Inline-rich work is committed/pushed as 144927f2. README now uses current
light/dark captures and task-focused installation/usage/development guidance, credits
Fabio Colacio/contributors/Folio and removes old package advertising/donations.
Documentation is committed/pushed as ebecce3f; user merged PR #1 into master as
40246940. Nested renderer commits are reachable in the user's forks. Checkout is
master. The host still lacks flatpak-builder; run sudo apt install flatpak-builder
elfutils interactively, then the README builder and flatpak install --user --reinstall
commands. Chain build/install with && so failed builds do not trigger installation
from an empty repository. GNOME 50 Platform/SDK and a user Marker installation are
already present. Host tool installation is not completed because sudo needs a password.

# Final handoff

## Implemented

Rendered-size follow-up corrects the user's saved 210% preview zoom to 100%.
Shared Monospace 11pt typography, 640px measure and heading hierarchy are retained;
no production source/schema/default changes were needed for this issue. Existing
native typography validation passes and README documents the preview zoom reset.

Ctrl+H now opens Find and Replace in the existing editor search bar; Replace and
Replace All use the canonical buffer and native undo. Empty replacement deletes
matches. Ctrl+F remains Find-only; next/previous find navigation advances correctly.
The document menu exposes replacement. Preview-only opens editable source for it.
Native ~/.local installation is updated; restart an older process to use Ctrl+H.

Native user installation is complete: `marker` and the explicit user desktop entry
select ~/.local/bin/marker from build-user. Direct installed-app captures in
INSTALLATION-PROOF.md demonstrate all four scientific engines and released-button
Source/Render. The user's saved Dual Pane view hides the Formatted-only control;
Ctrl+5 exposes it. That earlier installation-only chunk changed no app source or
existing notebooks/settings; the subsequent Ctrl+H chunk updates editor code.

Formatted Markdown supports gnuplot, Mermaid, Charter and inline/display/fenced
KaTeX or MathJax using the existing scientific renderer. Charter's existing setting
is exposed in Preferences. A pinned, licensed offline MathJax 3.2.2 fallback works
when the preferred system MathJax 2 is absent.

One page-wide Source/Render button at the right of the formatting toolbar replaces
region buttons/pinning/cursor-toggle API. Source shows raw Markdown, stops workers/
data monitoring and disposes the inline renderer. Render resumes from current
source/preferences/CSS; choice survives view switches for that live editor.
Rendered-element clicks request page Source. Existing caret/search reveals in
Render remain local editing behavior. Formatting does not duplicate full-preview
evaluation. Print/export render explicitly on demand. GTK's modified-changed signal
keeps title/Save/page dirty indicators accurate after edit/undo/save.

README usage and four light/dark/narrow/reference screenshots are updated. Prior
native compatibility, notebook navigation, styles, source fidelity and exports remain.

## Important architectural decisions

Find/replace shares the editor's existing search context/bar; no separate dialog,
search model, buffer, preference or persistence migration. Widget/signal ownership
stays with the editor and GtkSourceView owns replacement/undo semantics.

Use a correctly configured user-prefix build instead of requiring authenticated
system installation. Keep the application ID/profile and install all runtime
assets through Meson. Proof runs the installed executable separately from tests
with isolated MathJax settings, synthetic data and an unmodified WebKit sandbox.

One canonical buffer and live-editor presentation choice; no persistence migration.
One shared parser/renderer/cascade; no per-region explicit-state transfer or button
controller observation. Picture gestures still claim press; the toolbar uses normal
GTK button behavior. Pausing invalidates requests, cancels readiness/monitors and
unloads scientific documents with JavaScript disabled. Stale provisional commits
are replaced with blank; outstanding readiness waits terminate with a paused error.
Print retains its editor and waits for WebKit job completion before pausing again.
MathJax fallback origin/version/hash/license are recorded beside the unmodified asset.

## Validation performed

Ctrl+H follow-up: optimized native build and all seven suites pass on isolated Xvfb
(17 window workflows, 89.24s, no skips; timeout multiplier 2 for portal startup).
Actual Ctrl+H key dispatch, repeated opening, selected-match navigation, replacement,
Replace All/atomic undo, deletion, absent/empty queries, Ctrl+F transition, close and
source/formatted/preview mode behavior are covered. Native install hashes match;
desktop entry/schema/diff checks pass. No new production warnings; existing GTK
test-helper deprecations and isolated portal/EGL diagnostics remain.

Installation follow-up: build-user optimized build/install, matching binary hashes,
desktop-entry validation, strict schemas/resources and task diff review pass.
All seven native suites pass (window workflows 86.92s; no skips) on isolated Xvfb.
Three actual installed-app captures are reviewed; Source persists after release
and scrolling without selection, and Render resumes equations/diagrams/charts.
The earlier live-desktop 6/7 run hit a GDK surface assertion and is not counted as
a pass. Portal/EGL diagnostics are documented. Temporary app/display sessions stop
after validation; user's view preference remains Dual Pane.

Final native optimized builds pass against the installed GTK 4.14/adwaita 1.5/
SourceView 5.12/spelling 0.2 stack. All 7 suites pass (16 window workflows, 60.92s,
no skips). The isolated native asset prefix exercises the new installed fallback;
build-native is also compiled for system reinstall.
Modern SDK optimized build/assets and all 11 suites pass (57.32s), including the
final modified-signal correction and real pointer regression. All Marker workflows
run; upstream libspelling skips one optional words-database case (database absent).

Actual Mermaid/Charter/gnuplot SVG and both math backends, inline/display/fenced math,
disabled renderer preferences, Source editing/undo/no selection/view switches,
readiness cancellation/stale commits, real mouse press/release/drag/image activation,
CSS/data pause/resume, unchanged saved Markdown and real HTML/PDF export are covered.
Native visual matrix exercises defaults, persistence, light/dark and wide/medium/
narrow layouts. Nineteen isolated captures generated; updated reference/rich images
reviewed. Strict schemas, compiled resources, both JavaScript syntax checks, vendor
hash/install paths, README local links and ownership/stale API/task diff review pass.

## Known limitations

System MathJax 2 branch is retained but this host/SDK lacks it; actual fallback
MathJax 3 and KaTeX are tested. Physical printer/desktop chooser integrations need
the normal desktop environment; PDF and print preparation cancellation are exercised.
Existing nested-region/interactive-preview/local-export-asset limitations remain.
Existing style-helper/X11 test deprecations and native tinyexpr warnings remain;
this task adds no production compiler warnings. Isolated bus/software-renderer
notices are environmental; no GTK CSS errors/criticals were found.

## Deferred work

System reinstall deferral superseded by native user installation. sudo -n still
requires a password, but build-user is installed under ~/.local and the user
desktop entry explicitly launches it. /usr/local already matched build-native
at this follow-up's start. Existing system files and user settings are preserved.
Previous desktop/package/conformance deferrals remain explicit in TASKS.

## Recommended follow-up

Use build-user for this user's subsequent updates: meson compile -C build-user
then meson install -C build-user. Run marker and select Formatted Markdown (Ctrl+5).
See INSTALLATION-PROOF.md for executable/launcher identity and capture evidence.
No commit/push requested or performed. Inherited edits are preserved; final task
counts compare against /tmp/marker-page-render-before. SDK diagnostic text logs
redact inherited environment; task-owned Xvfb is stopped after validation.
