/* MSFC for Wii U - stage 1.5: loading screen + music.
 *
 * - Shows the artwork full screen (fade-in, progress bar) and plays the song
 *   while the same checks as stage 1 run: find the SD card, create/open the
 *   MSFC folder, validate MSFC.data and log its volume index.
 * - Log: <sd>/MSFC/msfc_wiiu.log
 * - Nothing is decompressed or launched yet. Press HOME to exit.
 */
#include <SDL.h>
#include <whb/sdcard.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "adpcm.h"
#include "assets.h"
#include "msfc_index.h"

#define SCREEN_W 1280
#define SCREEN_H 720
#define TITLE    "Mighty Switch Force! Collection (Wii U) - stage 1.5"

/* ---- logging: in-memory buffer, saved to the log file once it is known --- */

static char   g_log[96 * 1024];
static size_t g_logLen;
static char   g_logPath[300];

static void save_log(void)
{
    FILE *f;
    if (!g_logPath[0]) return;
    f = fopen(g_logPath, "wb");
    if (!f) return;
    fwrite(g_log, 1, g_logLen, f);
    fclose(f);
}

static void append(const char *line)
{
    size_t n = strlen(line);
    if (g_logLen + n + 2 < sizeof g_log) {
        memcpy(g_log + g_logLen, line, n);
        g_logLen += n;
        g_log[g_logLen++] = '\n';
    }
}

