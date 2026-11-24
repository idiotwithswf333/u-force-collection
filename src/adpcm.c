#include "adpcm.h"
#include <string.h>

static const int16_t kStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};
static const int8_t kIndex[16] = { -1, -1, -1, -1, 2, 4, 6, 8,
                                   -1, -1, -1, -1, 2, 4, 6, 8 };

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int adpcm_open(AdpcmStream *s, const uint8_t *wav, size_t size)
{
    size_t pos = 12;
    int haveFmt = 0, haveData = 0;
    uint32_t dataBytes = 0;

    memset(s, 0, sizeof *s);
    if (size < 12 || memcmp(wav, "RIFF", 4) || memcmp(wav + 8, "WAVE", 4)) return -1;

    while (pos + 8 <= size) {
        uint32_t id = le32(wav + pos), len = le32(wav + pos + 4);
        const uint8_t *body = wav + pos + 8;
        if (!memcmp(wav + pos, "fmt ", 4)) {
            if (len < 20 || pos + 8 + len > size) return -2;
            if (le16(body) != 0x0011) return -3;               /* IMA ADPCM only */
            s->channels        = le16(body + 2);
            s->sampleRate      = le32(body + 4);
            s->blockAlign      = le16(body + 12);
            s->samplesPerBlock = le16(body + 18);
            haveFmt = 1;
        } else if (!memcmp(wav + pos, "data", 4)) {
            s->blocks = body;
            dataBytes = len;
            if (pos + 8 + len > size) dataBytes = (uint32_t)(size - pos - 8);
            haveData = 1;
            break;
        }
        (void)id;
        pos += 8 + len + (len & 1);
    }
    if (!haveFmt || !haveData) return -4;
    if (s->channels < 1 || s->channels > ADPCM_MAX_CHANNELS) return -5;
    if (s->blockAlign < 4u * s->channels || s->samplesPerBlock == 0 ||
        s->samplesPerBlock > ADPCM_MAX_BLOCK_SAMPLES) return -6;
    s->blockCount = dataBytes / s->blockAlign;
    if (s->blockCount == 0) return -7;
    return 0;
}

static void decode_block(AdpcmStream *s, uint32_t b)
{
    const uint8_t *p = s->blocks + (size_t)b * s->blockAlign;
    const uint32_t ch = s->channels;
    int pred[ADPCM_MAX_CHANNELS], idx[ADPCM_MAX_CHANNELS];
    uint32_t c, i, frame;

    for (c = 0; c < ch; c++) {
        pred[c] = (int16_t)le16(p + 4 * c);
        idx[c]  = p[4 * c + 2];
        if (idx[c] > 88) idx[c] = 88;
        s->pcm[c] = (int16_t)pred[c];            /* frame 0 = header predictor */
    }
    p += 4 * ch;

    frame = 1;
    /* then groups of 4 bytes (8 samples) per channel, channels interleaved */
    for (i = 0; frame < s->samplesPerBlock &&
                (size_t)(p - (s->blocks + (size_t)b * s->blockAlign)) + 4 * ch <= s->blockAlign;
         i++) {
        for (c = 0; c < ch; c++) {
            uint32_t k;
            for (k = 0; k < 4; k++) {
                uint8_t byte = p[4 * c + k];
                int n;
                for (n = 0; n < 2; n++) {
                    int nib = (n == 0) ? (byte & 0x0F) : (byte >> 4);
                    int step = kStep[idx[c]];
                    int diff = ((2 * (nib & 7) + 1) * step) >> 3;
                    uint32_t f = frame + k * 2 + (uint32_t)n;
                    if (nib & 8) pred[c] -= diff; else pred[c] += diff;
                    if (pred[c] > 32767) pred[c] = 32767;
                    if (pred[c] < -32768) pred[c] = -32768;
                    idx[c] += kIndex[nib];
                    if (idx[c] < 0) idx[c] = 0;
                    if (idx[c] > 88) idx[c] = 88;
                    if (f < s->samplesPerBlock) s->pcm[f * ch + c] = (int16_t)pred[c];
                }
            }
        }
        p += 4 * ch;
        frame += 8;
    }
    s->avail = s->samplesPerBlock;
    s->pos = 0;
}

void adpcm_rewind(AdpcmStream *s)
{
    s->block = 0;
    s->pos = 0;
    s->avail = 0;
}

size_t adpcm_read(AdpcmStream *s, int16_t *out, size_t frames, int loop)
{
    size_t done = 0;
    const uint32_t ch = s->channels;

    while (done < frames) {
        size_t n;
        if (s->pos >= s->avail) {
            if (s->block >= s->blockCount) {
                if (!loop) break;
                s->block = 0;
            }
            decode_block(s, s->block++);
        }
        n = s->avail - s->pos;
        if (n > frames - done) n = frames - done;
        memcpy(out + done * ch, s->pcm + (size_t)s->pos * ch, n * ch * sizeof(int16_t));
        s->pos += (uint32_t)n;
        done += n;
    }
    return done;
}
