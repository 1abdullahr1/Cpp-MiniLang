// MiniLang - A Win32 C++ App that interprets a custom HTML-like language
// Compile: g++ MiniLang.cpp -o MiniLang.exe -mwindows -lgdi32 -lcomctl32
// Or with MSVC: cl MiniLang.cpp /link user32.lib gdi32.lib comctl32.lib

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

// ─── Control IDs ────────────────────────────────────────────────────────────
#define ID_INPUT_EDIT   101
#define ID_RUN_BUTTON   102
#define ID_CLEAR_BUTTON 103
#define ID_CANVAS       104

// ─── Window handles ─────────────────────────────────────────────────────────
HWND g_hMain    = NULL;
HWND g_hInput   = NULL;
HWND g_hRun     = NULL;
HWND g_hClear   = NULL;
HWND g_hCanvas  = NULL;

// ─── Parsed render elements ─────────────────────────────────────────────────
struct RenderElement {
    enum Type { TEXT, HEADING, BOLD, ITALIC, UNDERLINE, COLOR_TEXT,
                HR, BR, BUTTON_ELEM, LINK, HIGHLIGHT } type;
    std::wstring text;
    COLORREF     color  = RGB(30, 30, 30);
    COLORREF     bgColor= RGB(255,255,255);
    int          level  = 1;   // for headings h1-h6
    bool         bold   = false;
    bool         italic = false;
    bool         underline = false;
    int          fontSize  = 16;
};

std::vector<RenderElement> g_elements;

// ─── Helper: trim whitespace ─────────────────────────────────────────────────
static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    size_t b = s.find_last_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    return s.substr(a, b - a + 1);
}

// ─── Helper: to lower ───────────────────────────────────────────────────────
static std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

// ─── Helper: parse #RRGGBB or named colors ──────────────────────────────────
static COLORREF ParseColor(const std::wstring& c) {
    std::wstring lc = ToLower(Trim(c));
    if (lc == L"red")     return RGB(220, 50, 47);
    if (lc == L"green")   return RGB(0, 160, 0);
    if (lc == L"blue")    return RGB(0, 100, 220);
    if (lc == L"yellow")  return RGB(230, 200, 0);
    if (lc == L"orange")  return RGB(230, 120, 0);
    if (lc == L"purple")  return RGB(130, 0, 200);
    if (lc == L"pink")    return RGB(220, 60, 120);
    if (lc == L"cyan")    return RGB(0, 180, 200);
    if (lc == L"white")   return RGB(255, 255, 255);
    if (lc == L"black")   return RGB(0, 0, 0);
    if (lc == L"gray" || lc == L"grey") return RGB(120, 120, 120);
    if (!lc.empty() && lc[0] == L'#' && lc.size() == 7) {
        int r = wcstol(lc.substr(1,2).c_str(), nullptr, 16);
        int g = wcstol(lc.substr(3,2).c_str(), nullptr, 16);
        int b = wcstol(lc.substr(5,2).c_str(), nullptr, 16);
        return RGB(r, g, b);
    }
    return RGB(30, 30, 30);
}

// ─── Simple tag value extractor: <tag>value</tag> ───────────────────────────
// Returns inner text; sets 'found' to true if tag exists
static bool GetTagContent(const std::wstring& src, const std::wstring& tag,
                          std::wstring& inner)
{
    std::wstring open  = L"<" + tag;
    std::wstring close = L"</" + tag + L">";
    std::wstring srcL  = ToLower(src);
    std::wstring tagL  = ToLower(tag);
    std::wstring openL = L"<" + tagL;
    std::wstring closeL= L"</" + tagL + L">";

    size_t s = srcL.find(openL);
    if (s == std::wstring::npos) return false;
    // find '>'
    size_t gt = srcL.find(L'>', s);
    if (gt == std::wstring::npos) return false;
    size_t e = srcL.find(closeL, gt);
    if (e == std::wstring::npos) return false;
    inner = src.substr(gt + 1, e - gt - 1);
    return true;
}

