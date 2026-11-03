/* wave_lcd.h: read the Wave's 480 x 64 graphic LCD back as text, for the MPC screen (which can show live text but no live picture).
 *
 * The firmware draws its text with two bitmap fonts kept in the OS file itself (w2sys.bin, OS 1.700): a 5 x 7 font in 6 x 8 cells
 * (8 bytes a glyph from 0x47eec) and a bold font in 8 x 10 cells (10 bytes a glyph from 0x482ac), both indexed by ASCII code. The
 * decoder loads them from the user's own OS file (nothing of the font ships) and finds every glyph on the screen by exact match of its
 * whole cell, ink on paper or inverted. Matches that overlap a better one are dropped (box borders look like '_' and the like).
 * The result is laid out on the Wave's own eight columns (60 px each) and up to eight text lines, top to bottom: cells[line][column],
 * a run of text in the column it starts in.
 * Boxes, lines and inversion are not kept. */
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace wavelcd {

constexpr int W = 480, H = 64, COLS = 8, LINES = 8, COLW = W / COLS;

struct Font {
    int w = 0, h = 0;                                     // cell size in pixels (w <= 8)
    std::unordered_map<uint64_t, char> glyphs;            // pack(cell rows) -> character
    bool ok() const { return !glyphs.empty(); }
};

inline uint64_t pack(const uint8_t *rows, int h) {        // the cell's rows (any height) -> FNV-1a, with the height
    uint64_t k = 1469598103934665603ull ^ (uint64_t)h;
    for (int r = 0; r < h; r++) k = (k ^ rows[r]) * 1099511628211ull;
    return k;
}

inline Font load_font(const std::vector<uint8_t> &rom, size_t base, int stride, int w, int h) {
    Font f;
    f.w = w;
    f.h = h;
    const uint8_t mask = (uint8_t)(0xff << (8 - w));
    for (int c = 33; c < 127; c++) {
        if (c == '_' || c == '-' || c == '|' || c == '.' || c == ',' || c == '\'' || c == '`') continue;   // line fragments: boxes
        const size_t o = base + (size_t)c * stride;
        if (o + h > rom.size()) return Font{};
        uint8_t rows[16];
        int ink = 0;
        for (int r = 0; r < h; r++) { rows[r] = rom[o + r] & mask; ink += __builtin_popcount(rows[r]); }
        if (ink < 3) continue;
        f.glyphs.emplace(pack(rows, h), (char)c);
    }
    return f;
}

struct Decoder {
    Font small, bold;
    bool ok() const { return small.ok() && bold.ok(); }

    bool load(const std::vector<uint8_t> &os) {           // OS 1.700's w2sys.bin
        if (os.size() < 0x48a00) return false;
        small = load_font(os, 0x47eec, 8, 6, 8);
        bold = load_font(os, 0x482ac, 10, 8, 10);
        // sanity: 'O' and 'P' as the 1.700 fonts draw them
        const uint8_t o[8] = {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0};
        auto it = small.glyphs.find(pack(o, 8));
        if (it == small.glyphs.end() || it->second != 'O') { small = Font{}; bold = Font{}; return false; }
        return ok();
    }

    struct Hit { int x, y, w, h, ink; char c; };

    /* px: 480 x 64, one byte per pixel (nonzero = lit). Returns cells[line][column]. Line r is the LCD's pixel rows 8r..8r+7. */
    std::array<std::array<std::string, COLS>, LINES> decode(const uint8_t *px) const {
        Screen sc = decode_screen(px);
        return sc.text;
    }

    enum : uint8_t { S_INV = 1, S_LEFT = 2, S_RIGHT = 4, S_TOP = 8, S_BOTTOM = 16 };
    struct Screen {
        std::array<std::array<std::string, COLS>, LINES> text{};
        std::array<std::array<uint8_t, COLS>, LINES> style{};      // S_* bits: the cell's background as the firmware drew it
        bool operator!=(const Screen &o) const { return text != o.text || style != o.style; }
    };

    Screen decode_screen(const uint8_t *px) const {
        Screen sc;
        std::vector<Hit> hits;
        auto pixel = [&](int x, int y) -> bool { return x >= 0 && x < W && y >= 0 && y < H && px[y * W + x]; };
        auto scan = [&](const Font &f) {
            uint8_t rows[16];
            for (int inv = 0; inv < 2; inv++)
                for (int y = 0; y + f.h <= H + 1; y++)
                    for (int x = -1; x + f.w <= W + 1; x++) {
                        int ink = 0;
                        for (int r = 0; r < f.h; r++) {
                            uint8_t v = 0;
                            for (int k = 0; k < f.w; k++)
                                if (pixel(x + k, y + r) != (inv != 0)) v |= (uint8_t)(0x80 >> k);
                            rows[r] = v;
                            ink += __builtin_popcount(v);
                        }
                        if (ink < 3) continue;
                        auto it = f.glyphs.find(pack(rows, f.h));
                        if (it != f.glyphs.end()) hits.push_back({x, y, f.w, f.h, ink + (f.h > 8 ? 10000 : 0), it->second});
                    }
        };
        scan(bold);
        scan(small);
        // keep the best of overlapping matches (bold first, then the inkier)
        std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.ink > b.ink; });
        std::vector<Hit> kept;
        for (const Hit &h : hits) {
            bool clash = false;
            for (const Hit &k : kept)
                if (h.x < k.x + k.w - 1 && k.x < h.x + h.w - 1 && h.y < k.y + k.h - 1 && k.y < h.y + h.h - 1) { clash = true; break; }
            if (!clash) kept.push_back(h);
        }
        // lines: the LCD's 8-pixel text rows, by each glyph's middle (the bold font's 10-pixel cells fall in one of them)
        std::vector<std::vector<Hit>> lines(LINES);
        for (const Hit &h : kept) lines[std::min(LINES - 1, std::max(0, (h.y + h.h / 2) / 8))].push_back(h);
        auto &out = sc.text;
        for (size_t li = 0; li < lines.size() && li < (size_t)LINES; li++) {
            auto &l = lines[li];
            std::sort(l.begin(), l.end(), [](const Hit &a, const Hit &b) { return a.x < b.x; });
            // a run (glyphs less than three cells apart) stays in the column it starts in: the MPC draws each column's text from
            // its left edge to the end of the line, so a long title runs on over the columns it covers
            int prev_end = -1000, run_col = -1;
            for (const Hit &h : l) {
                const int gap = h.x - prev_end;
                const int col = std::min(COLS - 1, std::max(0, (h.x + 2) / COLW));
                const int prev_col = std::min(COLS - 1, std::max(0, (prev_end - 2) / COLW));
                if (run_col < 0 || gap >= 3 * h.w || (gap >= h.w && col != prev_col)) {   // a far gap, or a space at a column edge
                    run_col = col;
                    if (!out[li][run_col].empty()) out[li][run_col].push_back(' ');
                } else if (gap >= h.w / 2 + 1) {
                    out[li][run_col].append((size_t)std::max(1, (gap + h.w / 2) / h.w), ' ');
                }
                out[li][run_col].push_back(h.c);
                prev_end = h.x + h.w;
            }
        }
        // each cell's background: mostly lit = an inverted bar; a lit edge most of its length = a box side
        for (int r = 0; r < LINES; r++)
            for (int c = 0; c < COLS; c++) {
                const int x0 = c * COLW, y0 = r * 8;
                int lit = 0;
                for (int y = y0; y < y0 + 8; y++)
                    for (int x = x0; x < x0 + COLW; x++) lit += pixel(x, y);
                uint8_t st = lit * 2 > COLW * 8 ? S_INV : 0;
                const bool inv = st & S_INV;
                auto col_lit = [&](int x) { int n = 0; for (int y = y0; y < y0 + 8; y++) n += pixel(x, y) != inv; return n >= 7; };
                auto row_lit = [&](int y) { int n = 0; for (int x = x0; x < x0 + COLW; x++) n += pixel(x, y) != inv; return n >= COLW * 3 / 4; };
                if (inv) { sc.style[r][c] = st; continue; }     // inside a bar, letter strokes look like edges
                if (col_lit(x0) || col_lit(x0 + 1)) st |= S_LEFT;
                if (col_lit(x0 + COLW - 1) || col_lit(x0 + COLW - 2)) st |= S_RIGHT;
                if (row_lit(y0) || row_lit(y0 + 1)) st |= S_TOP;
                if (row_lit(y0 + 7) || row_lit(y0 + 6)) st |= S_BOTTOM;
                sc.style[r][c] = st;
            }
        return sc;
    }
};

}  // namespace wavelcd
