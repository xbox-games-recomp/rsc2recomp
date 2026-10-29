"""Direct-call cross references between XBE sections:
xrefs.py <xbe> <from-section> <to-lo> <to-hi>

Linear-sweeps the from-section with capstone and prints every direct call/jmp
target in [to-lo, to-hi) with its call sites. Used to list which library
functions the game calls (e.g. into XPP) before re-locating overrides.
"""
import collections
import struct
import sys

import capstone


def sections(xbe):
    base, = struct.unpack_from("<I", xbe, 0x104)
    n, hdr = struct.unpack_from("<II", xbe, 0x11C)
    for i in range(n):
        o = hdr - base + i * 56
        _flags, va, vsize, raw, rsize, name_va = struct.unpack_from("<IIIIII", xbe, o)
        no = name_va - base
        yield xbe[no:xbe.index(b"\0", no)].decode("latin-1"), va, raw, rsize


def main():
    xbe = open(sys.argv[1], "rb").read()
    src = sys.argv[2]
    lo, hi = int(sys.argv[3], 16), int(sys.argv[4], 16)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    refs = collections.defaultdict(list)
    for name, va, raw, rsize in sections(xbe):
        if name != src:
            continue
        for ins in md.disasm(xbe[raw:raw + rsize], va):
            if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
                t = int(ins.op_str, 16)
                if lo <= t < hi:
                    refs[t].append(ins.address)
    for t in sorted(refs):
        sites = refs[t]
        print("%08X  %3d  %s" % (t, len(sites), " ".join("%08X" % s for s in sites[:6])))


main()