// ─── Get attribute value from tag: <tag attr="val"> ─────────────────────────
static std::wstring GetAttr(const std::wstring& tagStr, const std::wstring& attr) {
    std::wstring tagL = ToLower(tagStr);
    std::wstring atL  = ToLower(attr) + L"=\"";
    size_t p = tagL.find(atL);
    if (p == std::wstring::npos) return L"";
    size_t vs = p + atL.size();
    size_t ve = tagL.find(L'"', vs);
    if (ve == std::wstring::npos) return L"";
    return tagStr.substr(vs, ve - vs);
}

// ─── Parse the mini-language code into RenderElements ────────────────────────
// Supported tags:
//   <h1>-<h6>text</h1-6>        headings
//   <b>text</b>                  bold
//   <i>text</i>                  italic
//   <u>text</u>                  underline
//   <p>text</p>                  paragraph
//   <br>                         line break
//   <hr>                         horizontal rule
//   <color value="red">text</color>  colored text
//   <highlight value="#ffff00">text</highlight>  highlighted text
//   <button>label</button>       button-like display
//   <link>text</link>            link-styled text
//   plain text                   rendered as normal paragraph

static void ParseCode(const std::wstring& code) {
    g_elements.clear();

    // We do a simple linear scan looking for tags
    std::wstring codeL = ToLower(code);
    size_t pos = 0;
    size_t len = code.size();

    while (pos < len) {
        if (code[pos] == L'<') {
            // find end of tag name
            size_t tagStart = pos + 1;
            bool closing = (tagStart < len && code[tagStart] == L'/');
            if (closing) { tagStart++; }

            // find end of tag
            size_t tagEnd = code.find(L'>', pos);
            if (tagEnd == std::wstring::npos) { pos++; continue; }

            std::wstring fullTag = code.substr(pos, tagEnd - pos + 1);
            std::wstring fullTagL= ToLower(fullTag);

            // self-closing tags
            if (fullTagL == L"<br>" || fullTagL == L"<br/>") {
                RenderElement el; el.type = RenderElement::BR; el.text = L"";
                g_elements.push_back(el);
                pos = tagEnd + 1; continue;
            }
            if (fullTagL == L"<hr>" || fullTagL == L"<hr/>") {
                RenderElement el; el.type = RenderElement::HR; el.text = L"";
                g_elements.push_back(el);
                pos = tagEnd + 1; continue;
            }

            // Extract tag name
            size_t nameEnd = tagStart;
            while (nameEnd < tagEnd && code[nameEnd] != L' ' && code[nameEnd] != L'>') nameEnd++;
            std::wstring tagName = ToLower(code.substr(tagStart, nameEnd - tagStart));

            if (closing) { pos = tagEnd + 1; continue; } // skip closing tags

            // Find matching close tag
            std::wstring closeTag = L"</" + tagName + L">";
            size_t contentStart = tagEnd + 1;
            size_t closePos = ToLower(code).find(closeTag, contentStart);
            std::wstring inner = (closePos != std::wstring::npos)
                ? code.substr(contentStart, closePos - contentStart)
                : L"";
            size_t nextPos = (closePos != std::wstring::npos)
                ? closePos + closeTag.size()
                : tagEnd + 1;

            RenderElement el;
            el.text = Trim(inner);

            if (tagName == L"h1" || tagName == L"h2" || tagName == L"h3" ||
                tagName == L"h4" || tagName == L"h5" || tagName == L"h6") {
                el.type  = RenderElement::HEADING;
                el.level = tagName[1] - L'0';
                el.bold  = true;
                // Font size: h1=36 h2=30 h3=24 h4=20 h5=18 h6=16
                static const int sizes[] = {36,30,24,20,18,16};
                el.fontSize = sizes[el.level - 1];
                el.color = RGB(20, 20, 20);
                g_elements.push_back(el);
            }
            else if (tagName == L"b" || tagName == L"strong") {
                el.type = RenderElement::BOLD; el.bold = true; el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"i" || tagName == L"em") {
                el.type = RenderElement::ITALIC; el.italic = true; el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"u") {
                el.type = RenderElement::UNDERLINE; el.underline = true; el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"p") {
                el.type = RenderElement::TEXT; el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"color") {
                std::wstring val = GetAttr(fullTag, L"value");
                el.type  = RenderElement::COLOR_TEXT;
                el.color = ParseColor(val.empty() ? L"red" : val);
                el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"highlight") {
                std::wstring val = GetAttr(fullTag, L"value");
                el.type   = RenderElement::HIGHLIGHT;
                el.bgColor= ParseColor(val.empty() ? L"yellow" : val);
                el.color  = RGB(20,20,20);
                el.fontSize = 16;
                g_elements.push_back(el);
            }
            else if (tagName == L"button") {
                el.type = RenderElement::BUTTON_ELEM; el.fontSize = 15; el.bold = true;
                el.color = RGB(255,255,255); el.bgColor = RGB(50, 120, 230);
                g_elements.push_back(el);
            }
            else if (tagName == L"link" || tagName == L"a") {
                el.type = RenderElement::LINK; el.underline = true; el.fontSize = 16;
                el.color = RGB(0, 80, 200);
                g_elements.push_back(el);
            }
            else {
                // Unknown tag — just show inner text as plain
                el.type = RenderElement::TEXT; el.fontSize = 16;
                g_elements.push_back(el);
            }

            pos = nextPos;
        }
        else {
            // Plain text until next '<'
            size_t nextTag = code.find(L'<', pos);
            std::wstring plain = (nextTag != std::wstring::npos)
                ? code.substr(pos, nextTag - pos)
                : code.substr(pos);
            plain = Trim(plain);
            if (!plain.empty()) {
                RenderElement el;
                el.type = RenderElement::TEXT;
                el.text = plain;
                el.fontSize = 16;
                el.color = RGB(30, 30, 30);
                g_elements.push_back(el);
            }
            pos = (nextTag != std::wstring::npos) ? nextTag : len;
        }
    }
}

