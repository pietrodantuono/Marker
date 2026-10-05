# <img width="30" src="data/com.github.fabiocolacio.marker.svg"/>Marker

Marker is a Markdown editor for Linux made with GTK 4 and libadwaita.

**NOTE:** Issues regarding markdown parsing should go to the [scidown repo](https://github.com/mandarancio/scidown).

## Features

* View and edit markdown documents
* Open folders as notebooks, with Pages/Files navigation for nested Markdown, images, and data files
* Switch between source, preview, dual-pane, dual-window, and formatted Markdown views
* HTML and LaTeX conversion of markdown documents with [scidown](https://github.com/Mandarancio/scidown/)
  * Support for YAML headers
  * Document classes
  * Beamer/presentation mode (`class: beamer`)
  * Abstract sections
  * Table of Contents
  * External document inclusion
  * Equations, figures, table and listings with reference id and caption
  * Internal references
* Extra scientific syntax of SciDown on its [wiki](https://github.com/Mandarancio/scidown/wiki/)
* TeX math rendering with [KaTeX](https://khan.github.io/KaTeX/) or [MathJax](mathjax.org/)
* Support for [mermaid](https://mermaidjs.github.io/) diagrams
* Support for [charter](https://github.com/Mandarancio/charter/) for plotting
* Sandboxed [gnuplot](https://www.gnuplot.info/) previews in fenced code blocks, including relative CSV, DAT, and TXT data files
* Syntax highlighting for code blocks with [highlight.js](https://highlightjs.org/)
* Built-in HTML and PDF export
* Office document export with [pandoc](https://pandoc.org/)
  * RTF
  * ODT
  * DOCX
* Custom CSS themes and notebook `.marker.css` overrides
* Custom syntax themes
* Native GTK 4 and libadwaita application

## Screenshots

![scrot.png](help/C/figures/scrot.png)

![scrot1.png](help/C/figures/scrot1.png)

![scrot2.png](help/C/figures/scrot2.png)

![slides.png](help/C/figures/slides.png)

## Packages

* [Fedora (thanks to @tim77)](https://src.fedoraproject.org/rpms/marker)
* [Flathub (thanks to @jsparber and @bertob)](https://flathub.org/apps/details/com.github.fabiocolacio.marker)
* [Arch Linux AUR (thanks to @mmetak)](https://aur.archlinux.org/packages/marker-git/)
* [Arch Linux official package (thanks to @City-busz)](https://archlinux.org/packages/extra/x86_64/marker/)
* [Ubuntu PPA Stable (thanks to @apandada1 and @olivierb2)](https://launchpad.net/~apandada1/+archive/ubuntu/marker)
* [Ubuntu PPA Daily builds (thanks to @apandada1 and @olivierb2)](https://launchpad.net/~apandada1/+archive/ubuntu/marker-daily)
* [![Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/marker)


## Rich elements in Formatted Markdown

Formatted Markdown shows images, equations, Markdown tables, gnuplot charts and
Mermaid diagrams within the writing page. Click an element or its **Source**
button to edit the original Markdown in place; **Render** switches back. Inline
images and equations reveal their containing paragraph. Keyboard navigation and
selection into a rendered region also expose its source.

Enable mathematics, Mermaid and gnuplot in Preview preferences as needed. Notebook
styles apply to the rendered elements, and linked gnuplot data changes refresh them.
Unavailable images or failed plots keep editable source with a retry control.
Saving, undo and exports use the original Markdown buffer.

Complex list/quote nesting retains source presentation. Inline snapshots have a
64 MiB readback budget; oversized documents fall back to source and remain viewable
in the regular preview. Interactive plot controls and links use the regular preview;
the inline surface opens source for editing.

## Install this checkout

These instructions install the current GTK 4/Folio redesign from your local
checkout. Published packages and older release archives may contain a different
version. Run the build commands from the directory containing `meson.build`.

### Recommended: local Flatpak

Flatpak supplies the required GNOME 50 runtime, so your distribution does not need
new GTK/WebKit development packages. On Debian/Ubuntu, install the build tools:

```bash
sudo apt install flatpak flatpak-builder elfutils git
```

Set up Flathub and install the runtime/SDK if they are not already installed:

```bash
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install flathub org.gnome.Platform//50 org.gnome.Sdk//50
```

For a fresh clone, initialize the renderer submodules before building:

```bash
git submodule update --init --recursive
```

Build the local sources and install Marker for your user:

```bash
flatpak-builder --force-clean --state-dir=build-flatpak-state \
  --repo=build-flatpak-repo build-flatpak com.github.fabiocolacio.marker.json
flatpak install --user build-flatpak-repo com.github.fabiocolacio.marker
flatpak run --user com.github.fabiocolacio.marker
```

Marker also appears in your application launcher. This installs the checkout's
build, independently of a system-wide Marker package. The app has access to your
home directory for notebook folders. Flatpak settings/state use its application
profile; a native installation's profile is not automatically imported.

To update after changing the source, close Marker, rerun the builder command,
then replace the user installation:

```bash
flatpak install --user --reinstall build-flatpak-repo com.github.fabiocolacio.marker
```

To remove the app while retaining its profile and notebook folders:

```bash
flatpak uninstall --user com.github.fabiocolacio.marker
```

HTML and PDF export, mathematics, diagrams and the bundled gnuplot WebAssembly
renderer are included. The manifest does not bundle Pandoc; DOCX/ODT/RTF export
requires Pandoc inside the app's environment, not just installed on the host.
See the [Flatpak build guide](https://docs.flatpak.org/en/latest/first-build.html)
and [builder options](https://docs.flatpak.org/en/latest/flatpak-builder-command-reference.html)
for the underlying build/export/install workflow.

### Native installation

Use this route when the host provides **all** of these development dependencies:

* C compiler, pkg-config, Meson and Ninja
* GTK >= 4.18
* libadwaita >= 1.7
* GtkSourceView >= 5.14
* WebKitGTK >= 2.50 with the 6.0 API
* GLib/GIO and libsoup 3
* libspelling >= 0.4.10 (Meson can download the pinned fallback)
* gettext and itstool
* pandoc (optional, for DOCX/ODT/RTF export)

After initializing the submodules as above, use a fresh native build directory:

```bash
meson setup build-native --prefix=/usr/local --buildtype=release
meson compile -C build-native
sudo meson install -C build-native
meson test -C build-native --print-errorlogs
marker
```

Install the assets before running GUI/export tests; they use the configured prefix.
An old `build` directory configured for GTK 3 retains its old dependencies and
source paths. Use `build-native` for this branch. Installing `libgtk-4-dev` alone
is insufficient when its GTK version is below 4.18 or other dependencies are
missing. The native commands require compatible host libraries; an SDK-built
binary should run in its matching Flatpak environment.

### SDK development build

For development without replacing host libraries, start a GNOME 50 SDK shell:

```bash
flatpak run --filesystem="$PWD" --command=sh org.gnome.Sdk//50
```

Run the following **inside that shell**; the prefix keeps the installation in the
checkout rather than changing the host:

```bash
meson setup build-sdk --prefix="$PWD/build-sdk/install"
meson compile -C build-sdk
meson install -C build-sdk
meson test -C build-sdk --print-errorlogs
exit
```

GUI tests require a display and installed assets; unavailable display checks are
reported as skipped. Use the local Flatpak route above for a desktop installation.

## Donations/Tips

If you like Marker and would like to support the development of this project, please donate below!

[<img height="30" src="donate.png" alt="PayPal"/>](https://www.paypal.me/fabiocolacio)
