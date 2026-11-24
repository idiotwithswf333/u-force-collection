/* gcc -O2 -Isrc tests/adpcm_test.c src/adpcm.c -o adpcm_test
 * ./adpcm_test loading.wav reference.s16le  -> compares sample by sample */
#include <stdio.h>
#include <stdlib.h>
#include "adpcm.h"

static uint8_t *slurp(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb"); uint8_t *b; if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(*n); fread(b, 1, *n, f); fclose(f); return b;
}

int main(int argc, char **argv)
{
    size_t wn, rn, i, total = 0, diffs = 0, maxd = 0;
    uint8_t *w, *r; static AdpcmStream s; int rc;
    if (argc < 3) return 2;
    w = slurp(argv[1], &wn); r = slurp(argv[2], &rn);
    if (!w || !r) { puts("read fail"); return 1; }
    rc = adpcm_open(&s, w, wn);
    printf("open rc=%d ch=%u rate=%u blockAlign=%u spb=%u blocks=%u\n", rc, s.channels,
           s.sampleRate, s.blockAlign, s.samplesPerBlock, s.blockCount);
    if (rc) return 1;
    {
        const int16_t *ref = (const int16_t *)r;
        size_t refFrames = rn / (2 * s.channels), got;
        static int16_t buf[1000 * 2];
        while ((got = adpcm_read(&s, buf, 1000, 0)) > 0) {
            for (i = 0; i < got * s.channels && total + i < refFrames * s.channels; i++) {
                long d = (long)buf[i] - (long)ref[total + i]; if (d < 0) d = -d;
                if (d) { diffs++; if ((size_t)d > maxd) maxd = (size_t)d; }
            }
            total += got * s.channels;
        }
        printf("decoded %zu frames (ref %zu), differing samples %zu, max diff %zu\n",
               total / s.channels, refFrames, diffs, maxd);
    }
    return diffs ? 1 : 0;
}
