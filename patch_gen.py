"""Post-generation patches for rsc2recomp. Run after tools.recomp, before the build.

Each patch is (file-glob, anchor text, replacement). The anchor must be found in exactly
one place; a missing anchor is reported (the generated code changed under us).
"""
import glob,os,sys,re
GEN=os.path.join(os.path.dirname(os.path.abspath(__file__)),'src','recomp','gen','').replace('\\','/')
PATCHES=[
 # (anchor, replacement) pairs go here; see mm3recomp/patch_gen.py for examples.

 # Physical-heap free (sub_0023C920): "is this pointer in heap A" tests [base, base + (size << 10))
 # with size in bytes (0x20000 -> 128 MB). Harmless on the console, where XPhysicalAlloc is top-down
 # and heap A sits at the top of RAM; our arena is bottom-up, so every later XPhysicalAlloc block
 # (car textures) passed the test and was freed as a heap block, reading a "header" out of the
 # neighbouring texture. NOTES 15.
 ('loc_0023C932: ;\n    ecx = MEM32(edi + 0x6A4);\n    ecx = ecx << 0xA;\n',
  'loc_0023C932: ;\n    ecx = MEM32(edi + 0x6A4); /* patch_gen: bytes, not KB (NOTES 15) */\n'),
]
files=sorted(glob.glob(GEN+'recomp_*.c'))
ok=True
for anchor,repl in PATCHES:
    hits=[]
    for f in files:
        t=open(f,encoding='utf-8',errors='surrogateescape',newline='').read()
        a=anchor.replace('\n','\r\n') if '\r\n' in t[:2000] else anchor
        r=repl.replace('\n','\r\n') if '\r\n' in t[:2000] else repl
        if a in t: hits.append((f,t,a,r))
    if len(hits)!=1:
        print('PATCH ANCHOR FOUND IN',len(hits),'FILES:',anchor.splitlines()[0]); ok=False; continue
    f,t,a,r=hits[0]
    open(f,'w',encoding='utf-8',errors='surrogateescape',newline='').write(t.replace(a,r,1))
    print('patched',f.split('/')[-1])
sys.exit(0 if ok else 1)
