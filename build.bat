@echo off
rem Builds msfc_wiiu.wuhb with devkitPro's MSYS2. Edit DKP if installed elsewhere.
set "DKP=C:\devkitPro"
if not exist "%DKP%\msys2\usr\bin\bash.exe" (
  echo devkitPro not found at %DKP%. Edit DKP at the top of build.bat.
  pause
  exit /b 1
)
set "PROJ=%~dp0"
"%DKP%\msys2\usr\bin\bash.exe" -lc "cd \"$(cygpath -u \"$PROJ\")\" && (pacman -S --needed --noconfirm wut wut-tools wiiu-sdl2 ; make)"
echo.
if exist msfc_wiiu.wuhb (echo Built: %~dp0msfc_wiiu.wuhb) else (echo Build failed - see messages above.)
pause
