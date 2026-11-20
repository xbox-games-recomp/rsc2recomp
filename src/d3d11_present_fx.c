/*
 * Launcher Display / Graphics options for the original-Xbox D3D11 back ends: window
 * size, fullscreen, FXAA, CAS / FSR 1 scaling, anisotropic filtering, title stats.
 * See d3d11_present_fx.h. The same file is in mm3recomp, pgr2recomp and rsc2recomp.
 *
 * CAS and FSR 1 (EASU + RCAS) follow AMD FidelityFX ffx_cas.h / ffx_fsr1.h (MIT), in
 * plain float HLSL with Load taps; sharpness matches ReXGlue's defaults (CAS additional
 * sharpness 0, RCAS 0.2 stops), so the same setting looks the same in every port.
 */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d3d11_present_fx.h"

enum { FX_BILINEAR, FX_CAS, FX_FSR };

static int s_fxaa;              /* 0 none, 1 FXAA, 2 FXAA (extreme) */
static int s_effect = FX_BILINEAR;
static int s_aniso = -1;        /* ReXGlue numbering: -1 game default, 0 off, 1..5 = 1x..16x */
static int s_title_stats = 1;
static int s_fullscreen;
static int s_widescreen;
static uint32_t s_win_w, s_win_h;

static int env_int(const char *name, int def)
{
    const char *e = getenv(name);
    return e && *e ? atoi(e) : def;
}

void pfx_read_options(void)
{
    const char *e;
    s_fxaa = env_int("RECOMP_D3D11_FXAA", 0);
    if (s_fxaa < 0 || s_fxaa > 2) s_fxaa = 1;
    e = getenv("RECOMP_D3D11_PRESENT_EFFECT");
    s_effect = !e ? FX_BILINEAR : !_stricmp(e, "cas") ? FX_CAS : !_stricmp(e, "fsr") ? FX_FSR : FX_BILINEAR;
    s_aniso = env_int("RECOMP_D3D11_ANISO", -1);
    s_title_stats = env_int("RECOMP_TITLE_STATS", 1) != 0;
    s_fullscreen = env_int("RECOMP_FULLSCREEN", 0) != 0;
    s_widescreen = env_int("RECOMP_WIDESCREEN", 0) != 0;
    s_win_w = (uint32_t)env_int("RECOMP_WINDOW_WIDTH", 0);
    s_win_h = (uint32_t)env_int("RECOMP_WINDOW_HEIGHT", 0);
}

/* ---- Display ------------------------------------------------------------ */

void pfx_window_size(uint32_t *w, uint32_t *h, uint32_t def_w, uint32_t def_h)
{
    *w = s_win_w >= 160 && s_win_w <= 16384 ? s_win_w : def_w;
    *h = s_win_h >= 120 && s_win_h <= 16384 ? s_win_h : def_h;
}

int pfx_fullscreen(void) { return s_fullscreen; }

int pfx_widescreen(void) { return s_widescreen; }

float pfx_aspect(void) { return s_widescreen ? 16.0f / 9.0f : 4.0f / 3.0f; }

static WINDOWPLACEMENT s_saved_place = { sizeof(WINDOWPLACEMENT) };
static int s_is_fullscreen;

