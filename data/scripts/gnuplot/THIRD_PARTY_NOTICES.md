# Third-party notices and binary provenance

Marker's gnuplot adapter is derived from Reonarudo's
`gnuplot-markdown-preview` VS Code extension. Its MIT license is included in
`EXTENSION-LICENSE`. This does not relicense the bundled software.
The audited runtime artifacts were vendored from upstream commit
`d1aff4764d72e6da7f871c9f60f16e3a3cce2b43`.

## Gnuplot 6.0.2

Copyright 1986–1993, 1998, 2004 Thomas Williams, Colin Kelley, with additional
contributors identified in `SOURCE-NOTICES.txt`.

The bundled runtime was compiled from the **unmodified official gnuplot 6.0.2 release**, downloaded from:
https://sourceforge.net/projects/gnuplot/files/gnuplot/6.0.2/gnuplot-6.0.2.tar.gz/download

Source archive SHA-256: `f68a3b0bbb7bbbb437649674106d94522c00bf2f285cce0c19c3180b1ee7e738`.

Full gnuplot redistribution terms are included in `Copyright`; retain that file
and this document with redistributions. Component notices, including
BSD-licensed numerical code, are retained in `SOURCE-NOTICES.txt`.

No gnuplot source changes or patches were applied. Configuration and linker
flags select a browser/worker-only Emscripten adapter, disable dynamic
execution and optional native dependencies, and set fixed linear memory.

The runtime was built with Emscripten 6.0.9 (the audited toolchain reports
`6.0.9-git`). Exact artifact hashes are recorded in `SHA256SUMS`.

Upstream contacts: https://www.gnuplot.info/ and
gnuplot-info@lists.sourceforge.net. Original extension:
https://github.com/Reonarudo/gnuplot-markdown-preview.

## Emscripten and standard libraries

The generated JavaScript adapter and linked runtime contain Emscripten code
(MIT / University of Illinois/NCSA; `EMSCRIPTEN-LICENSE`), musl libc
(`MUSL-COPYRIGHT`), and LLVM runtime / libc++ components (`LLVM-LICENSE`).
These full notices accompany the binary.
