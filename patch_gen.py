"""Post-generation patches for rsc2recomp. Run after tools.recomp, before the build.

Each patch is (file-glob, anchor text, replacement). The anchor must be found in exactly
one place; a missing anchor is reported (the generated code changed under us).
"""
import glob,os,sys,re
GEN=os.path.join(os.path.dirname(os.path.abspath(__file__)),'src','recomp','gen','').replace('\\','/')
PATCHES=[
 # (anchor, replacement) pairs go here; see mm3recomp/patch_gen.py for examples.
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