void pfx_set_fullscreen(HWND hwnd, int on)
{
    if (!hwnd || on == s_is_fullscreen)
        return;
    if (on) {
        MONITORINFO mi = { sizeof mi };
        GetWindowPlacement(hwnd, &s_saved_place);
        GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    } else {
        SetWindowLongA(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(hwnd, &s_saved_place);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    s_is_fullscreen = on;
}

int pfx_handle_key(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_SYSKEYDOWN && w == VK_RETURN && (l & (1 << 29)) && !(l & (1 << 30))) {
        pfx_set_fullscreen(hwnd, !s_is_fullscreen);
        return 1;
    }
    return msg == WM_SYSKEYDOWN && w == VK_RETURN;   /* Alt+Enter repeat: not Start */
}

/* ---- Graphics options ---------------------------------------------------- */

int pfx_title_stats(void) { return s_title_stats; }

UINT pfx_anisotropy(void)
{
    if (s_aniso < 2) return 0;
    return 1u << (s_aniso > 5 ? 4 : s_aniso - 1);
}

int pfx_active(void) { return s_fxaa != 0 || s_effect != FX_BILINEAR; }

const char *pfx_describe(void)
{
    static char buf[64];
    buf[0] = 0;
    if (s_fxaa) strcat(buf, s_fxaa == 2 ? ", FXAA (extreme)" : ", FXAA");
    if (s_effect == FX_CAS) strcat(buf, ", CAS");
    if (s_effect == FX_FSR) strcat(buf, ", FSR 1");
    if (pfx_anisotropy()) sprintf(buf + strlen(buf), ", %ux anisotropic", pfx_anisotropy());
    return buf;
}

/* ---- Shaders ------------------------------------------------------------- */

static const char s_fx_hlsl[] =
"Texture2D tex : register(t0);\n"
"SamplerState smp : register(s0);\n"
/* src: input w, h, 1/w, 1/h; dst: output origin x, y (render-target pixels), w, h;
 * prm: CAS peak, RCAS sharpness. */
"cbuffer FXCB : register(b0) { float4 src; float4 dst; float4 prm; };\n"
"struct BOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
"float3 T(int2 p) { return tex.Load(int3(clamp(p, int2(0, 0), int2(src.xy) - 1), 0)).rgb; }\n"
"float L(float3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }\n"
"float4 BLIT(BOut i) : SV_Target { return float4(tex.SampleLevel(smp, i.uv, 0).rgb, 1); }\n"
/* FXAA, Lottes' console variant. FXAA_EXTREME: lower edge thresholds, a longer span
 * and FXAA 3.11's sub-pixel blend, so thin and sub-pixel detail is smoothed too. */
"#ifdef FXAA_EXTREME\n"
"#define EDGE_MIN 0.0156\n#define EDGE_REL 0.063\n#define SPAN 16.0\n"
"#else\n"
"#define EDGE_MIN 0.0312\n#define EDGE_REL 0.125\n#define SPAN 8.0\n"
"#endif\n"
"float fxl(float2 uv) { return dot(tex.SampleLevel(smp, uv, 0).rgb, float3(0.299, 0.587, 0.114)); }\n"
"float4 FXAA(BOut i) : SV_Target {\n"
"  float2 rc = src.zw, uv = i.uv;\n"
"  float3 cm = tex.SampleLevel(smp, uv, 0).rgb;\n"
"  float lm = dot(cm, float3(0.299, 0.587, 0.114));\n"
"  float nw = fxl(uv + float2(-0.5, -0.5) * rc), ne = fxl(uv + float2(0.5, -0.5) * rc);\n"
"  float sw = fxl(uv + float2(-0.5, 0.5) * rc), se = fxl(uv + float2(0.5, 0.5) * rc);\n"
"  float lo = min(lm, min(min(nw, ne), min(sw, se))), hi = max(lm, max(max(nw, ne), max(sw, se)));\n"
"  if (hi - lo < max(EDGE_MIN, hi * EDGE_REL)) return float4(cm, 1);\n"
"  float2 dir = float2((sw + se) - (nw + ne), (nw + sw) - (ne + se));\n"
"  float red = max((nw + ne + sw + se) * (0.25 * 0.125), 1.0 / 128.0);\n"
"  dir = clamp(dir / (min(abs(dir.x), abs(dir.y)) + red), -SPAN, SPAN) * rc;\n"
"  float3 a = 0.5 * (tex.SampleLevel(smp, uv - dir / 6.0, 0).rgb + tex.SampleLevel(smp, uv + dir / 6.0, 0).rgb);\n"
"  float3 b = a * 0.5 + 0.25 * (tex.SampleLevel(smp, uv - dir * 0.5, 0).rgb + tex.SampleLevel(smp, uv + dir * 0.5, 0).rgb);\n"
"  float lb = dot(b, float3(0.299, 0.587, 0.114));\n"
"  float3 c = (lb < lo || lb > hi) ? a : b;\n"
"#ifdef FXAA_EXTREME\n"
"  int2 p = int2(i.pos.xy);\n"
"  float3 n = T(p + int2(0, -1)), s = T(p + int2(0, 1)), w = T(p + int2(-1, 0)), e = T(p + int2(1, 0));\n"
"  float3 avg = (2.0 * (n + s + w + e) + T(p + int2(-1, -1)) + T(p + int2(1, -1)) + T(p + int2(-1, 1)) + T(p + int2(1, 1))) / 12.0;\n"
"  float sub = saturate(abs(dot(avg, float3(0.299, 0.587, 0.114)) - lm) / (hi - lo));\n"
"  sub = sub * sub * (3.0 - 2.0 * sub);\n"
"  c = lerp(c, avg, sub * sub * 0.75);\n"
"#endif\n"
"  return float4(c, 1);\n"
"}\n"
/* CAS, sharpen only (ffx_cas.h CasFilter without scaling). */
"float4 CAS(BOut i) : SV_Target {\n"
"  int2 p = int2(i.pos.xy - dst.xy);\n"
"  float3 a = T(p + int2(-1, -1)), b = T(p + int2(0, -1)), c = T(p + int2(1, -1));\n"
"  float3 d = T(p + int2(-1, 0)), e = T(p), f = T(p + int2(1, 0));\n"
"  float3 g = T(p + int2(-1, 1)), h = T(p + int2(0, 1)), k = T(p + int2(1, 1));\n"
"  float3 mn = min(min(min(d, e), min(f, b)), h);\n"
"  float3 mn2 = min(mn, min(min(a, c), min(g, k)));\n"
"  mn += mn2;\n"
"  float3 mx = max(max(max(d, e), max(f, b)), h);\n"
"  float3 mx2 = max(mx, max(max(a, c), max(g, k)));\n"
"  mx += mx2;\n"
"  float3 amp = sqrt(saturate(min(mn, 2.0 - mx) / max(mx, 1e-5)));\n"
"  float3 w = amp * prm.x;\n"
"  return float4(saturate(((b + d + f + h) * w + e) / (1.0 + 4.0 * w)), 1);\n"
"}\n"
/* FSR 1 EASU (ffx_fsr1.h FsrEasuF): 12 taps around the source position, a Lanczos-2
 * lobe stretched along the local edge direction, clamped to the nearest 2x2. */
"void easu_set(inout float2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE) {\n"
"  float dc = lD - lC, cb = lC - lB;\n"
"  float lenX = max(abs(dc), abs(cb)); lenX = lenX > 0.0 ? 1.0 / lenX : 0.0;\n"
"  float dirX = lD - lB;\n"
"  dir.x += dirX * w;\n"
"  lenX = saturate(abs(dirX) * lenX); lenX *= lenX;\n"
"  len += lenX * w;\n"
"  float ec = lE - lC, ca = lC - lA;\n"
"  float lenY = max(abs(ec), abs(ca)); lenY = lenY > 0.0 ? 1.0 / lenY : 0.0;\n"
"  float dirY = lE - lA;\n"
"  dir.y += dirY * w;\n"
"  lenY = saturate(abs(dirY) * lenY); lenY *= lenY;\n"
"  len += lenY * w;\n"
"}\n"
"void easu_tap(inout float3 aC, inout float aW, float2 off, float2 dir, float2 len, float lob, float clp, float3 c) {\n"
"  float2 v = float2(off.x * dir.x + off.y * dir.y, off.x * -dir.y + off.y * dir.x) * len;\n"
"  float d2 = min(v.x * v.x + v.y * v.y, clp);\n"
"  float wB = 2.0 / 5.0 * d2 - 1.0, wA = lob * d2 - 1.0;\n"
"  wB *= wB; wA *= wA;\n"
"  wB = 25.0 / 16.0 * wB - (25.0 / 16.0 - 1.0);\n"
"  float w = wB * wA;\n"
"  aC += c * w; aW += w;\n"
"}\n"
"float4 EASU(BOut i) : SV_Target {\n"
"  float2 pp = (i.pos.xy - dst.xy) * (src.xy / dst.zw) - 0.5;\n"
"  float2 fp = floor(pp); pp -= fp;\n"
"  int2 q = int2(fp);\n"
"  float3 bC = T(q + int2(0, -1)), cC = T(q + int2(1, -1));\n"
"  float3 eC = T(q + int2(-1, 0)), fC = T(q), gC = T(q + int2(1, 0)), hC = T(q + int2(2, 0));\n"
"  float3 iC = T(q + int2(-1, 1)), jC = T(q + int2(0, 1)), kC = T(q + int2(1, 1)), lC = T(q + int2(2, 1));\n"
"  float3 nC = T(q + int2(0, 2)), oC = T(q + int2(1, 2));\n"
"  float bL = L(bC), cL = L(cC), eL = L(eC), fL = L(fC), gL = L(gC), hL = L(hC);\n"
"  float iL = L(iC), jL = L(jC), kL = L(kC), lL = L(lC), nL = L(nC), oL = L(oC);\n"
"  float2 dir = 0; float len = 0;\n"
"  easu_set(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);\n"
"  easu_set(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);\n"
"  easu_set(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);\n"
"  easu_set(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);\n"
"  float dirR = dir.x * dir.x + dir.y * dir.y;\n"
"  bool zro = dirR < 1.0 / 32768.0;\n"
"  dirR = zro ? 1.0 : rsqrt(dirR);\n"
"  dir.x = zro ? 1.0 : dir.x;\n"
"  dir *= dirR;\n"
"  len = len * 0.5; len *= len;\n"
"  float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));\n"
"  float2 len2 = float2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);\n"
"  float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;\n"
"  float clp = 1.0 / lob;\n"
"  float3 mn4 = min(min(fC, gC), min(jC, kC)), mx4 = max(max(fC, gC), max(jC, kC));\n"
"  float3 aC = 0; float aW = 0;\n"
"  easu_tap(aC, aW, float2(0, -1) - pp, dir, len2, lob, clp, bC);\n"
"  easu_tap(aC, aW, float2(1, -1) - pp, dir, len2, lob, clp, cC);\n"
"  easu_tap(aC, aW, float2(-1, 1) - pp, dir, len2, lob, clp, iC);\n"
"  easu_tap(aC, aW, float2(0, 1) - pp, dir, len2, lob, clp, jC);\n"
"  easu_tap(aC, aW, float2(0, 0) - pp, dir, len2, lob, clp, fC);\n"
"  easu_tap(aC, aW, float2(-1, 0) - pp, dir, len2, lob, clp, eC);\n"
"  easu_tap(aC, aW, float2(1, 1) - pp, dir, len2, lob, clp, kC);\n"
"  easu_tap(aC, aW, float2(2, 1) - pp, dir, len2, lob, clp, lC);\n"
"  easu_tap(aC, aW, float2(2, 0) - pp, dir, len2, lob, clp, hC);\n"
"  easu_tap(aC, aW, float2(1, 0) - pp, dir, len2, lob, clp, gC);\n"
"  easu_tap(aC, aW, float2(1, 2) - pp, dir, len2, lob, clp, oC);\n"
"  easu_tap(aC, aW, float2(0, 2) - pp, dir, len2, lob, clp, nC);\n"
"  return float4(min(mx4, max(mn4, aC / aW)), 1);\n"
"}\n"
/* FSR 1 RCAS (ffx_fsr1.h FsrRcasF), without the noise detection. */
"float4 RCAS(BOut i) : SV_Target {\n"
"  int2 p = int2(i.pos.xy - dst.xy);\n"
"  float3 b = T(p + int2(0, -1)), d = T(p + int2(-1, 0)), e = T(p), f = T(p + int2(1, 0)), h = T(p + int2(0, 1));\n"
"  float3 mn4 = min(min(b, d), min(f, h)), mx4 = max(max(b, d), max(f, h));\n"
"  float3 hitMin = min(mn4, e) / max(4.0 * mx4, 1e-5);\n"
"  float3 hitMax = (1.0 - max(mx4, e)) / min(4.0 * mn4 - 4.0, -1e-5);\n"
"  float3 lobeC = max(-hitMin, hitMax);\n"
"  float lobe = max(-(0.25 - 1.0 / 16.0), min(max(lobeC.r, max(lobeC.g, lobeC.b)), 0.0)) * prm.y;\n"
"  return float4(saturate((lobe * (b + d + h + f) + e) / (4.0 * lobe + 1.0)), 1);\n"
"}\n";

typedef struct { float src[4], dst[4], prm[4]; } FxCB;

static ID3D11Device *s_dev;
static ID3D11PixelShader *s_blit, *s_fxaa_ps, *s_cas, *s_easu, *s_rcas;
static ID3D11Buffer *s_cb;
static int s_ok;

typedef struct {
    ID3D11Texture2D *tex;
    ID3D11RenderTargetView *rtv;
    ID3D11ShaderResourceView *srv;
    uint32_t w, h;
} Target;
static Target s_t[3];   /* 0: scan-out source, 1: second at source size, 2: picture size */

static ID3D11PixelShader *make_ps(const char *entry, int extreme)
{
    D3D_SHADER_MACRO defs[] = { { "FXAA_EXTREME", "1" }, { NULL, NULL } };
    ID3DBlob *code = NULL, *err = NULL;
    ID3D11PixelShader *ps = NULL;
    HRESULT hr = D3DCompile(s_fx_hlsl, sizeof s_fx_hlsl - 1, "present_fx", extreme ? defs : NULL, NULL, entry,
                            "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11] present shader %s: %s\n", entry, err ? (const char *)ID3D10Blob_GetBufferPointer(err) : "?");
        if (err) ID3D10Blob_Release(err);
        return NULL;
    }
    if (err) ID3D10Blob_Release(err);
    ID3D11Device_CreatePixelShader(s_dev, ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), NULL, &ps);
    ID3D10Blob_Release(code);
    return ps;
}

