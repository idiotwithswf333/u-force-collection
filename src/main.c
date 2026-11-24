/* MSFC for Wii U - stage 1: launcher + logging.
 *
 * - Finds the SD card and the MSFC folder at its root.
 * - Writes a log to <sd>/MSFC/msfc_wiiu.log (also shown on screen).
 * - If <sd>/MSFC/MSFC.data exists, validates its header and logs the volume
 *   index. Nothing is decompressed or launched yet.
 */
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/sdcard.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "msfc_index.h"

#define APP_TITLE "Mighty Switch Force! Collection (Wii U) - stage 1"

/* ---- logging: console + in-memory buffer saved to the log file ---------- */

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

/* Screen + file (file is rewritten each time; fine for a handful of lines). */
static void LOG(const char *fmt, ...)
{
    char line[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    WHBLogPrint(line);
    append(line);
    save_log();
}

/* File only (used for the 141-line volume list). */
static void LOGF(const char *fmt, ...)
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

/* Which prefix reaches the SD card depends on the environment, so try a few
 * and say which one worked. */
static int pick_sd_base(char *out, size_t n)
{
    static const char *candidates[] = {
        "fs:/vol/external01",
        "/vol/external01",
        "sd:",
    };
    size_t i;
    for (i = 0; i < sizeof candidates / sizeof candidates[0]; i++) {
        if (dir_exists(candidates[i])) {
            snprintf(out, n, "%s", candidates[i]);
            return 1;
        }
    }
    return 0;
}

/* ---- MSFC.data inspection ----------------------------------------------- */

static void inspect_data(const char *dataPath)
{
    FILE *f = fopen(dataPath, "rb");
    uint8_t hb[MSFC_HEADER_BYTES];
    uint8_t *idx = NULL;
    MsfcHeader h;
    long fileSize;
    int rc;
    uint32_t i, bad = 0;
    unsigned long long maxSize = 0, sumStored = 0;
    char maxName[96] = "";

    if (!f) {
        LOG("MSFC.data not found (errno %d). Put it in the MSFC folder.", errno);
        return;
    }
    fseek(f, 0, SEEK_END);
    fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    LOG("MSFC.data found, %ld bytes", fileSize);

    if (fread(hb, 1, sizeof hb, f) != sizeof hb) {
        LOG("Could not read the header.");
        goto done;
    }
    rc = msfc_parse_header(hb, sizeof hb, &h);
    if (rc != MSFC_OK) {
        LOG("Header check failed: %s", msfc_strerror(rc));
        goto done;
    }
    LOG("Header OK: %u volumes, index %u bytes", (unsigned)h.count, (unsigned)h.dataStart);

    idx = malloc(h.dataStart);
    if (!idx) { LOG("Out of memory reading index."); goto done; }
    fseek(f, 0, SEEK_SET);
    if (fread(idx, 1, h.dataStart, f) != h.dataStart) {
        LOG("Could not read the index.");
        goto done;
    }

    LOGF("--- volume index (idx name offset stored size flags) ---");
    for (i = 0; i < h.count; i++) {
        MsfcEntry e;
        rc = msfc_get_entry(idx, h.dataStart, &h, i, &e);
        if (rc != MSFC_OK) {
            LOG("Entry %u unreadable: %s", (unsigned)i, msfc_strerror(rc));
            bad++;
            continue;
        }
        LOGF("%3u %-36s off=%llu stored=%llu size=%llu flags=%u",
             (unsigned)i, e.name,
             (unsigned long long)e.offset, (unsigned long long)e.storedSize,
             (unsigned long long)e.size, (unsigned)e.flags);
        sumStored += e.storedSize;
        if (e.offset + e.storedSize > (unsigned long long)fileSize) bad++;
        if (e.size > maxSize) {
            maxSize = e.size;
            snprintf(maxName, sizeof maxName, "%s", e.name);
        }
    }

    LOG("Stored data %llu + index %u = %llu (file is %ld)",
        sumStored, (unsigned)h.dataStart,
        sumStored + h.dataStart, fileSize);
    LOG("Largest volume uncompressed: %s, %llu bytes", maxName, maxSize);
    if (bad)
        LOG("WARNING: %u entries failed checks (file may be damaged).", (unsigned)bad);
    else if (sumStored + h.dataStart == (unsigned long long)fileSize)
        LOG("Index matches file size exactly. MSFC.data looks complete.");
    else
        LOG("Index parsed, but sizes do not add up to the file size.");

done:
    free(idx);
    fclose(f);
}

/* ------------------------------------------------------------------------- */

int main(void)
{
    char base[200], dataDir[260], dataPath[300];

    WHBProcInit();
    WHBLogConsoleInit();

    LOG(APP_TITLE);

    LOG("WHBMountSdCard: %s", WHBMountSdCard() ? "ok" : "failed (continuing)");

    if (!pick_sd_base(base, sizeof base)) {
        LOG("Could not find the SD card at any known path.");
        LOG("No log file can be written. Press HOME to exit.");
    } else {
        snprintf(dataDir, sizeof dataDir, "%s/MSFC", base);
        snprintf(g_logPath, sizeof g_logPath, "%s/msfc_wiiu.log", dataDir);

        if (!dir_exists(dataDir)) {
            if (mkdir(dataDir, 0777) == 0)
                LOG("Created %s", dataDir);
            else
                LOG("Could not create %s (errno %d)", dataDir, errno);
        }
        LOG("SD path in use: %s", base);
        LOG("Data folder:    %s", dataDir);
        LOG("Log file:       %s", g_logPath);

        snprintf(dataPath, sizeof dataPath, "%s/MSFC.data", dataDir);
        inspect_data(dataPath);
        LOG("Done. Press HOME to exit.");
    }
    save_log();

    while (WHBProcIsRunning()) {
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(50));
    }

    WHBLogConsoleFree();
    WHBUnmountSdCard();
    WHBProcShutdown();
    return 0;
}
