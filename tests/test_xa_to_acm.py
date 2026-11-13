"""Regression test for the Level A redundant sound-group parameters."""
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "input.xa"
    output = Path(directory) / "output.acm"
    sector = bytearray(2336)
    sector[:8] = bytes((0, 0, 0x64, 0x11)) * 2
    for group in range(18):
        start = 8 + group * 128
        sector[start:start + 4] = bytes((group, group + 1, group + 2, group + 3))
        sector[start + 8:start + 16] = bytes((0xF0,)) * 8
        sector[start + 16:start + 128] = bytes((group,)) * 112
    source.write_bytes(sector)
    subprocess.run(["python3", "xa_to_acm.py", str(source), str(output)], check=True)
    acm = output.read_bytes()
    assert acm[:4] == b"FORM" and acm[38:42] == b"APCM"
    for group in range(18):
        start = 54 + group * 128
        params = bytes((group, group + 1, group + 2, group + 3))
        assert acm[start:start + 16] == params * 4
        assert acm[start + 16:start + 128] == bytes((group,)) * 112
    assert acm[54 + 2304:] == bytes(20)
print("Level A sound-group headers: OK")