int pfx_init(ID3D11Device *dev)
{
    D3D11_BUFFER_DESC bd;
    s_dev = dev;
    if (!pfx_active())
        return 1;
    s_blit = make_ps("BLIT", 0);
    if (s_fxaa) s_fxaa_ps = make_ps("FXAA", s_fxaa == 2);
    if (s_effect == FX_CAS) s_cas = make_ps("CAS", 0);
    if (s_effect == FX_FSR) { s_easu = make_ps("EASU", 0); s_rcas = make_ps("RCAS", 0); }
    memset(&bd, 0, sizeof bd);
    bd.ByteWidth = sizeof(FxCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ID3D11Device_CreateBuffer(dev, &bd, NULL, &s_cb);
    s_ok = s_blit && s_cb && (!s_fxaa || s_fxaa_ps) && (s_effect != FX_CAS || s_cas) &&
           (s_effect != FX_FSR || (s_easu && s_rcas));
    if (!s_ok) {
        fprintf(stderr, "[D3D11] present effects unavailable: plain scaling\n");
        s_fxaa = 0;
        s_effect = FX_BILINEAR;
    }
    return s_ok;
}

static int ensure_target(Target *t, uint32_t w, uint32_t h)
{
    D3D11_TEXTURE2D_DESC td;
    if (t->tex && t->w == w && t->h == h)
        return 1;
    if (t->srv) ID3D11ShaderResourceView_Release(t->srv);
    if (t->rtv) ID3D11RenderTargetView_Release(t->rtv);
    if (t->tex) ID3D11Texture2D_Release(t->tex);
    memset(t, 0, sizeof *t);
    memset(&td, 0, sizeof td);
    td.Width = w; td.Height = h;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(ID3D11Device_CreateTexture2D(s_dev, &td, NULL, &t->tex)))
        return 0;
    ID3D11Device_CreateRenderTargetView(s_dev, (ID3D11Resource *)t->tex, NULL, &t->rtv);
    ID3D11Device_CreateShaderResourceView(s_dev, (ID3D11Resource *)t->tex, NULL, &t->srv);
    t->w = w; t->h = h;
    return t->rtv && t->srv;
}

