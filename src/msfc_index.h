/* MSFC.data index reader. Pure C, no console/OS calls, endian-independent
 * (the file is little-endian; the Wii U CPU is big-endian). */
#ifndef MSFC_INDEX_H
#define MSFC_INDEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSFC_MAGIC            0x18F32F12u
#define MSFC_HEADER_BYTES     0x30u
#define MSFC_MAX_ENTRIES      4096u
#define MSFC_MAX_INDEX_BYTES  (1u << 20)

enum {
    MSFC_OK         =  0,
    MSFC_ERR_SHORT  = -1,  /* buffer too small              */
    MSFC_ERR_MAGIC  = -2,  /* not an MSFC.data file         */
    MSFC_ERR_RANGE  = -3   /* a field/table is out of range */
};

/* Fields at file offsets 0x10..0x2F. Table offsets are absolute file offsets. */
typedef struct {
    uint32_t count;        /* number of volumes (141 in the Steam build)      */
    uint32_t nameOffsTbl;  /* u32[count]  absolute offsets of volume names    */
    uint32_t hashTbl;      /* u32[count]  name hashes (unused so far)         */
    uint32_t flagsTbl;     /* u8[count]   1 = raw-deflate compressed          */
    uint32_t storedTbl;    /* u64[count]  stored (compressed) size            */
    uint32_t sizeTbl;      /* u64[count]  uncompressed size                   */
    uint32_t offsetTbl;    /* u64[count]  absolute file offset of the volume  */
    uint32_t dataStart;    /* first volume starts here = index size in bytes  */
} MsfcHeader;

typedef struct {
    const char *name;      /* points into the index buffer */
    uint64_t    offset;
    uint64_t    storedSize;
    uint64_t    size;
    uint8_t     flags;
} MsfcEntry;

/* Parse the first MSFC_HEADER_BYTES bytes of the file. */
int msfc_parse_header(const uint8_t *buf, size_t len, MsfcHeader *h);

/* buf = the first h->dataStart bytes of the file. */
int msfc_get_entry(const uint8_t *buf, size_t len, const MsfcHeader *h,
                   uint32_t index, MsfcEntry *out);

const char *msfc_strerror(int code);

#ifdef __cplusplus
}
#endif
#endif
