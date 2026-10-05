# Modernize Marker with Folio notebooks and rich Markdown editing

Marker moves to GTK 4 and libadwaita with a notebook-based writing interface while keeping Markdown files and nested scientific projects intact.

## Changes

- Add configurable notebook book icons, Pages/Files navigation, search, regional headers and responsive navigation.
- Add centered Formatted Markdown, heading gutters and a right-side outline. Images, equations, tables, gnuplot charts and Mermaid diagrams render inline; Source/Render edits the original buffer with native undo.
- Share notebook `.marker.css` styles across preview and HTML/PDF export, with live stylesheet and linked-data refresh.
- Remove verified obsolete GTK3 code, repair packaging and point recursive renderer submodules at reachable SSH forks.
- Rewrite README with current screenshots, clone/install/update instructions, workflows, limitations and original creator/contributor credits. Remove donations and the unused PayPal image.

## Validation

GNOME 50 SDK and optimized local Flatpak builds pass; the Flatpak is installed locally. All 11 Meson suites pass, including real GTK/WebKit editing, responsive and dark states, 2x display scaling, persistence, failure recovery, live CSS/CSV and HTML/PDF export. Schemas, resources, scripts and metadata validate.

The documentation follow-up checks 15 local links/anchors and 11 shell examples, reviews the current screenshots and rebuilds the SDK target after removing the unused asset.

## Review notes

Inline elements are visual snapshots; complex list/quote nesting keeps source, and interaction uses regular preview. Native builds require the documented GTK/WebKit versions. Pandoc office export is optional and not bundled by the Flatpak. Desktop chooser/file-manager/physical-printer and Debian package checks remain documented deferrals.

Base: `pietrodantuono/Marker:master`. Head: `feature/workspace-theme-preferences`.