// ─── Canvas WndProc ──────────────────────────────────────────────────────────
static int g_scrollY = 0;
static int g_totalHeight = 0;

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int W = rc.right;

        // Create off-screen buffer
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, W, rc.bottom);
        SelectObject(memDC, memBmp);

        // Background
        HBRUSH bgBrush = CreateSolidBrush(RGB(250, 250, 252));
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        SetBkMode(memDC, TRANSPARENT);

        int x = 40;
        int y = 30 - g_scrollY;
        int margin = 40;
        int maxW = W - margin * 2;

        for (auto& el : g_elements) {
            if (el.type == RenderElement::BR) {
                y += 10;
                continue;
            }
            if (el.type == RenderElement::HR) {
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 210));
                HPEN old = (HPEN)SelectObject(memDC, pen);
                MoveToEx(memDC, x, y + 8, NULL);
                LineTo(memDC, x + maxW, y + 8);
                SelectObject(memDC, old);
                DeleteObject(pen);
                y += 24;
                continue;
            }

            // Create font
            LOGFONT lf = {};
            lf.lfHeight = -el.fontSize;
            lf.lfWeight = el.bold ? FW_BOLD : FW_NORMAL;
            lf.lfItalic = el.italic ? TRUE : FALSE;
            lf.lfUnderline = el.underline ? TRUE : FALSE;
            wcscpy_s(lf.lfFaceName, el.type == RenderElement::HEADING
                ? L"Georgia" : L"Segoe UI");
            HFONT hFont = CreateFontIndirect(&lf);
            HFONT hOld  = (HFONT)SelectObject(memDC, hFont);

            if (el.type == RenderElement::BUTTON_ELEM) {
                // Draw button box
                RECT btnRc = {x, y, x + maxW, y + el.fontSize + 18};
                HBRUSH btnBr = CreateSolidBrush(el.bgColor);
                FillRect(memDC, &btnRc, btnBr);
                DeleteObject(btnBr);
                // Border
                HPEN bPen = CreatePen(PS_SOLID, 1, RGB(30, 80, 180));
                HPEN bOld = (HPEN)SelectObject(memDC, bPen);
                Rectangle(memDC, btnRc.left, btnRc.top, btnRc.right, btnRc.bottom);
                SelectObject(memDC, bOld);
                DeleteObject(bPen);
                SetTextColor(memDC, el.color);
                SetBkMode(memDC, TRANSPARENT);
                DrawText(memDC, el.text.c_str(), -1, &btnRc,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                y += el.fontSize + 28;
            }
            else if (el.type == RenderElement::HIGHLIGHT) {
                RECT textRc = {x, y, x + maxW, y + el.fontSize * 3};
                DrawText(memDC, el.text.c_str(), -1, &textRc,
                    DT_CALCRECT | DT_WORDBREAK);
                HBRUSH hlBr = CreateSolidBrush(el.bgColor);
                FillRect(memDC, &textRc, hlBr);
                DeleteObject(hlBr);
                SetTextColor(memDC, el.color);
                DrawText(memDC, el.text.c_str(), -1, &textRc, DT_WORDBREAK);
                y += textRc.bottom - textRc.top + 10;
            }
            else {
                SetTextColor(memDC, el.color);
                RECT textRc = {x, y, x + maxW, y + el.fontSize * 3 + 60};
                DrawText(memDC, el.text.c_str(), -1, &textRc,
                    DT_CALCRECT | DT_WORDBREAK);
                DrawText(memDC, el.text.c_str(), -1, &textRc, DT_WORDBREAK);

                // Underline decoration for headings (h1, h2)
                if (el.type == RenderElement::HEADING && el.level <= 2) {
                    HPEN dPen = CreatePen(PS_SOLID, 2,
                        el.level == 1 ? RGB(50, 120, 230) : RGB(180, 180, 200));
                    HPEN dOld = (HPEN)SelectObject(memDC, dPen);
                    MoveToEx(memDC, x, textRc.bottom + 4, NULL);
                    LineTo(memDC, x + maxW, textRc.bottom + 4);
                    SelectObject(memDC, dOld);
                    DeleteObject(dPen);
                    y += 8;
                }
                y += textRc.bottom - textRc.top + 12;
            }

            SelectObject(memDC, hOld);
            DeleteObject(hFont);
        }

        g_totalHeight = y + g_scrollY + 30;

        // Blit
        BitBlt(hdc, 0, 0, W, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);

        // Update scrollbar
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin   = 0;
        si.nMax   = g_totalHeight;
        si.nPage  = rc.bottom;
        si.nPos   = g_scrollY;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask  = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wParam)) {
            case SB_LINEUP:   g_scrollY -= 20; break;
            case SB_LINEDOWN: g_scrollY += 20; break;
            case SB_PAGEUP:   g_scrollY -= si.nPage; break;
            case SB_PAGEDOWN: g_scrollY += si.nPage; break;
            case SB_THUMBTRACK: g_scrollY = si.nTrackPos; break;
        }
        g_scrollY = max(0, min(g_scrollY, g_totalHeight));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scrollY -= delta / 3;
        g_scrollY = max(0, g_scrollY);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ─── Run the code from input box ─────────────────────────────────────────────
