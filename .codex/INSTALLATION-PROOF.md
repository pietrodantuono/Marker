# Native installation and rendering proof

Date: 2026-10-06

## Installed executable and launcher

The current checkout is installed natively into `/home/pietro/.local`. No Flatpak
or administrator password is needed for this user-owned installation:

```sh
meson setup build-user --prefix="$HOME/.local" --buildtype=release
meson compile -C build-user
meson install -C build-user
desktop-file-edit --set-key=Exec --set-value="$HOME/.local/bin/marker %U" \
  "$HOME/.local/share/applications/com.github.fabiocolacio.marker.desktop"
update-desktop-database "$HOME/.local/share/applications"
```

For subsequent updates, run the compile, install and desktop-entry commands above.
Meson reinstalls the generic desktop entry, so restore its explicit Exec afterwards.
`marker` resolves to
`/home/pietro/.local/bin/marker`. The user desktop entry keeps the existing
application ID and explicitly uses `Exec=/home/pietro/.local/bin/marker %U`.
Desktop entry validation and desktop database refresh pass.

The installed executable and `build-user/marker` have the same SHA-256:
`6104e2b576d361e258936b46a3669c361f93e836401185bac2178abea4972037`.
The earlier `/usr/local` installation already contained the feature; it was
not the reason the Source/Render control was missing.

## Show the feature

The user's saved view is **Dual Pane**. The page-wide toggle belongs to
**Formatted Markdown**, selected with **Ctrl+5** or the document view chooser.
Its **Source** / **Render** button is at the bottom right of the formatting bar.
Source displays the full original buffer and pauses scientific evaluation;
Render resumes rendered elements. Preferences → Preview enables individual
renderers and selects KaTeX or MathJax.

Open the synthetic demonstration with the installed executable:

```sh
marker --formatted "$PWD/.codex/demos/scientific-rendering.md"
```

The demonstration contains MathJax-compatible equations, Mermaid, Charter and
gnuplot. It contains no user notebook data. Proof uses a temporary keyfile settings
profile selecting MathJax and isolated session state; the user's settings remain
unchanged. Captures must come directly from the installed executable's X11 window,
not the UI test harness or a generated illustration.

Fresh-profile initialization resets a preseeded math backend to KaTeX. MathJax is
selected after that initialization and verified again after the proof app launches.
The final captures replace the initial KaTeX captures.

## Captures and validation

Direct X11 captures from `/home/pietro/.local/bin/marker` (PID 2467404, window
0x200004, 1200×800) on task-owned display :0:

- [Render: MathJax, Mermaid and Charter](screenshots/installed-render.png)
- [Render: Charter and gnuplot](screenshots/installed-charts.png)
- [Source: raw Markdown and released-button state](screenshots/installed-source.png)

The actual installed app runs separately from the test harness. A normal mouse
press/release selects Source; scrolling without text selection leaves it in Source.
A second normal click resumes math/diagram/chart rendering. Images are unedited
application-window captures, with no desktop or real notebook content. Production
WebKit sandboxing remains enabled.

The optimized build and native installation succeed. All seven build-user suites
pass on isolated Xvfb (window workflows: 86.92s; no skipped suites). A first live
desktop test run failed with a GDK surface/device-position assertion after two
workflows; this result is recorded rather than counted as a pass. Existing
tinyexpr allocation warnings and deprecated GTK test helpers remain. Portal/EGL
diagnostics on the isolated display are environmental. Desktop launcher validation,
strict schemas, resources and the task diff are checked. Test/proof app sessions
and the task-owned display are stopped after captures; user settings are unchanged.
