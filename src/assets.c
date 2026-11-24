/* Embeds binary files into the executable. Paths are relative to the folder
 * `make` runs in (the project root). */
#include "assets.h"

#define INCBIN(sym, path)                                   \
    __asm__(".pushsection .rodata\n"                        \
            ".balign 16\n"                                  \
            ".global " #sym "\n" #sym ":\n"                 \
            ".incbin \"" path "\"\n"                        \
            ".global " #sym "_end\n" #sym "_end:\n"         \
            ".byte 0\n"                                     \
            ".popsection\n")

INCBIN(msfc_splash_bmp,  "meta/splash.bmp");
INCBIN(msfc_loading_wav, "meta/loading.wav");
