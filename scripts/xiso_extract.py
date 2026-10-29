"""Minimal XDVDFS (XISO) extractor: xiso_extract.py <iso> <outdir> [partition_offset]"""
import os, struct, sys

SECTOR = 2048

def main():
    iso, out = sys.argv[1], sys.argv[2]
    base = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0
    f = open(iso, "rb")
    f.seek(base + 32 * SECTOR)
    vd = f.read(SECTOR)
    assert vd[:20] == b"MICROSOFT*XBOX*MEDIA", "no XDVDFS header"
    root_sec, root_size = struct.unpack_from("<II", vd, 20)
    nfiles = nbytes = 0

    def walk_dir(sec, size, path):
        nonlocal nfiles, nbytes
        if size == 0:
            return
        f.seek(base + sec * SECTOR)
        table = f.read(size)
        seen = set()
        stack = [0]
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(table):
                continue
            seen.add(off)
            left, right, start, fsize, attr, nlen = struct.unpack_from("<HHIIBB", table, off)
            if left == 0xFFFF and right == 0xFFFF and start == 0xFFFFFFFF:
                continue
            name = table[off + 14: off + 14 + nlen].decode("latin-1")
            if left:
                stack.append(left * 4)
            if right:
                stack.append(right * 4)
            p = os.path.join(path, name)
            if attr & 0x10:
                os.makedirs(p, exist_ok=True)
                walk_dir(start, fsize, p)
            else:
                f.seek(base + start * SECTOR)
                remaining = fsize
                with open(p, "wb") as o:
                    while remaining:
                        chunk = f.read(min(remaining, 1 << 22))
                        if not chunk:
                            raise IOError("short read in " + p)
                        o.write(chunk)
                        remaining -= len(chunk)
                nfiles += 1
                nbytes += fsize

    os.makedirs(out, exist_ok=True)
    walk_dir(root_sec, root_size, out)
    print("files=%d bytes=%d" % (nfiles, nbytes))

main()
