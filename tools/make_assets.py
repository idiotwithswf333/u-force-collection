#!/usr/bin/env python3
"""Regenerates the loading-screen assets in meta/ from your source files.

    python tools/make_assets.py splash.png song.mp3

Needs Pillow and ffmpeg. Produces:
  meta/tv.png       1280x720 boot splash shown by the Wii U on the TV
  meta/drc.png      854x480  boot splash shown on the GamePad
  meta/splash.bmp   1280x720 24-bit BMP, embedded in the app (in-app loading screen)
  meta/loading.wav  48 kHz stereo IMA-ADPCM, embedded in the app (loading music)
"""
import subprocess, sys
from PIL import Image

if len(sys.argv) != 3:
    sys.exit(__doc__)
img, song = sys.argv[1], sys.argv[2]

im = Image.open(img).convert("RGB").resize((1280, 720), Image.LANCZOS)
im.save("meta/tv.png")
im.resize((854, 480), Image.LANCZOS).save("meta/drc.png")
im.save("meta/splash.bmp")
subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", song, "-vn", "-ac", "2", "-ar", "48000",
                       "-c:a", "adpcm_ima_wav", "-block_size", "1024", "meta/loading.wav"])
print("done")