static void LOG(const char *fmt, ...)
{
    char line[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    append(line);
    save_log();
}

static void LOGF(const char *fmt, ...)   /* buffer only, no file rewrite */
{
    char line[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    append(line);
}

/* ---- SD card discovery -------------------------------------------------- */

static int dir_exists(const char *path)
{
    char probe[300];
    DIR *d;
    snprintf(probe, sizeof probe, "%s/", path);
    d = opendir(probe);
    if (!d) return 0;
    closedir(d);
    return 1;
}

static int pick_sd_base(char *out, size_t n)
{
    static const char *candidates[] = { "fs:/vol/external01", "/vol/external01", "sd:" };
    size_t i;
    for (i = 0; i < sizeof candidates / sizeof candidates[0]; i++) {
        if (dir_exists(candidates[i])) {
            snprintf(out, n, "%s", candidates[i]);
            return 1;
        }
    }
    return 0;
}

/* ---- music -------------------------------------------------------------- */

static AdpcmStream      g_music;
static SDL_AudioDeviceID g_audioDev;
static int              g_gain;            /* 0..256, ramps up = fade-in */

static void audio_cb(void *ud, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    size_t frames = (size_t)len / (sizeof(int16_t) * g_music.channels);
    size_t got, total, i;
    int g = g_gain;
    (void)ud;

    got   = adpcm_read(&g_music, out, frames, 1);
    total = got * g_music.channels;
    for (i = 0; i < total; i++) out[i] = (int16_t)(((int)out[i] * g) >> 8);
    if (total * sizeof(int16_t) < (size_t)len)
        memset(out + total, 0, (size_t)len - total * sizeof(int16_t));
    if (g_gain < 256) g_gain = (g + 8 > 256) ? 256 : g + 8;
}

static void start_music(void)
{
    SDL_AudioSpec want, have;
    int rc = adpcm_open(&g_music, msfc_loading_wav,
                        (size_t)(msfc_loading_wav_end - msfc_loading_wav));
    if (rc != 0) { LOG("audio: embedded wav rejected (%d)", rc); return; }

    memset(&want, 0, sizeof want);
    want.freq     = (int)g_music.sampleRate;
    want.format   = AUDIO_S16SYS;
    want.channels = (Uint8)g_music.channels;
    want.samples  = 2048;
    want.callback = audio_cb;

    g_audioDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_audioDev) { LOG("audio: open failed: %s", SDL_GetError()); return; }
    LOG("audio: driver %s, %d Hz, %d ch, buffer %d", SDL_GetCurrentAudioDriver(),
        have.freq, (int)have.channels, (int)have.samples);
    SDL_PauseAudioDevice(g_audioDev, 0);
}

/* ---- loading work, one small step per frame ------------------------------ */

typedef struct {
    int step, done, failed;
    float target;
    char base[200], dir[260], dataPath[300];
    FILE *f;
    long fileSize;
    MsfcHeader h;
    uint8_t *idx;
} Loader;

static Loader L;

static void fail(const char *msg)
{
    LOG("FAILED: %s", msg);
    L.failed = 1;
}

static void load_step(void)
{
    if (L.failed || L.done) return;

    switch (L.step) {
    case 0:
        LOG("WHBMountSdCard: %s", WHBMountSdCard() ? "ok" : "failed (continuing)");
        if (!pick_sd_base(L.base, sizeof L.base)) { fail("SD card not found at any known path"); return; }
        LOG("SD path in use: %s", L.base);
        L.target = 0.15f;
        break;

    case 1:
        snprintf(L.dir, sizeof L.dir, "%s/MSFC", L.base);
        snprintf(L.dataPath, sizeof L.dataPath, "%s/MSFC.data", L.dir);
        snprintf(g_logPath, sizeof g_logPath, "%s/msfc_wiiu.log", L.dir);
        if (!dir_exists(L.dir)) {
            if (mkdir(L.dir, 0777) == 0) LOG("Created %s", L.dir);
            else LOG("Could not create %s (errno %d)", L.dir, errno);
        }
        LOG("Data folder: %s", L.dir);
        LOG("Log file:    %s", g_logPath);
        L.target = 0.30f;
        break;

    case 2:
        L.f = fopen(L.dataPath, "rb");
        if (!L.f) { LOG("MSFC.data not found (errno %d). Put it in the MSFC folder.", errno); fail("no MSFC.data"); return; }
        fseek(L.f, 0, SEEK_END);
        L.fileSize = ftell(L.f);
        fseek(L.f, 0, SEEK_SET);
        LOG("MSFC.data found, %ld bytes", L.fileSize);
        L.target = 0.45f;
        break;

    case 3: {
        uint8_t hb[MSFC_HEADER_BYTES];
        int rc;
        if (fread(hb, 1, sizeof hb, L.f) != sizeof hb) { fail("could not read header"); return; }
        rc = msfc_parse_header(hb, sizeof hb, &L.h);
        if (rc != MSFC_OK) { LOG("Header check failed: %s", msfc_strerror(rc)); fail("bad header"); return; }
        LOG("Header OK: %u volumes, index %u bytes", (unsigned)L.h.count, (unsigned)L.h.dataStart);
        L.target = 0.60f;
        break;
    }

    case 4:
        L.idx = malloc(L.h.dataStart);
        if (!L.idx) { fail("out of memory for index"); return; }
        fseek(L.f, 0, SEEK_SET);
        if (fread(L.idx, 1, L.h.dataStart, L.f) != L.h.dataStart) { fail("could not read index"); return; }
        L.target = 0.75f;
        break;

    case 5: {
        uint32_t i, bad = 0;
        unsigned long long maxSize = 0, sumStored = 0;
        char maxName[96] = "";

        LOGF("--- volume index (idx name offset stored size flags) ---");
        for (i = 0; i < L.h.count; i++) {
            MsfcEntry e;
            int rc = msfc_get_entry(L.idx, L.h.dataStart, &L.h, i, &e);
            if (rc != MSFC_OK) { LOG("Entry %u unreadable: %s", (unsigned)i, msfc_strerror(rc)); bad++; continue; }
            LOGF("%3u %-36s off=%llu stored=%llu size=%llu flags=%u", (unsigned)i, e.name,
                 (unsigned long long)e.offset, (unsigned long long)e.storedSize,
                 (unsigned long long)e.size, (unsigned)e.flags);
            sumStored += e.storedSize;
            if (e.offset + e.storedSize > (unsigned long long)L.fileSize) bad++;
            if (e.size > maxSize) { maxSize = e.size; snprintf(maxName, sizeof maxName, "%s", e.name); }
        }
        LOG("Stored data %llu + index %u = %llu (file is %ld)", sumStored,
            (unsigned)L.h.dataStart, sumStored + L.h.dataStart, L.fileSize);
        LOG("Largest volume uncompressed: %s, %llu bytes", maxName, maxSize);
        if (bad) LOG("WARNING: %u entries failed checks (file may be damaged).", (unsigned)bad);
        else if (sumStored + L.h.dataStart == (unsigned long long)L.fileSize)
            LOG("Index matches file size exactly. MSFC.data looks complete.");
        else LOG("Index parsed, but sizes do not add up to the file size.");
        L.target = 1.0f;
        L.done = 1;
        LOG("Loading finished. Press HOME to exit.");
        break;
    }
    default:
        L.done = 1;
        break;
    }
    L.step++;
}

static void load_cleanup(void)
{
    free(L.idx);
    if (L.f) fclose(L.f);
    save_log();
}

/* ---- drawing ------------------------------------------------------------ */

static void draw(SDL_Renderer *r, SDL_Texture *tex, float shown, int failed, int frame)
{
    SDL_Rect track = { 240, 654, 800, 18 };
    SDL_Rect fill  = { 244, 658, (int)(792.0f * shown), 10 };

    SDL_SetRenderDrawColor(r, 16, 24, 48, 255);
    SDL_RenderClear(r);
    if (tex) SDL_RenderCopy(r, tex, NULL, NULL);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    SDL_RenderFillRect(r, &track);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 230);
    SDL_RenderDrawRect(r, &track);
    if (fill.w > 0) {
        if (failed) SDL_SetRenderDrawColor(r, 230, 60, 60, 255);
        else        SDL_SetRenderDrawColor(r, 255, 205, 60, 255);
        SDL_RenderFillRect(r, &fill);
    }
    if (frame < 40) {                              /* fade in from black */
        SDL_Rect all = { 0, 0, SCREEN_W, SCREEN_H };
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(255 - frame * 255 / 40));
        SDL_RenderFillRect(r, &all);
    }
    SDL_RenderPresent(r);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture  *tex = NULL;
    int running = 1, frame = 0, headless = 0;
    float shown = 0.0f;
    (void)argc; (void)argv;

    LOG(TITLE);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        LOG("SDL_Init failed: %s", SDL_GetError());
        headless = 1;
    } else {
        win = SDL_CreateWindow("MSFC", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               SCREEN_W, SCREEN_H, 0);
        if (win) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) {
            LOG("window/renderer failed: %s", SDL_GetError());
            headless = 1;
        } else {
            SDL_RendererInfo info;
            SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
            if (SDL_GetRendererInfo(ren, &info) == 0) LOG("video: driver %s, renderer %s, displays %d",
                SDL_GetCurrentVideoDriver(), info.name, SDL_GetNumVideoDisplays());
            {
                SDL_Surface *s = SDL_LoadBMP_RW(
                    SDL_RWFromConstMem(msfc_splash_bmp, (int)(msfc_splash_bmp_end - msfc_splash_bmp)), 1);
                if (s) { tex = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
                else LOG("splash image failed: %s", SDL_GetError());
            }
            start_music();
        }
    }

    if (headless) {                      /* no screen: still do the work and write the log */
        while (!L.done && !L.failed) load_step();
        LOG("Finished without a display. Check the SDL messages above.");
        load_cleanup();
        if (g_audioDev) SDL_CloseAudioDevice(g_audioDev);
        SDL_Quit();
        return 0;
    }

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }
        if (frame >= 10) load_step();                /* let the fade-in start first */
        shown += (L.target - shown) * 0.08f;
        if (L.target - shown < 0.002f) shown = L.target;
        draw(ren, tex, shown, L.failed, frame);
        frame++;
    }

    if (g_audioDev) SDL_CloseAudioDevice(g_audioDev);
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    load_cleanup();
    WHBUnmountSdCard();
    SDL_Quit();
    return 0;
}
