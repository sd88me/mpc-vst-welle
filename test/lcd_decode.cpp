// Decode an LCD dump (PGM from wave_lcd_dump: 0 = lit) with the fonts of <w2sys.bin>; prints the cells.
#include "wave_lcd.h"
#include <cstdio>
int main(int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <w2sys.bin> <lcd.pgm>\n", argv[0]); return 2; }
    std::vector<uint8_t> os;
    if (FILE* f = fopen(argv[1], "rb")) { int c; while ((c = fgetc(f)) != EOF) os.push_back((uint8_t)c); fclose(f); }
    wavelcd::Decoder d;
    if (!d.load(os)) { printf("fonts not found\n"); return 1; }
    std::vector<uint8_t> px(480 * 64);
    FILE* f = fopen(argv[2], "rb");
    char hdr[32]; if (!fgets(hdr, sizeof hdr, f)) return 1;
    for (auto& p : px) p = fgetc(f) == 0;
    fclose(f);
    auto sc = d.decode_screen(px.data());
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            const int st = sc.style[r][c];
            printf("%c%c%c%c%c%-10.10s", st & 1 ? '#' : ' ', st & 2 ? '|' : ' ', st & 8 ? '^' : ' ', st & 16 ? '_' : ' ', st & 4 ? '|' : ' ', sc.text[r][c].c_str());
        }
        printf("\n");
    }
}
