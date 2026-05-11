# MiniLang — A Mini Browser Engine in C++

> **Write HTML-like code. Watch it paint on screen. Understand how browsers actually work.**

---

## What Is This?

MiniLang is a tiny interpreter engine written in **pure C++ using the Win32 API**.

You type markup code into a text box, press **Run**, and the engine parses it and **draws the result directly onto the screen** — no browser, no web engine, no library. Just raw C++ painting pixels based on your code.

The goal is not to build a real browser. The goal is to **show you exactly what a browser does internally**:

1. Read your text (HTML)
2. Parse the tags into structured data (the DOM)
3. Walk that structure and **paint each element onto the screen**

MiniLang does all three steps — in about 600 lines of C++ you can read and understand in an afternoon.

---

## Why I Built This

Most people learn HTML by writing it in a text editor and opening it in Chrome. It works — but Chrome is a 35 million line black box. You have no idea what it actually did with your code.

I wanted to answer: **what is the minimum code needed to go from `<h1>Hello</h1>` to pixels on screen?**

The answer is this project. It is deliberately simple so every line is explainable.

---

## How a Real Browser Works (and How MiniLang Mirrors It)

```
Your HTML text
      │
      ▼
 [ Tokenizer ]        — splits "<h1>Hello</h1>" into tokens
      │
      ▼
 [ Parser ]           — builds a tree of elements (the DOM)
      │
      ▼
 [ Layout Engine ]    — decides where each element goes on screen
      │
      ▼
 [ Paint / Render ]   — draws rectangles, text, lines onto pixels
      │
      ▼
   Screen
```

MiniLang has all four stages — just much simpler than a real browser:

| Stage | Real Browser | MiniLang |
|---|---|---|
| Tokenizer | Handles 100+ edge cases | Simple `<` / `>` scan |
| Parser | Full HTML5 spec | Linear tag matching |
| Layout | CSS box model, floats, flexbox | Top-to-bottom flow |
| Paint | GPU-accelerated compositing | Win32 `DrawText` + `GDI` |

---

## The Mini Language

MiniLang understands a small set of HTML-like tags. Here is every tag it supports:

| Tag | What it renders |
|---|---|
| `<h1>` to `<h6>` | Headings, sized from 36px down to 16px |
| `<p>text</p>` | A paragraph of body text |
| `<b>text</b>` | Bold text |
| `<i>text</i>` | Italic text |
| `<u>text</u>` | Underlined text |
| `<color value="red">text</color>` | Colored text (named or `#RRGGBB`) |
| `<highlight value="yellow">text</highlight>` | Text with a colored background |
| `<button>label</button>` | A blue button-shaped block |
| `<link>text</link>` | Blue underlined link style |
| `<hr>` | A horizontal dividing line |
| `<br>` | A blank line gap |

Plain text with no tags is rendered as a normal paragraph.

---

## How to Build

### Requirements

- Windows 10 or 11
- MinGW-w64 **or** Visual Studio with the C++ workload

### With MinGW (g++)

```bash
g++ MiniLang.cpp -o MiniLang.exe -mwindows -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++
```

### With MSVC (Visual Studio Developer Command Prompt)

```bash
cl MiniLang.cpp /link user32.lib gdi32.lib comctl32.lib
```

### Cross-compile from Linux / GitHub Codespace

```bash
sudo apt-get install -y mingw-w64
x86_64-w64-mingw32-g++ MiniLang.cpp -o MiniLang.exe -mwindows -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++
```

No third-party libraries needed. Everything used ships with Windows and your compiler.

---

## How to Use

1. Run `MiniLang.exe`
2. The top box is your **code editor** — type your markup there
3. Press **▶ Run** (or `Ctrl + Enter`)
4. The output renders live in the panel below
5. Press **✕ Clear** to start fresh

---

## Examples to Try

Copy any of these into the input box and press Run.

---

### Example 1 — The basics

```
<h1>My First Page</h1>
<p>This is a paragraph of text rendered by a C++ engine.</p>
<hr>
<p>No browser was involved in making this.</p>
```

---

### Example 2 — Text styling

```
<h2>Text Styles</h2>
<b>This is bold.</b>
<br>
<i>This is italic.</i>
<br>
<u>This is underlined.</u>
<br>
<b><i>Bold and italic together.</i></b>
```

---

### Example 3 — Color

```
<h2>Colors</h2>
<color value="red">Red text using a named color.</color>
<br>
<color value="#1a73e8">Blue text using a hex code.</color>
<br>
<highlight value="yellow">Highlighted like a marker pen.</highlight>
<br>
<highlight value="#d4edda">Custom green highlight.</highlight>
```

