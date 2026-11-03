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
    auto t = d.decode(px.data());
    for (auto& line : t) {
        bool any = false; for (auto& c : line) any |= !c.empty();
        if (!any) continue;
        for (auto& c : line) printf("[%-10s]", c.c_str());
        printf("\n");
    }
}
