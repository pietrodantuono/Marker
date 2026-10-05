// SPDX-License-Identifier: GPL-3.0-or-later
// Runs only in the presentation renderer. It never reads/writes the source buffer.
async function markerRichSnapshot(count, token) {
  document.documentElement.style.background = 'transparent';
  Object.assign(document.body.style, {
    padding: '0', margin: '0', maxWidth: 'none', background: 'transparent'
  });
  await Promise.all(Array.from(document.images).map(image => image.complete
    ? Promise.resolve()
    : new Promise(resolve => {
      image.addEventListener('load', resolve, {once: true});
      image.addEventListener('error', resolve, {once: true});
      setTimeout(resolve, 5000);
    })));
  // Offscreen WebKit suspends animation frames. Layout queries below flush layout.
  await new Promise(resolve => setTimeout(resolve, 0));
  const rects = [];
  for (let i = 0; i < count; i++) {
    const start = document.getElementById(`${token}-start-${i}`);
    const end = document.getElementById(`${token}-end-${i}`);
    const nodes = [];
    let rect = {x: 0, y: 0, width: 0, height: 0};
    if (start && end) {
      for (let node = start.nextElementSibling; node && node !== end; node = node.nextElementSibling)
        nodes.push(node);
      const bounds = nodes.map(node => node.getBoundingClientRect()).filter(b => b.width && b.height);
      if (bounds.length) {
        const x = Math.max(0, Math.floor(Math.min(...bounds.map(b => b.left))));
        const y = Math.max(0, Math.floor(Math.min(...bounds.map(b => b.top)) + scrollY));
        rect = {x, y, width: Math.ceil(Math.max(...bounds.map(b => b.right))) - x,
          height: Math.ceil(Math.max(...bounds.map(b => b.bottom)) + scrollY) - y};
      }
    }
    if (!nodes.length || !rect.width || !rect.height) rect.error = 'empty';
    else if (nodes.some(node => node.matches('.gnuplot-error') || node.querySelector('.gnuplot-error')))
      rect.error = 'plot';
    else if (nodes.some(node => Array.from(node.querySelectorAll('img')).some(image => !image.naturalWidth)))
      rect.error = 'image';
    else if (nodes.some(node => node.querySelector('.katex-error')))
      rect.error = 'equation';
    else if (nodes.some(node => node.querySelector(
      'code.language-gnuplot,code.language-mermaid,code.language-math,code.language-tex,code.language-latex')))
      rect.error = 'disabled';
    if (rect.error) rect.width = rect.height = 0;
    rects.push(rect);
  }
  return {rects, width: document.documentElement.scrollWidth,
    pixels: document.documentElement.scrollWidth * document.documentElement.scrollHeight};
}