Supported named colors: `red`, `green`, `blue`, `yellow`, `orange`, `purple`, `pink`, `cyan`, `white`, `black`, `gray`

---

### Example 4 — A simple webpage layout

```
<h1>About Me</h1>
<hr>
<h2>Who I Am</h2>
<p>A developer who wanted to understand how browsers work from the inside.</p>
<h2>What I Built</h2>
<p>A mini rendering engine in C++ using only the Win32 API.</p>
<hr>
<h2>Contact</h2>
<link>github.com/yourusername</link>
<br>
<button>Download My Resume</button>
```

---

### Example 5 — A fake product card

```
<h1>SuperWidget Pro</h1>
<hr>
<color value="green"><b>In Stock</b></color>
<br>
<p>The best widget you will ever buy. Guaranteed or your money back.</p>
<highlight value="#fff3cd">Limited time offer — 20% off today only!</highlight>
<br>
<button>Add to Cart</button>
<br>
<link>View full details</link>
```

---

### Example 6 — Heading scale (shows the h1–h6 size system)

```
<h1>Heading Level 1</h1>
<h2>Heading Level 2</h2>
<h3>Heading Level 3</h3>
<h4>Heading Level 4</h4>
<h5>Heading Level 5</h5>
<h6>Heading Level 6</h6>
<hr>
<p>Body text sits below all headings at 16px.</p>
```

---

## How the Code Works

The source is one file: `MiniLang.cpp`. It has four clear sections:

### 1. The Parser (`ParseCode`)

```cpp
void ParseCode(const std::wstring& code) { ... }
```

This function walks the input string character by character looking for `<` and `>`. When it finds a tag it:

- Reads the tag name (`h1`, `b`, `color`, etc.)
- Reads any attributes (`value="red"`)
- Finds the matching closing tag
- Extracts the inner text
- Creates a `RenderElement` struct and pushes it into a list

That list is the engine's equivalent of the DOM — a structured representation of what to draw.

### 2. The RenderElement (the DOM node)

```cpp
struct RenderElement {
    enum Type { TEXT, HEADING, BOLD, ITALIC, ... } type;
    std::wstring text;
    COLORREF color;
    int fontSize;
    bool bold, italic, underline;
    ...
};
```

Every tag becomes one of these structs. The renderer only looks at this list — it never touches the original string again. This is exactly how a real browser separates parsing from rendering.

### 3. The Layout + Paint Engine (`CanvasProc / WM_PAINT`)

```cpp
case WM_PAINT: {
    for (auto& el : g_elements) {
        // choose font based on el.bold, el.italic, el.fontSize
        // call DrawText() to measure and paint the text
        // advance y position downward
    }
}
```

This is the layout engine. It loops through every element, creates the right font, calls Win32's `DrawText` to measure the text height, paints it, then moves `y` down so the next element starts below. Top-to-bottom flow — the simplest possible layout model.

A real browser's layout engine does the same thing but handles columns, floats, flexbox, grid, margins, and thousands of other CSS rules. The concept is identical.

### 4. The Win32 Window (WinMain + MainWndProc)

The standard Win32 plumbing: create a window, an edit control for input, buttons, and a custom canvas. The canvas is its own registered window class with its own `WndProc` that handles `WM_PAINT` (draw), `WM_VSCROLL` (scroll), and `WM_MOUSEWHEEL` (mouse scroll).

---

## What This Project Is NOT

- Not a real browser (no CSS, no JavaScript, no networking)
- Not production code (no error recovery, no full HTML5 compliance)
- Not a rendering benchmark

It is a **learning tool** — the smallest possible thing that shows the core idea.

---

## Things You Could Add

If you want to extend it, here are some ideas ordered by difficulty:

- `<small>` and `<big>` tags — just change `el.fontSize`
- `<center>` alignment — pass `DT_CENTER` flag to `DrawText`
- Background color for the whole page — paint a `FillRect` before the loop
- `<img src="...">` — load a bitmap with `LoadImage` and `BitBlt` it
- A proper font size attribute — parse `size="24"` from the tag
- Multiple fonts — parse `font="Georgia"` and set `lfFaceName`
- A real tokenizer that handles nested tags — the current one is linear

---

## License

MIT — use it, break it, learn from it, build on it.

---

## Author

Built to answer one question: *what is the least amount of code needed to turn text into pixels?*

Turns out the answer fits in a single `.cpp` file.
