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

 # Tree draw distance (RECOMP_TREE_DETAIL, src/recomp_manual.c): the tree system's size threshold
 # ([0x4947D0 + 0x8108] = 0x49C8D8; 0.02, then the track's value, stored by the loader) is divided
 # by the knob where the culls read it: the five group culls (0x2AF9B0, 0x2B0A30 and the like, all
 # `threshold * factor`) and the per-tree cull 0x10F0D0. Default 1 = the console's. NOTES 23.
 ('    xmm1.f[0] = xmm1.f[0] * MEMF(0x49C8D8); /* mulss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm1.f[0] = xmm1.f[0] * (MEMF(0x49C8D8) * recomp_tree_lod_scale()); } /* patch_gen: NOTES 23 */\n', 5),
 ('    xmm5 = XMM_SCALAR(MEMF(ecx + 0x8108)); /* movss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm5 = XMM_SCALAR(MEMF(ecx + 0x8108) * recomp_tree_lod_scale()); } /* patch_gen: NOTES 23 */\n'),
 # ... and the far plane of the trees' cull frustum, (0.2 + 1) * the zone's view distance (about 530 units
 # on SS1, while their fog is capped at 85%), is multiplied by it in the render traversal sub_00273E70. NOTES 23.
 ('loc_002741D0: ;\n    xmm0 = XMM_SCALAR(MEMF(0x3DF758)); /* movss */\n    xmm0.f[0] = xmm0.f[0] + MEMF(0x3B8400); /* addss */\n    xmm0.f[0] = xmm0.f[0] * MEMF(esi + 0x24268); /* mulss */\n',
  'loc_002741D0: ;\n    xmm0 = XMM_SCALAR(MEMF(0x3DF758)); /* movss */\n    xmm0.f[0] = xmm0.f[0] + MEMF(0x3B8400); /* addss */\n    xmm0.f[0] = xmm0.f[0] * MEMF(esi + 0x24268); /* mulss */\n    { extern float recomp_tree_lod_scale(void); xmm0.f[0] = xmm0.f[0] / recomp_tree_lod_scale(); } /* patch_gen: NOTES 23 */\n'),
 # ... and the switch from a tree's model to its flat card: sub_0010F0D0 crossfades by projected size
 # between [type + 0x2F8] * [0x4947D0 + 0x80E0] * [+0x80D0] and that * [+0x80DC] (fade 1.0 = card only),
 # and sub_00114390 / sub_00114860 / sub_00114C80 draw the model only above the size window
 # [+0x8114]..[+0x8110]. All of these are multiplied by the knob's scale, so models last n times
 # further. At 4x the card that took over a distant tree model left nothing on screen. NOTES 25.
 ('    MEMF(esp + 0x40) = xmm0.f[0]; /* movss */\n    xmm0 = XMM_SCALAR(MEMF(edx + 0x14)); /* movss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm0.f[0] = xmm0.f[0] * recomp_tree_lod_scale(); xmm1.f[0] = xmm1.f[0] * recomp_tree_lod_scale(); } /* patch_gen: NOTES 25 */\n    MEMF(esp + 0x40) = xmm0.f[0]; /* movss */\n    xmm0 = XMM_SCALAR(MEMF(edx + 0x14)); /* movss */\n'),
 ('    xmm0 = XMM_SCALAR(MEMF(ebp + 0x8110)); /* movss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm0 = XMM_SCALAR(MEMF(ebp + 0x8110) * recomp_tree_lod_scale()); } /* patch_gen: NOTES 25 */\n', 2),
 ('    xmm2 = XMM_SCALAR(MEMF(ebp + 0x8110)); /* movss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm2 = XMM_SCALAR(MEMF(ebp + 0x8110) * recomp_tree_lod_scale()); } /* patch_gen: NOTES 25 */\n', 2),
 ('    xmm0.f[0] = xmm0.f[0] - MEMF(ebp + 0x8114); /* subss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm0.f[0] = xmm0.f[0] - MEMF(ebp + 0x8114) * recomp_tree_lod_scale(); } /* patch_gen: NOTES 25 */\n', 3),
 ('    xmm2.f[0] = xmm2.f[0] - MEMF(ebp + 0x8114); /* subss */\n',
  '    { extern float recomp_tree_lod_scale(void); xmm2.f[0] = xmm2.f[0] - MEMF(ebp + 0x8114) * recomp_tree_lod_scale(); } /* patch_gen: NOTES 25 */\n'),
 ('    _fca = xmm0.f[0]; _fcb = MEMF(ebp + 0x8114); /* comiss */\n',
  '    { extern float recomp_tree_lod_scale(void); _fca = xmm0.f[0]; _fcb = MEMF(ebp + 0x8114) * recomp_tree_lod_scale(); } /* patch_gen: NOTES 25 */\n'),
 ('    _fca = xmm2.f[0]; _fcb = MEMF(ebp + 0x8114); /* comiss */\n',
  '    { extern float recomp_tree_lod_scale(void); _fca = xmm2.f[0]; _fcb = MEMF(ebp + 0x8114) * recomp_tree_lod_scale(); } /* patch_gen: NOTES 25 */\n'),

 # The render queue's fixed arrays, never full on the console but overrun once the tree knob lets more
 # objects through (a 4x drive crashed in the start flyby at ~2280 draws, sub_00273360). Each insert is
 # clamped so an overflow rewrites the last slot instead of what follows: the buckets in sub_00273E70
 # (24-byte items after their counts: 1536 at +0x12254, 1024 at +0x1B258, 256 at +0x2125C and +0x22A60,
 # the last one followed by the zone's view distance), the tree list sub_0010F0D0 fills (3104 at +0x10),
 # and sub_00273890's copy of it (2048 at +0x2978). NOTES 25.
 ('    ecx = MEM32(esi + 0x22A5C);\n    edx = ecx + ecx * 2;\n',
  '    if (MEM32(esi + 0x22A5C) >= 256) MEM32(esi + 0x22A5C) = 255; /* patch_gen: NOTES 25 */\n    ecx = MEM32(esi + 0x22A5C);\n    edx = ecx + ecx * 2;\n', 3),
 ('    ecx = MEM32(esi + 0x22A5C);\n    ecx = ecx + ecx * 2;\n',
  '    if (MEM32(esi + 0x22A5C) >= 256) MEM32(esi + 0x22A5C) = 255; /* patch_gen: NOTES 25 */\n    ecx = MEM32(esi + 0x22A5C);\n    ecx = ecx + ecx * 2;\n'),
 ('    ecx = MEM32(esi + 0x21258);\n    ecx = ecx + ecx * 2;\n',
  '    if (MEM32(esi + 0x21258) >= 256) MEM32(esi + 0x21258) = 255; /* patch_gen: NOTES 25 */\n    ecx = MEM32(esi + 0x21258);\n    ecx = ecx + ecx * 2;\n', 3),
 ('    ecx = MEM32(esi + 0x1B254);\n    ecx = ecx + ecx * 2;\n',
  '    if (MEM32(esi + 0x1B254) >= 1024) MEM32(esi + 0x1B254) = 1023; /* patch_gen: NOTES 25 */\n    ecx = MEM32(esi + 0x1B254);\n    ecx = ecx + ecx * 2;\n', 2),
 ('    ecx = MEM32(esi + 0x12250);\n    ecx = ecx + ecx * 2;\n',
  '    if (MEM32(esi + 0x12250) >= 1536) MEM32(esi + 0x12250) = 1535; /* patch_gen: NOTES 25 */\n    ecx = MEM32(esi + 0x12250);\n    ecx = ecx + ecx * 2;\n'),
 ('loc_0010F276: ;\n    ecx = MEM32(esp + 0x70);\n',
  'loc_0010F276: ;\n    ecx = MEM32(esp + 0x70);\n    if (MEM32(ecx) >= 3104) MEM32(ecx) = 3103; /* patch_gen: NOTES 25 */\n'),
 ('loc_0010F764: ;\n    ecx = MEM32(esp + 0x70);\n',
  'loc_0010F764: ;\n    ecx = MEM32(esp + 0x70);\n    if (MEM32(ecx) >= 3104) MEM32(ecx) = 3103; /* patch_gen: NOTES 25 */\n'),
 ('loc_00273961: ;\n    eax = MEM32(edi + 0x2974);\n',
  'loc_00273961: ;\n    if (MEM32(edi + 0x2974) >= 2048) MEM32(edi + 0x2974) = 2047; /* patch_gen: NOTES 25 */\n    eax = MEM32(edi + 0x2974);\n'),

 # Boot waited ~20 s before the window opened: the network init (sub_002074E0, after XNetStartup and
 # WSAStartup) polls the check sub_002071F0 with Sleep(100) until XNetGetTitleXnAddr stops answering
 # XNET_GET_XNADDR_PENDING (0). With no NIC, XNet's DHCP gives up only after ~20 s and then answers
 # 0x8002 (ETHERNET | TROUBLESHOOT). Give the check that answer at once; the XNADDR it fills is a local
 # that nothing reads, and XNet's own state is left alone. NOTES 24.
 ('loc_002071FC: ;\n    _cf = 0; /* logical op clears CF */\n    eax = eax & 0xFFFFFFDFu;\n',
  'loc_002071FC: ;\n    if (eax == 0) eax = 0x8002u; /* patch_gen: pending -> no-DHCP answer (NOTES 24) */\n    _cf = 0; /* logical op clears CF */\n    eax = eax & 0xFFFFFFDFu;\n'),
]
files=sorted(glob.glob(GEN+'recomp_*.c'))
ok=True
for patch in PATCHES:
    # (anchor, replacement[, n]): the anchor must occur exactly n times (default 1) over all
    # files; n > 1 is for identical sites, every one of which is replaced.
    anchor,repl=patch[0],patch[1]
    want=patch[2] if len(patch)>2 else 1
    hits=[]; done=0
    for f in files:
        t=open(f,encoding='utf-8',errors='surrogateescape',newline='').read()
        a=anchor.replace('\n','\r\n') if '\r\n' in t[:2000] else anchor
        r=repl.replace('\n','\r\n') if '\r\n' in t[:2000] else repl
        done+=t.count(r)
        if a in t: hits.append((f,t,a,r,t.count(a)))
    if done==want:
        # Already applied (a second run over the same generated files); some replacements still
        # contain their anchor, so this has to be checked first.
        print('already patched:',anchor.splitlines()[0]); continue
    n=sum(h[4] for h in hits)
    if n!=want:
        print('PATCH ANCHOR FOUND',n,'TIMES (WANT',want,'):',anchor.splitlines()[0]); ok=False; continue
    for f,t,a,r,_ in hits:
        open(f,'w',encoding='utf-8',errors='surrogateescape',newline='').write(t.replace(a,r))
        print('patched',f.split('/')[-1])
sys.exit(0 if ok else 1)
