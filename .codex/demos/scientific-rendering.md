# Scientific rendering

This demonstration opens in **Formatted Markdown** (`Ctrl+5`). The **Source** / **Render** button controls this entire page.

## MathJax

Inline mathematics: $E = mc^2$.

$$
\int_0^1 x^2\,dx = \frac{1}{3}
$$

## Mermaid

```mermaid
graph LR
  A[Markdown source] --> B[Rendered element]
```

## Charter

```charter
width: 640
height: 150
plot
 x range: 1 10 3
 y math: 2*x
```

## gnuplot

```gnuplot
set samples 80
plot sin(x) title 'gnuplot'
```

Switch the page to **Source** to edit without evaluating charts, then choose **Render** to resume.
