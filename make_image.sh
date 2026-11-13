#!/bin/sh
set -eu
mkdir -p build disk
if [ -n "${PSXAVENC:-}" ]; then
    encoder=$PSXAVENC
elif [ -x build/psxavenc/psxavenc ]; then
    encoder=build/psxavenc/psxavenc
else
    encoder=psxavenc
fi
"$encoder" -q -t xa -f 37800 -c 2 -b 8 "${MUSIC_SOURCE:-surasshu_eek.mkv}" build/MUSIC.XA
python3 xa_to_acm.py build/MUSIC.XA build/MUSIC.ACM
printf 'CD-i scroller example\r\n' > build/COPYRGHT.TXT
printf 'CD-i scroller example\r\n' > build/ABSTRACT.TXT
printf 'CD-i scroller example\r\n' > build/BIBLIOGR.TXT
printf 'CD-i scroller example\r\n' > build/MESSAGE.TXT
rm -f build/MASTER.LOG disk/SCROLLER.CDI disk/SCROLLER.BIN disk/SCROLLER.CUE
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy dosbox -conf /home/andre/GIT/Nobelia/dosbox.conf master.bat -exit
cat build/MASTER.LOG
grep -q 'End   generation of album' build/MASTER.LOG
dd skip=150 bs=2352 if=disk/SCROLLER.CDI of=disk/SCROLLER.BIN
printf 'FILE "SCROLLER.BIN" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' > disk/SCROLLER.CUE
