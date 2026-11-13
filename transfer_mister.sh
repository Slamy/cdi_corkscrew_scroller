#!/bin/sh
set -eu

# Nobelia's README uses this destination for CD-i disc images.
# Pass a CUE path as the first argument; the companion BIN must have the
# same basename and match the FILE entry in the CUE.
cue=${1:-disk/SCROLLER.CUE}
base=${cue%.*}
bin=$base.BIN
host=${MISTER_HOST:-root@mister}
dest=${MISTER_CDI_DIR:-/media/fat/games/CD-i}

if [ ! -f "$cue" ] || [ ! -f "$bin" ]; then
    printf 'Disc image not found: %s and %s\n' "$cue" "$bin" >&2
    exit 1
fi

scp "$cue" "$bin" "$host:$dest/"
