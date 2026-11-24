# Mighty Switch Force! Collection for Wii U (stage 1.5)

Stage 1.5 adds a proper loading experience on top of the stage 1 launcher:

* **Boot splash** (Wii U menu, before the app starts): `meta/tv.png` (1280x720)
  and `meta/drc.png` (854x480) are passed to `wuhbtool`.
* **In-app loading screen**: the artwork full screen with a fade-in and a
  progress bar, while the song plays. The image (`meta/splash.bmp`) and the
  music (`meta/loading.wav`, IMA-ADPCM) are compiled into the app.
* The stage 1 checks still run behind it and write `sd:/MSFC/msfc_wiiu.log`.

## Build on Windows (devkitPro installed)
Double-click `build.bat` (edit `DKP` inside if devkitPro is not in `C:\devkitPro`).
Or open the devkitPro **MSYS2** shell, `cd` here and run:

    pacman -S --needed wut wut-tools wiiu-sdl2
    make

## Build with GitHub Actions
Push to GitHub; `.github/workflows/build.yml` builds `msfc_wiiu.wuhb`.
Download it from the run's artifacts.

## Use
1. Copy `msfc_wiiu.wuhb` to `sd:/wiiu/apps/`.
2. Put your own `MSFC.data` in `sd:/MSFC/`.
3. Launch, then read `sd:/MSFC/msfc_wiiu.log`. Press HOME to exit.

## Changing the artwork or song
    python tools/make_assets.py new_image.png new_song.mp3
(needs Pillow and ffmpeg), then rebuild.

## Checks on a PC
    gcc -O2 -Isrc tests/dump_index.c src/msfc_index.c -o dump_index && ./dump_index MSFC.data
    ffmpeg -i meta/loading.wav -f s16le ref.s16le
    gcc -O2 -Isrc tests/adpcm_test.c src/adpcm.c -o adpcm_test && ./adpcm_test meta/loading.wav ref.s16le

## MSFC.data format (little-endian)
Magic 0x18F32F12; header fields at 0x10 give the volume count and the offsets of
the name, hash, flag, stored-size, size and file-offset tables; `dataStart` is
the index size. Each volume is raw deflate (no zlib header).
