/* Streaming decoder for IMA ADPCM WAV files (format tag 0x11), as written by
 * ffmpeg's adpcm_ima_wav encoder. Decodes one block at a time on demand, so
 * a few MB of compressed audio costs almost nothing at startup.
 * Output is native-endian signed 16-bit, interleaved. */
#ifndef ADPCM_H
#define ADPCM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADPCM_MAX_CHANNELS 2
#define ADPCM_MAX_BLOCK_SAMPLES 4096

typedef struct {
    const uint8_t *blocks;      /* start of the data chunk                  */
    uint32_t       blockCount;
    uint32_t       blockAlign;  /* bytes per block                          */
    uint32_t       samplesPerBlock;
    uint32_t       sampleRate;
    uint16_t       channels;

    /* playback state */
    uint32_t       block;       /* next block to decode                     */
    uint32_t       pos;         /* next frame to hand out from pcm[]        */
    uint32_t       avail;       /* frames currently decoded in pcm[]        */
    int16_t        pcm[ADPCM_MAX_BLOCK_SAMPLES * ADPCM_MAX_CHANNELS];
} AdpcmStream;

/* Returns 0 on success, negative on a file this decoder cannot play. */
int adpcm_open(AdpcmStream *s, const uint8_t *wav, size_t size);

/* Writes up to `frames` stereo/mono frames to out. When loop is non-zero the
 * stream restarts at the end. Returns frames written (less only if !loop). */
size_t adpcm_read(AdpcmStream *s, int16_t *out, size_t frames, int loop);

void adpcm_rewind(AdpcmStream *s);

#ifdef __cplusplus
}
#endif
#endif
