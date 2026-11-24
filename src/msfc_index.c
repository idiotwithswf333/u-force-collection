#include "msfc_index.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

int msfc_parse_header(const uint8_t *buf, size_t len, MsfcHeader *h)
{
    if (len < MSFC_HEADER_BYTES) return MSFC_ERR_SHORT;
    if (rd32(buf) != MSFC_MAGIC) return MSFC_ERR_MAGIC;

    h->count       = rd32(buf + 0x10);
    h->nameOffsTbl = rd32(buf + 0x14);
    h->hashTbl     = rd32(buf + 0x18);
    h->flagsTbl    = rd32(buf + 0x1C);
    h->storedTbl   = rd32(buf + 0x20);
    h->sizeTbl     = rd32(buf + 0x24);
    h->offsetTbl   = rd32(buf + 0x28);
    h->dataStart   = rd32(buf + 0x2C);

    if (h->count == 0 || h->count > MSFC_MAX_ENTRIES) return MSFC_ERR_RANGE;
    if (h->dataStart < MSFC_HEADER_BYTES || h->dataStart > MSFC_MAX_INDEX_BYTES)
        return MSFC_ERR_RANGE;
    return MSFC_OK;
}

static int table_fits(uint32_t off, uint64_t bytes, size_t len)
{
    return (uint64_t)off + bytes <= (uint64_t)len;
}

int msfc_get_entry(const uint8_t *buf, size_t len, const MsfcHeader *h,
                   uint32_t i, MsfcEntry *out)
{
    uint64_t n = h->count;
    uint32_t nameOff;

    if (i >= h->count) return MSFC_ERR_RANGE;
    if (!table_fits(h->nameOffsTbl, 4 * n, len) ||
        !table_fits(h->flagsTbl,    1 * n, len) ||
        !table_fits(h->storedTbl,   8 * n, len) ||
        !table_fits(h->sizeTbl,     8 * n, len) ||
        !table_fits(h->offsetTbl,   8 * n, len))
        return MSFC_ERR_SHORT;

    nameOff = rd32(buf + h->nameOffsTbl + 4 * i);
    if (nameOff >= len || !memchr(buf + nameOff, 0, len - nameOff))
        return MSFC_ERR_RANGE;

    out->name       = (const char *)(buf + nameOff);
    out->flags      = buf[h->flagsTbl + i];
    out->storedSize = rd64(buf + h->storedTbl + 8 * i);
    out->size       = rd64(buf + h->sizeTbl   + 8 * i);
    out->offset     = rd64(buf + h->offsetTbl + 8 * i);
    return MSFC_OK;
}

const char *msfc_strerror(int code)
{
    switch (code) {
    case MSFC_OK:        return "ok";
    case MSFC_ERR_SHORT: return "buffer too short";
    case MSFC_ERR_MAGIC: return "bad magic (not an MSFC.data?)";
    case MSFC_ERR_RANGE: return "field out of range";
    default:             return "unknown error";
    }
}
