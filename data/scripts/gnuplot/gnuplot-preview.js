/*
 * Gnuplot Markdown preview adapter.
 *
 * Derived from gnuplot-markdown-preview by Reonarudo, licensed under the MIT
 * license included beside this file. Marker-specific browser integration is
 * Copyright (C) 2026 Marker contributors.
 */

(() => {
  "use strict";

  const MAX_SOURCE_BYTES = 64_000;
  const MAX_FILE_BYTES = 1024 * 1024;
  const MAX_TOTAL_BYTES = 4 * 1024 * 1024;
  const MAX_FILES = 16;
  const MAX_OUTPUT_BYTES = 4_000_000;
  const WORLD_HANDLER = window.webkit?.messageHandlers;
  const encoder = new TextEncoder();
  const session = Math.random().toString(16).slice(2);
  const renderedPlots = [];
  let occurrence = 0;

  const workerSource = String.raw`
import createGnuplot from "marker-gnuplot:///gnuplot.mjs";

const MAX_FILE_BYTES = 1024 * 1024;
const MAX_TOTAL_BYTES = 4 * 1024 * 1024;
const MAX_FILES = 16;
const MAX_OUTPUT_BYTES = 4_000_000;
let diagnostics = "";
const log = (line) => {
  if (diagnostics.length < 16_000) diagnostics += String(line) + "\n";
};

try {
  const response = await fetch("marker-gnuplot:///gnuplot.wasm");
  if (!response.ok) throw new Error("Unable to load the bundled gnuplot runtime.");
  const instance = await createGnuplot({
    wasmBinary: new Uint8Array(await response.arrayBuffer()),
    noInitialRun: true,
    noFSInit: true,
    print: log,
    printErr: log,
  });
  const fs = instance.FS;
  fs.init(() => null, null, null);
  fs.mkdir("/work");
  fs.chdir("/work");

  const originalOpen = fs.open.bind(fs);
  const dataNames = new Set();
  let mounting = false;
  fs.open = (path, flags, mode) => {
    const name = path.startsWith("/work/") ? path.slice(6) : path;
    const reading = typeof flags === "number"
      ? (flags & 3) === 0 && (flags & (64 | 512 | 1024)) === 0
      : flags === "r";
    const allowed = name === "plot.svg" ||
      (name === "script.gp" && (mounting || reading)) ||
      (dataNames.has(name) && (mounting || reading));
    if (!allowed) throw new fs.ErrnoError(44);
    return originalOpen(path, flags, mode);
  };

  const heapSnapshot = Uint8Array.from(instance.HEAPU8);
  self.onmessage = ({data: request}) => {
    let result;
    let success = false;
    diagnostics = "";
    try {
      const files = request.files || [];
      const total = files.reduce((sum, file) => sum + file.bytes.length, 0);
      if (files.length > MAX_FILES || total > MAX_TOTAL_BYTES)
        throw new Error("Plot data exceeds input limits.");

      mounting = true;
      for (const file of files) {
        if (!/^data-\d+\.dat$/.test(file.name) || dataNames.has(file.name) ||
            file.bytes.length > MAX_FILE_BYTES)
          throw new Error("Invalid plot data snapshot.");
        dataNames.add(file.name);
        fs.writeFile(file.name, file.bytes);
      }
      fs.writeFile("script.gp",
        "set terminal svg size 1000,500 enhanced background rgb 'white'\n" +
        "set output 'plot.svg'\n" + request.source + "\n");
      mounting = false;

      const exitCode = instance.callMain(["script.gp"]);
      if (exitCode !== 0)
        throw new Error(diagnostics.trim() || "Gnuplot rejected this program.");
      result = fs.readFile("plot.svg", {encoding: "utf8"});
      if (!result.includes("</svg>"))
        throw new Error(diagnostics.trim() || "Gnuplot did not produce a complete SVG plot.");
      success = true;
    } catch (error) {
      result = error instanceof Error ? error.message :
        diagnostics.trim() || "Gnuplot rendering failed.";
    } finally {
      mounting = false;
      for (const path of ["script.gp", "plot.svg", ...dataNames]) {
        try { fs.unlink(path); } catch (_) { /* The file may not exist. */ }
      }
      dataNames.clear();
      instance.HEAPU8.set(heapSnapshot);
    }

    if (new TextEncoder().encode(result).length > MAX_OUTPUT_BYTES) {
      result = "Gnuplot output exceeds the 4 MB limit.";
      success = false;
    }
    self.postMessage({type: "result", id: request.id, success, result});
  };
  self.postMessage({type: "ready"});
} catch (error) {
  self.postMessage({
    type: "startup-error",
    error: error instanceof Error ? error.message : "WASM initialization failed.",
  });
}
`;

  function message(error, fallback = "Gnuplot rendering failed.") {
    return error instanceof Error && error.message ? error.message : fallback;
  }

  function checkOutputPolicy(source) {
    const code = source
      .replace(/\\\r?\n/g, "")
      .replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|#[^\n]*/g, " ");
    if (/\b(?:set|unset)\s+(?:term(?:i(?:n(?:a(?:l)?)?)?)?|out(?:p(?:u(?:t)?)?)?)\b/i.test(code))
      throw new Error("The preview controls terminal and output. Remove set terminal / set output commands.");
    if (/(?:^|[;\n])\s*(?:load|call)\b/i.test(code))
      throw new Error("Loading external gnuplot scripts is not supported.");
  }

  function dataReferences(source) {
    const tokens = [];
    let index = 0;
    let lineStart = true;
    while (index < source.length) {
      if (lineStart) {
        const block = /^\s*\$[\w]+\s*<<\s*([\w]+)[^\n]*\n/.exec(source.slice(index));
        if (block) {
          index += block[0].length;
          while (index < source.length) {
            const newline = source.indexOf("\n", index);
            const end = newline < 0 ? source.length : newline;
            const line = source.slice(index, end).trim();
            index = newline < 0 ? end : end + 1;
            if (line === block[1]) break;
          }
          lineStart = true;
          continue;
        }
      }

      const char = source[index];
      if (char === "\\" && /^\\\r?\n/.test(source.slice(index))) {
        index += source[index + 1] === "\r" ? 3 : 2;
        continue;
      }
      if (char === "#") {
        while (index < source.length && source[index] !== "\n") index++;
        continue;
      }
      if (/\s/.test(char) && char !== "\n") {
        index++;
        continue;
      }

      const start = index;
      if (char === "\"" || char === "'") {
        index++;
        let value = "";
        while (index < source.length && source[index] !== char) {
          if (source[index] === "\\") {
            const next = source[++index];
            if (next === undefined) break;
            value += next === char || next === "\\" ? next : "\\" + next;
            index++;
          } else {
            value += source[index++];
          }
        }
        if (source[index] !== char)
          throw new Error("Unterminated quoted string in gnuplot source.");
        index++;
        tokens.push({value, start, end: index, quoted: true});
      } else if (/[\w$]/.test(char)) {
        while (index < source.length && /[\w$]/.test(source[index])) index++;
        tokens.push({value: source.slice(start, index), start, end: index, quoted: false});
      } else {
        index++;
        tokens.push({value: char, start, end: index, quoted: false});
      }
      lineStart = char === "\n";
    }

    const references = [];
    let plotting = false;
    let operand = false;
    let depth = 0;
    let statement = true;
    let rangeDepth = 0;
    for (let n = 0; n < tokens.length; n++) {
      const token = tokens[n];
      if (!token.quoted && (token.value === "\n" || token.value === ";")) {
        plotting = false;
        operand = false;
        statement = true;
        depth = 0;
        rangeDepth = 0;
        continue;
      }
      if (statement) {
        plotting = !token.quoted && ["plot", "splot"].includes(token.value.toLowerCase());
        operand = plotting;
        statement = false;
        if (plotting) continue;
      }
      if (!plotting) continue;
      if (operand && !token.quoted && token.value === "[") {
        rangeDepth++;
        continue;
      }
      if (rangeDepth) {
        if (!token.quoted && token.value === "]") rangeDepth--;
        continue;
      }
      if (operand) {
        operand = false;
        if (token.quoted && token.value && !["-", "+", "++"].includes(token.value)) {
          const next = tokens[n + 1];
          if (next && !next.quoted && [".", "+", "?", "["].includes(next.value))
            throw new Error("Computed data filenames are not supported. Use a literal relative filename.");
          references.push({path: token.value, start: token.start, end: token.end});
        }
      }
      if (!token.quoted && ["(", "[", "{"].includes(token.value)) depth++;
      if (!token.quoted && [")", "]", "}"].includes(token.value)) depth--;
      if (!token.quoted && token.value === "," && depth === 0) operand = true;
    }
    return references;
  }

  function validateDataPath(path) {
    if (!path || path.startsWith("/") || path.startsWith("~") || path.startsWith("<") ||
        /[\\:\x00-\x1f\x7f]/.test(path) || path.split("/").includes(".."))
      throw new Error("Data files must use relative paths below this Markdown document.");
    if (!/\.(csv|dat|txt)$/i.test(path))
      throw new Error("Only .csv, .dat, and .txt data files are supported.");
  }

  function mountedSource(source, references, names) {
    let result = source;
    for (const reference of [...references].reverse()) {
      result = result.slice(0, reference.start) +
        "\"" + names.get(reference.path) + "\"" + result.slice(reference.end);
    }
    return result;
  }

  async function prepare(source) {
    const references = dataReferences(source);
    const paths = [...new Set(references.map((reference) => reference.path))];
    if (paths.length > MAX_FILES)
      throw new Error(`A plot can reference at most ${MAX_FILES} data files.`);
    for (const path of paths) validateDataPath(path);

    const names = new Map(paths.map((path, index) => [path, `data-${index}.dat`]));
    const files = [];
    let total = 0;
    for (const path of paths) {
      if (!WORLD_HANDLER?.markerGnuplotData)
        throw new Error("Gnuplot data loading is unavailable.");
      const encoded = await WORLD_HANDLER.markerGnuplotData.postMessage(path);
      if (typeof encoded !== "string") throw new Error("Unable to read plot data.");
      const binary = atob(encoded);
      const bytes = Uint8Array.from(binary, (char) => char.charCodeAt(0));
      total += bytes.length;
      if (bytes.length > MAX_FILE_BYTES || total > MAX_TOTAL_BYTES)
        throw new Error("Plot data exceeds input limits.");
      files.push({name: names.get(path), bytes, displayName: path});
    }
    return {source: mountedSource(source, references, names), files};
  }

  function createRuntime() {
    const blobUrl = URL.createObjectURL(new Blob([workerSource], {type: "text/javascript"}));
    const worker = new Worker(blobUrl, {type: "module"});
    let stopped = false;
    let nextId = 0;

    const ready = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        stopped = true;
        worker.terminate();
        URL.revokeObjectURL(blobUrl);
        reject(new Error("Gnuplot initialization timed out."));
      }, 10_000);
      const onMessage = ({data}) => {
        if (data.type !== "ready" && data.type !== "startup-error") return;
        clearTimeout(timer);
        worker.removeEventListener("message", onMessage);
        URL.revokeObjectURL(blobUrl);
        if (data.type === "ready") resolve();
        else {
          stopped = true;
          worker.terminate();
          reject(new Error(data.error || "WASM initialization failed."));
        }
      };
      worker.addEventListener("message", onMessage);
      worker.addEventListener("error", (event) => {
        clearTimeout(timer);
        stopped = true;
        URL.revokeObjectURL(blobUrl);
        reject(new Error(event.message || "Gnuplot worker failed to start."));
      }, {once: true});
    });
    return {
      async render(input) {
        await ready;
        if (stopped) throw new Error("Gnuplot runtime stopped.");
        const id = ++nextId;
        return await new Promise((resolve, reject) => {
          const timer = setTimeout(() => {
            stopped = true;
            worker.terminate();
            reject(new Error("Gnuplot exceeded the 3 second render limit."));
          }, 3_000);
          const onMessage = ({data}) => {
            if (data.type !== "result" || data.id !== id) return;
            clearTimeout(timer);
            worker.removeEventListener("message", onMessage);
            if (data.success) resolve(data.result);
            else reject(new Error(data.result || "Gnuplot rendering failed."));
          };
          worker.addEventListener("message", onMessage);
          worker.postMessage({id, ...input});
        });
      },
      dispose() {
        stopped = true;
        worker.terminate();
        URL.revokeObjectURL(blobUrl);
      },
    };
  }

  const allowedElements = new Set(
    "svg g defs path rect circle ellipse line polyline polygon text tspan title desc use clipPath pattern linearGradient radialGradient stop filter feFlood feComposite feBlend".split(" ")
  );
  const allowedAttributes = new Set(
    "id x y x1 y1 x2 y2 dx dy cx cy r rx ry width height viewBox preserveAspectRatio d points transform fill fill-opacity fill-rule stroke stroke-width stroke-opacity stroke-linecap stroke-linejoin stroke-dasharray stroke-dashoffset opacity font-family font-size font-weight font-style text-anchor dominant-baseline textLength lengthAdjust rotate clip-path clipPathUnits patternUnits patternTransform patternContentUnits gradientUnits gradientTransform offset stop-color stop-opacity filter filterUnits result in in2 operator mode flood-color flood-opacity color xmlns xmlns:xlink href xlink:href".split(" ")
  );
  const svgNamespace = "http://www.w3.org/2000/svg";

  function sanitizeSvg(source, displayNames) {
    if (source.length > MAX_OUTPUT_BYTES || /<!DOCTYPE|<!ENTITY/i.test(source))
      throw new Error("Unsafe or oversized SVG output.");
    const parsed = new DOMParser().parseFromString(source, "image/svg+xml");
    const root = parsed.documentElement;
    if (!root || root.tagName !== "svg" || root.namespaceURI !== svgNamespace ||
        parsed.querySelector("parsererror"))
      throw new Error("Gnuplot did not produce a valid SVG plot.");

    let textBytes = 0;
    const clean = (element) => {
      for (const attribute of [...element.attributes]) {
        const value = attribute.value;
        const localReference = /^#[A-Za-z_][\w:.-]*$/.test(value);
        const localUrl = /^url\(#[A-Za-z_][\w:.-]*\)$/.test(value);
        if (!allowedAttributes.has(attribute.name) ||
            ((attribute.name === "href" || attribute.name === "xlink:href") && !localReference) ||
            (/url\s*\(/i.test(value) && !localUrl) ||
            ((/[\\]|javascript:|data:|https?:|\/\//i.test(value)) && !attribute.name.startsWith("xmlns")))
          element.removeAttributeNode(attribute);
      }
      for (const child of [...element.childNodes]) {
        if (child.nodeType === Node.ELEMENT_NODE) {
          if (child.namespaceURI !== svgNamespace || !allowedElements.has(child.tagName))
            element.removeChild(child);
          else
            clean(child);
        } else if (child.nodeType !== Node.TEXT_NODE) {
          element.removeChild(child);
        } else if (child.nodeValue) {
          textBytes += encoder.encode(child.nodeValue).length;
          child.nodeValue = child.nodeValue.replace(/data-\d+\.dat/g, (name) =>
            displayNames.get(name) || name);
          if (textBytes > MAX_OUTPUT_BYTES)
            throw new Error("Unsafe or oversized SVG output.");
        }
      }
    };
    clean(root);

    const prefix = `gp-${session}-${++occurrence}-`;
    const withRoot = [root, ...root.querySelectorAll("*")];
    for (const element of withRoot) {
      if (element.hasAttribute("id"))
        element.setAttribute("id", prefix + element.getAttribute("id"));
    }
    for (const element of withRoot) {
      for (const name of ["href", "xlink:href"]) {
        const value = element.getAttribute(name);
        if (value?.startsWith("#")) element.setAttribute(name, "#" + prefix + value.slice(1));
      }
      for (const attribute of [...element.attributes]) {
        if (/^url\(#/.test(attribute.value))
          element.setAttribute(attribute.name, attribute.value.replace(/^url\(#/, `url(#${prefix}`));
      }
    }
    if (encoder.encode(new XMLSerializer().serializeToString(root)).length > MAX_OUTPUT_BYTES)
      throw new Error("Unsafe or oversized SVG output.");
    return root;
  }

  function showError(code, error) {
    const container = document.createElement("div");
    container.className = "gnuplot-error";
    container.setAttribute("role", "alert");
    const pre = document.createElement("pre");
    pre.textContent = message(error);
    container.append(pre);
    renderedPlots.push(container);
    code.closest("pre")?.replaceWith(container);
  }

  function showPlot(code, svg) {
    const container = document.createElement("div");
    container.className = "gnuplot-preview";
    container.setAttribute("role", "img");
    container.setAttribute("aria-label", "Gnuplot plot");
    container.append(document.importNode(svg, true));
    renderedPlots.push(container);
    code.closest("pre")?.replaceWith(container);
  }

  async function renderPlots() {
    const blocks = [...document.querySelectorAll("pre > code.language-gnuplot")];
    if (!blocks.length) return;
    let runtime;
    try {
      for (const code of blocks) {
        try {
          const source = code.textContent || "";
          if (encoder.encode(source).length > MAX_SOURCE_BYTES)
            throw new Error("Gnuplot source exceeds the 64 KB limit.");
          checkOutputPolicy(source);
          const input = await prepare(source);
          if (!runtime) runtime = createRuntime();
          const output = await runtime.render(input);
          const displayNames = new Map(input.files.map((file) => [file.name, file.displayName]));
          showPlot(code, sanitizeSvg(output, displayNames));
        } catch (error) {
          showError(code, error);
        }
      }
    } finally {
      runtime?.dispose();
    }
  }

  globalThis.markerGnuplotExport = (encodedHtml) => {
    const binary = atob(encodedHtml);
    const bytes = Uint8Array.from(binary, (char) => char.charCodeAt(0));
    const html = new TextDecoder().decode(bytes);
    const target = new DOMParser().parseFromString(html, "text/html");
    const originals = [...target.querySelectorAll("pre > code.language-gnuplot")]
      .map((code) => code.closest("pre"));
    if (originals.length !== renderedPlots.length)
      throw new Error(`Gnuplot export found ${renderedPlots.length} rendered plots for ${originals.length} source blocks.`);
    originals.forEach((original, index) => {
      const replacement = renderedPlots[index].cloneNode(true);
      original.replaceWith(target.importNode(replacement, true));
    });
    return "<!DOCTYPE html>\n" + target.documentElement.outerHTML;
  };

  void (async () => {
    try {
      await renderPlots();
    } catch (error) {
      console.error(message(error));
    } finally {
      try { await WORLD_HANDLER?.markerGnuplotDone?.postMessage(true); }
      catch (_) { /* The page may have been replaced. */ }
    }
  })();
})();
