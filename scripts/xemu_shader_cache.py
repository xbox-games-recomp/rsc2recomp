"""Read xemu's on-disk shader cache: per-programme ground truth for NV2A pixel state.

xemu writes one file per compiled shader under %APPDATA%\\xemu\\xemu\\shaders\\<4hex>\\<12hex>:
  3 x (u64 length incl. NUL, string)   xemu version, GL vendor, GL version
  u32                                  binary format
  ShaderState (raw struct)             VshState, GeomState, PshState (PshState = last 360 bytes)
  u64 program size, GL program binary
The ShaderState size is found from the trailing u64 (offset + 8 + size == file size).

The vertex-programme tokens sit at a fixed offset inside ShaderState (208 bytes in xemu
0.8.136; --find-offset rediscovers it for another build). Their FNV-1a over 4 words per slot,
stopping at the slot with the final bit (word 3, bit 0), equals xboxrecomp's
nv2a_vsh_program_hash(), so a cache entry can be matched to a draw in our own logs.

usage:
  python -I xemu_shader_cache.py list [--hash H[,H..]] [--version 0.8.136] [--prog-offset 208]
  python -I xemu_shader_cache.py find-offset --hash H[,H..]   (hashes you know the game uses)
The cache holds every game run in xemu; filter by --hash or by the game you ran last
(--newer-than YYYY-MM-DD).
"""
import argparse, datetime, glob, os, struct, sys

ROOT = os.path.join(os.environ.get('APPDATA', ''), 'xemu', 'xemu', 'shaders')
PSH_NAMES = (['combiner_control', 'shader_stage_program', 'other_stage_input', 'final0', 'final1']
             + ['rgb_in%d' % i for i in range(8)] + ['rgb_out%d' % i for i in range(8)]
             + ['alpha_in%d' % i for i in range(8)] + ['alpha_out%d' % i for i in range(8)])


def parse(path):
    b = open(path, 'rb').read()
    p, strs = 0, []
    while len(strs) < 3 and p + 8 <= len(b):
        n = struct.unpack_from('<Q', b, p)[0]
        s = b[p + 8:p + 8 + n]
        if n == 0 or n > 512 or not all(32 <= c < 127 for c in s[:-1]):
            break
        strs.append(s.rstrip(b'\0').decode())
        p += 8 + n
    p += 4  # binary format
    for q in range(p, len(b) - 8):
        if q + 8 + struct.unpack_from('<Q', b, q)[0] == len(b):
            return strs, b[p:q]
    return strs, None


def prog_hash(st, off):
    w = struct.unpack_from('<%dI' % (len(st) // 4), st)
    k, h = off // 4, 2166136261
    for _ in range(136):
        if k + 4 > len(w):
            break
        for i in range(4):
            h = ((h ^ w[k + i]) * 16777619) & 0xFFFFFFFF
        if w[k + 3] & 1:
            break
        k += 4
    return h


def entries(args):
    since = None
    if getattr(args, 'newer_than', None):
        since = datetime.datetime.strptime(args.newer_than, '%Y-%m-%d').timestamp()
    for f in sorted(glob.glob(os.path.join(ROOT, '*', '*'))):
        if since and os.path.getmtime(f) < since:
            continue
        strs, st = parse(f)
        if st is None or not strs or (args.version and not strs[0].startswith(args.version)):
            continue
        yield f, strs, st


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['list', 'find-offset'])
    ap.add_argument('--hash', default='')
    ap.add_argument('--version', default='0.8.136')
    ap.add_argument('--prog-offset', type=int, default=208)
    ap.add_argument('--newer-than')
    ap.add_argument('--raw', action='store_true', help='also print the whole PshState as words')
    args = ap.parse_args()
    want = {int(h, 16) for h in args.hash.split(',') if h}

    if args.cmd == 'find-offset':
        if not want:
            sys.exit('find-offset needs --hash with programme hashes from your own logs')
        votes = {}
        for f, strs, st in entries(args):
            for off in range(0, len(st) - 16, 4):
                if prog_hash(st, off) in want:
                    votes[off] = votes.get(off, 0) + 1
            if sum(votes.values()) >= 10:
                break
        for off, n in sorted(votes.items(), key=lambda x: -x[1]):
            print('offset %d: %d matches' % (off, n))
        return

    seen = set()
    for f, strs, st in entries(args):
        h = prog_hash(st, args.prog_offset)
        if want and h not in want:
            continue
        psh = struct.unpack_from('<90I', st, len(st) - 360)
        key = (h, psh)
        if key in seen:
            continue
        seen.add(key)
        spec = st[16]
        print('%08X  %s  shaderstate=%d specular_enable=%d' % (h, os.path.relpath(f, ROOT), len(st), spec))
        named = dict(zip(PSH_NAMES, psh))
        stages = named['combiner_control'] & 0xF
        print('   cc %08X  stage_prog %08X  other_in %08X  final %08X %08X' % (
            named['combiner_control'], named['shader_stage_program'], named['other_stage_input'],
            named['final0'], named['final1']))
        for i in range(max(stages, 1)):
            print('   stage %d  rgb in %08X out %08X  alpha in %08X out %08X' % (
                i, named['rgb_in%d' % i], named['rgb_out%d' % i],
                named['alpha_in%d' % i], named['alpha_out%d' % i]))
        if args.raw:
            print('   raw', ' '.join('%08X' % x for x in psh))


if __name__ == '__main__':
    main()