static void RunCode() {
    int len = GetWindowTextLength(g_hInput);
    if (len == 0) return;
    std::wstring code(len + 1, L'\0');
    GetWindowText(g_hInput, &code[0], len + 1);
    code.resize(len);
    g_scrollY = 0;
    ParseCode(code);
    InvalidateRect(g_hCanvas, NULL, TRUE);
}

// ─── Main WndProc ─────────────────────────────────────────────────────────────
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Input multiline edit
        g_hInput = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 10, 10, hwnd, (HMENU)ID_INPUT_EDIT, NULL, NULL);
        // Set font for input
        HFONT hFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        SendMessage(g_hInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Run button
        g_hRun = CreateWindow(L"BUTTON", L"▶  Run",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)ID_RUN_BUTTON, NULL, NULL);

        // Clear button
        g_hClear = CreateWindow(L"BUTTON", L"✕  Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, hwnd, (HMENU)ID_CLEAR_BUTTON, NULL, NULL);

        // Set button font
        HFONT btnFont = CreateFont(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessage(g_hRun,   WM_SETFONT, (WPARAM)btnFont, TRUE);
        SendMessage(g_hClear, WM_SETFONT, (WPARAM)btnFont, TRUE);

        // Canvas (custom render area)
        WNDCLASS wc2 = {};
        wc2.lpfnWndProc = CanvasProc;
        wc2.hInstance   = GetModuleHandle(NULL);
        wc2.lpszClassName= L"CanvasClass";
        wc2.hCursor     = LoadCursor(NULL, IDC_ARROW);
        wc2.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        RegisterClass(&wc2);

        g_hCanvas = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"CanvasClass", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL,
            0, 0, 10, 10, hwnd, (HMENU)ID_CANVAS, NULL, NULL);

        // Default code hint
        SetWindowText(g_hInput,
            L"<h1>Hello, MiniLang!</h1>\r\n"
            L"<p>This is a paragraph with <b>bold</b> vibes.</p>\r\n"
            L"<hr>\r\n"
            L"<h2>Features</h2>\r\n"
            L"<color value=\"blue\">Blue colored text here.</color>\r\n"
            L"<br>\r\n"
            L"<highlight value=\"yellow\">Highlighted text!</highlight>\r\n"
            L"<br>\r\n"
            L"<button>Click Me</button>\r\n"
            L"<link>Visit anthropic.com</link>"
        );
        return 0;
    }

    case WM_SIZE: {
        int W = LOWORD(lParam);
        int H = HIWORD(lParam);
        int pad = 10;
        int inputH = 200;
        int btnW = 110, btnH = 34;

        // Input box
        MoveWindow(g_hInput, pad, pad, W - pad*2, inputH, TRUE);
        // Buttons
        MoveWindow(g_hRun,   pad,           inputH + pad*2, btnW, btnH, TRUE);
        MoveWindow(g_hClear, pad + btnW+10, inputH + pad*2, btnW, btnH, TRUE);
        // Canvas
        int canvasTop = inputH + pad*2 + btnH + pad;
        MoveWindow(g_hCanvas, pad, canvasTop, W - pad*2, H - canvasTop - pad, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_RUN_BUTTON) {
            RunCode();
        }
        else if (LOWORD(wParam) == ID_CLEAR_BUTTON) {
            SetWindowText(g_hInput, L"");
            g_elements.clear();
            g_scrollY = 0;
            InvalidateRect(g_hCanvas, NULL, TRUE);
        }
        // Ctrl+Enter in edit fires EN_CHANGE; we handle key globally
        return 0;
    }

    case WM_KEYDOWN: {
        // Global Ctrl+Enter
        if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            RunCode();
        }
        return 0;
    }

    case WM_CTLCOLORBTN: {
        // Color run button
        HDC hdcBtn = (HDC)wParam;
        HWND hBtn  = (HWND)lParam;
        if (hBtn == g_hRun) {
            SetTextColor(hdcBtn, RGB(255,255,255));
            SetBkColor(hdcBtn,   RGB(50, 130, 240));
            return (LRESULT)CreateSolidBrush(RGB(50, 130, 240));
        }
        if (hBtn == g_hClear) {
            SetTextColor(hdcBtn, RGB(80,80,80));
            SetBkColor(hdcBtn,   RGB(230,230,235));
            return (LRESULT)CreateSolidBrush(RGB(230,230,235));
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(240, 242, 248));
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return 1;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ─── WinMain ─────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"MiniLangWnd";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassEx(&wc);

    g_hMain = CreateWindowEx(
        0, L"MiniLangWnd",
        L"MiniLang — HTML-like Language Interpreter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        NULL, NULL, hInst, NULL);

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Allow Ctrl+Enter in the edit control to trigger run
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN
            && (GetKeyState(VK_CONTROL) & 0x8000)) {
            RunCode();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
