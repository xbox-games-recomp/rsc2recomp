"""Disassemble guest code from default.xbe: xdis.py <va> [count] [xbe]

Maps the VA through the XBE section table (base 0x10000) and prints `count`
instructions with capstone. Handy for reading library code the disassembler did
not sweep, and for comparing a function with its MM3 counterpart.
"""
import struct
import sys

import capstone


def sections(xbe):
    base, = struct.unpack_from("<I", xbe, 0x104)
    n, hdr = struct.unpack_from("<II", xbe, 0x11C)
    out = []
    for i in range(n):
        o = hdr - base + i * 56
        _flags, va, vsize, raw, rsize, name_va = struct.unpack_from("<IIIIII", xbe, o)
        no = name_va - base
        name = xbe[no:xbe.index(b"\0", no)].decode("latin-1")
        out.append((name, va, vsize, raw, rsize))
    return out


def main():
    va = int(sys.argv[1], 16)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    path = sys.argv[3] if len(sys.argv) > 3 else "game_files/default.xbe"
    xbe = open(path, "rb").read()
    for name, sva, vsize, raw, rsize in sections(xbe):
        if sva <= va < sva + rsize:
            code = xbe[raw + va - sva: raw + rsize]
            break
    else:
        sys.exit("VA %08X not in a loaded section" % va)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    print("; %s" % name)
    for i, ins in enumerate(md.disasm(code, va)):
        if i >= count:
            break
        print("%08X  %-24s %s %s" % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))


main()
