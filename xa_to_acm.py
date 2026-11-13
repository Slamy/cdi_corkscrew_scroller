#!/usr/bin/env python3
"""Wrap 2336-byte 37.8 kHz 8-bit stereo XA sectors as CD-i APCM AIFF.

Nobelia's Master tool accepts APCM blocks containing 2304 bytes of sound
groups followed by 20 padding bytes. XA sector subheaders and EDC are not
part of those blocks; the CD-i mastering tool generates its own sectors.
"""
import struct
import sys
from pathlib import Path

source, target = map(Path, sys.argv[1:3])
data = source.read_bytes()
if not data or len(data) % 2336:
    raise SystemExit("XA input must contain complete 2336-byte sectors")
sectors = len(data) // 2336
blocks = bytearray()
for offset in range(0, len(data), 2336):
    sector = data[offset:offset + 2336]
    if sector[:4] != sector[4:8] or sector[3] != 0x11 or not sector[2] & 4:
        raise SystemExit("Expected stereo 37.8 kHz 8-bit XA audio sectors")
    payload = bytearray(sector[8:2312])
    # Level A has four parameters, repeated four times. psxavenc writes the
    # first copy but its 8-bit path does not initialize the third copy before
    # duplicating it. CD-i decoders may read any of the redundant copies.
    for group in range(0, len(payload), 128):
        params = payload[group:group + 4]
        payload[group + 4:group + 8] = params
        payload[group + 8:group + 12] = params
        payload[group + 12:group + 16] = params
    blocks.extend(payload)
    blocks.extend(bytes(20))

# AIFF 80-bit extended encoding of 37800 Hz; each stereo sector has 1008
# sample frames. APCM's block size includes the 20-byte padding.
comm = struct.pack(">hIh", 2, sectors * 1008, 8) + bytes.fromhex("400e93a8000000000000")
apcm = struct.pack(">II", 0, 2324) + blocks
body = b"AIFFCOMM" + struct.pack(">I", len(comm)) + comm
body += b"APCM" + struct.pack(">I", len(apcm)) + apcm
target.write_bytes(b"FORM" + struct.pack(">I", len(body)) + body)
