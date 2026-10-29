"""List generated functions whose first use of a CPU flag has no producer in that function.

xboxrecomp splits a function where a branch lands in the middle of it (or where the
disassembler only knew the tail as a branch target). Each chunk declares its own flag
variables, initialised to 0, so a chunk that starts with `jne` / `jcc` / `adc` / `setcc`
on flags its caller set tests zeros instead. In Midtown Madness 3 this silently broke D3D
SetTextureStageState (every stage state >= 0xC stored as COLOROP -> loading-screen image
missing). Typical source: `cmp x, y; jge tail` where tail itself is `jne ...`.

Textual order is used as an approximation of control flow (good enough to triage).
Each hit: inspect the caller's last flag producer before the transfer, then patch the
consumer in patch_gen.py (re-express the condition with the register it compared), or
fix the lifter to carry flags across chunk transfers.

usage: python -I scan_inherited_flags.py <gen dir> [--all]
       (default skips functions whose first flag read is in the very first statement
        after an ICALL-only body; --all prints everything)
"""
import glob, os, re, sys

FLAG = r'(?:_fa|_fb|_fas|_fbs|_flags|_cf)'
# `_fa = ...` at the start of a statement, or a shift's `if (count) _cf = ...`
PRODUCER = re.compile(r'^\s*(?:if \(\w+\) )?' + FLAG + r'\s*=(?!=)')
USE = re.compile(r'\b' + FLAG + r'\b')
DECL = re.compile(r'^\s*(?:int|uint32_t|int32_t)\s+' + FLAG + r'\b|^\s*\(void\)')
FUNC = re.compile(r'^void (sub_[0-9A-F]{8})\(void\)\s*$')


def scan(path):
    hits = []
    name = None
    produced = False
    reported = False
    for ln, line in enumerate(open(path, encoding='utf-8', errors='replace'), 1):
        m = FUNC.match(line)
        if m:
            name, produced, reported = m.group(1), False, False
            continue
        if name is None:
            continue
        if line.startswith('}'):
            name = None
            continue
        if DECL.match(line) or 'mm3recomp patch' in line or 'recomp patch' in line:
            continue
        if PRODUCER.match(line):
            produced = True
            continue
        if not produced and not reported and USE.search(line):
            hits.append((name, os.path.basename(path), ln, line.strip()))
            reported = True
    return hits


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    total = []
    for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.c'))):
        total += scan(f)
    for name, f, ln, text in total:
        print('%s  %s:%d  %s' % (name, f, ln, text[:110]))
    print('%d functions read flags they did not set' % len(total), file=sys.stderr)


if __name__ == '__main__':
    main()