void pfx_source_size(uint32_t src_w, uint32_t src_h, uint32_t pic_w, uint32_t pic_h, uint32_t *out_w, uint32_t *out_h)
{
    /* Growing: keep the guest image's own pixels for FXAA and the upscaler. Shrinking:
     * the scan-out's box filter averages it down to the picture first. */
    if (pic_w >= src_w && pic_h >= src_h) { *out_w = src_w; *out_h = src_h; }
    else { *out_w = pic_w; *out_h = pic_h; }
    if (!*out_w) *out_w = 1;
    if (!*out_h) *out_h = 1;
}

ID3D11RenderTargetView *pfx_source_target(uint32_t w, uint32_t h)
{
    if (!s_ok || !ensure_target(&s_t[0], w, h))
        return NULL;
    return s_t[0].rtv;
}

/* One full-screen pass: `in` -> `out` (a render target) over the viewport `vp`. */
static void pass(ID3D11DeviceContext *ctx, ID3D11PixelShader *ps, const Target *in, ID3D11RenderTargetView *out,
                 const D3D11_VIEWPORT *vp)
{
    FxCB cb;
    ID3D11ShaderResourceView *none = NULL;
    memset(&cb, 0, sizeof cb);
    cb.src[0] = (float)in->w; cb.src[1] = (float)in->h;
    cb.src[2] = 1.0f / (float)in->w; cb.src[3] = 1.0f / (float)in->h;
    cb.dst[0] = vp->TopLeftX; cb.dst[1] = vp->TopLeftY;
    cb.dst[2] = vp->Width; cb.dst[3] = vp->Height;
    cb.prm[0] = -1.0f / 8.0f;                 /* CAS peak: -1 / lerp(8, 5, sharpness 0) */
    cb.prm[1] = exp2f(-0.2f);                 /* RCAS: 0.2 stops below full sharpness */
    ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)s_cb, 0, NULL, &cb, 0, 0);
    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &out, NULL);
    ID3D11DeviceContext_RSSetViewports(ctx, 1, vp);
    ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &s_cb);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &in->srv);
    ID3D11DeviceContext_Draw(ctx, 3, 0);
    ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &none);
}

