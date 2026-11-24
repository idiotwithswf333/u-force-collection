# Mighty Switch Force! Collection for Wii U (stage 1)

Stage 1 is a WUHB that proves the pipeline: it shows up in the Wii U menu with the
game's icon, finds the SD card, and writes a log to `sd:/MSFC/msfc_wiiu.log`.
If `sd:/MSFC/MSFC.data` is there, it also checks the file's header and logs all
141 volumes (name, offset, stored size, uncompressed size).

## Build on Windows (devkitPro installed)
Double-click `build.bat` (edit `DKP` inside if devkitPro is not in `C:\devkitPro`).
Or open the devkitPro **MSYS2** shell, `cd` to this folder and run:

    pacman -S --needed wut wut-tools
    make

Result: `msfc_wiiu.wuhb` in this folder. If pacman cannot find the packages,
run `pacman -Syu` once first.

## Build with GitHub Actions
Push to GitHub. The Actions workflow (`.github/workflows/build.yml`) builds
`msfc_wiiu.wuhb`; download it from the run's artifacts.
Locally, with devkitPro: `dkp-pacman -S wut wut-tools && make`.

## Use
1. Copy `msfc_wiiu.wuhb` to `sd:/wiiu/apps/` (Aroma).
2. Create `sd:/MSFC/` and put your own `MSFC.data` in it (the app creates the
   folder on first run if it is missing).
3. Launch it, then read `sd:/MSFC/msfc_wiiu.log`.

## Check the data file on a PC
    gcc -O2 -Isrc tests/dump_index.c src/msfc_index.c -o dump_index
    ./dump_index MSFC.data

## MSFC.data format (little-endian)
Magic 0x18F32F12; header fields at 0x10 give the volume count and the offsets of
the name, hash, flag, stored-size, size and file-offset tables; `dataStart` is
the index size. Each volume is raw deflate (no zlib header).
