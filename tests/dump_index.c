/* PC-side check:
 *   gcc -O2 -Isrc tests/dump_index.c src/msfc_index.c -o dump_index
 *   ./dump_index MSFC.data      (the first 64 KB of the file is enough too)
 */
#include <stdio.h>
#include <stdlib.h>
#include "msfc_index.h"

int main(int argc, char **argv)
{
    uint8_t hb[MSFC_HEADER_BYTES];
    MsfcHeader h;
    unsigned long long maxsz = 0, sumStored = 0;
    const char *maxname = "";
    uint8_t *idx;
    FILE *f;
    int rc;
    uint32_t i;

    if (argc < 2) { fprintf(stderr, "usage: %s MSFC.data\n", argv[0]); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }

    if (fread(hb, 1, sizeof hb, f) != sizeof hb) return 1;
    rc = msfc_parse_header(hb, sizeof hb, &h);
    if (rc) { printf("header: %s\n", msfc_strerror(rc)); return 1; }

    idx = malloc(h.dataStart);
    fseek(f, 0, SEEK_SET);
    if (fread(idx, 1, h.dataStart, f) != h.dataStart) { puts("short index"); return 1; }

    printf("volumes: %u, index bytes: %u\n", h.count, h.dataStart);
    for (i = 0; i < h.count; i++) {
        MsfcEntry e;
        rc = msfc_get_entry(idx, h.dataStart, &h, i, &e);
        if (rc) { printf("entry %u: %s\n", i, msfc_strerror(rc)); return 1; }
        printf("%3u %-36s off=%-10llu stored=%-10llu size=%-10llu flag=%u\n", i, e.name,
               (unsigned long long)e.offset, (unsigned long long)e.storedSize,
               (unsigned long long)e.size, e.flags);
        sumStored += e.storedSize;
        if (e.size > maxsz) { maxsz = e.size; maxname = e.name; }
    }
    printf("sum stored=%llu (+index %u = %llu)\nlargest uncompressed: %s = %llu bytes\n",
           sumStored, h.dataStart, sumStored + h.dataStart, maxname, maxsz);
    return 0;
}