void pfx_present(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *back, const D3D11_VIEWPORT *pic,
                 ID3D11VertexShader *fullscreen_vs, ID3D11SamplerState *linear_clamp)
{
    const Target *cur = &s_t[0];
    D3D11_VIEWPORT whole;
    uint32_t pw = (uint32_t)pic->Width, ph = (uint32_t)pic->Height;
    int same = cur->w == pw && cur->h == ph;

    if (!s_ok || !cur->srv)
        return;
    memset(&whole, 0, sizeof whole);
    whole.Width = (float)cur->w; whole.Height = (float)cur->h;
    whole.MaxDepth = 1;
    ID3D11DeviceContext_VSSetShader(ctx, fullscreen_vs, NULL, 0);
    ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &linear_clamp);
    ID3D11DeviceContext_RSSetState(ctx, NULL);
    ID3D11DeviceContext_OMSetBlendState(ctx, NULL, NULL, 0xFFFFFFFFu);
    ID3D11DeviceContext_OMSetDepthStencilState(ctx, NULL, 0);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (s_fxaa && ensure_target(&s_t[1], cur->w, cur->h)) {
        pass(ctx, s_fxaa_ps, cur, s_t[1].rtv, &whole);
        cur = &s_t[1];
    }
    if (s_effect == FX_CAS) {
        if (same) { pass(ctx, s_cas, cur, back, pic); return; }
        {
            Target *o = cur == &s_t[0] ? &s_t[1] : &s_t[0];
            if (ensure_target(o, cur->w, cur->h)) {
                pass(ctx, s_cas, cur, o->rtv, &whole);
                cur = o;
            }
        }
    } else if (s_effect == FX_FSR) {
        if (same) { pass(ctx, s_rcas, cur, back, pic); return; }
        if (ensure_target(&s_t[2], pw, ph)) {
            D3D11_VIEWPORT pv = whole;
            pv.Width = (float)pw; pv.Height = (float)ph;
            pass(ctx, s_easu, cur, s_t[2].rtv, &pv);
            pass(ctx, s_rcas, &s_t[2], back, pic);
            return;
        }
    }
    pass(ctx, s_blit, cur, back, pic);
}
