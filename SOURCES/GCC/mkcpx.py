#!/usr/bin/env python3
"""mkcpx.py - turns a linked .PRG into a .CPX (replaces BASBUILD.PRG)

A CPX file is the 512-byte CPX header (icon, texts, version) followed
by a complete GEMDOS program file. The header comes from cpxhead.bin,
which was taken from the Pure C build; only the version is set here.
Like BASBUILD, a "vX.yz" placeholder in the data segment is replaced
by the version number.

usage: mkcpx.py header.bin program.prg out.cpx [version, default 0x130]
"""
import struct
import sys


def main():
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    head_fn, prg_fn, cpx_fn = sys.argv[1:4]
    version = int(sys.argv[4], 0) if len(sys.argv) == 5 else 0x130

    head = bytearray(open(head_fn, 'rb').read())
    if len(head) != 512 or head[0:2] != b'\x00\x64':
        sys.exit('%s: not a CPX header' % head_fn)
    struct.pack_into('>H', head, 8, version)

    prg = bytearray(open(prg_fn, 'rb').read())
    if prg[0:2] != b'\x60\x1a':
        sys.exit('%s: not a GEMDOS program' % prg_fn)
    tlen, dlen = struct.unpack_from('>LL', prg, 2)
    data = 28 + tlen
    pos = prg.find(b'vX.yz', data, data + dlen)
    if pos >= 0:
        prg[pos + 1] = ord('0') + (version >> 8)
        prg[pos + 3] = ord('0') + ((version >> 4) & 15)
        prg[pos + 4] = ord('0') + (version & 15)

    with open(cpx_fn, 'wb') as f:
        f.write(head)
        f.write(prg)


if __name__ == '__main__':
    main()
