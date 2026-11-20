/*
 * Launcher options shared by the original-Xbox D3D11 back ends (the same file in
 * mm3recomp, pgr2recomp and rsc2recomp), so these ports offer the same Display and
 * Graphics settings as the Xbox 360 (ReXGlue) ones:
 *
 *   RECOMP_WINDOW_WIDTH / RECOMP_WINDOW_HEIGHT   initial client size (default 960x720)
 *   RECOMP_FULLSCREEN=1                          borderless fullscreen; Alt+Enter toggles
 *   RECOMP_WIDESCREEN=1                          16:9: the toolkit reports a widescreen TV
 *                                                (titles that support it draw anamorphic
 *                                                16:9) and the picture is shown at 16:9
 *   RECOMP_D3D11_FXAA=0|1|2                      anti-aliasing: none, FXAA, FXAA (extreme)
 *   RECOMP_D3D11_PRESENT_EFFECT=bilinear|cas|fsr scaling to the window
 *   RECOMP_D3D11_ANISO=-1..5                     anisotropic filtering: game default, off,
 *                                                1x, 2x, 4x, 8x, 16x (ReXGlue's numbering)
 *   RECOMP_TITLE_STATS=0                         plain window title, no fps / draw counts
 *
 * Present path when FXAA or CAS/FSR is on: the back end scans out (with the video
 * overlay) into pfx_source_target(), at the guest image's size when the window is
 * larger, or the picture's size when it is smaller (the scan-out averages it down);
 * pfx_present() then runs FXAA at that size and scales to the window with the chosen
 * filter: bilinear, CAS (sharpen, then bilinear) or FSR 1 (EASU up, then RCAS).
 */
#ifndef D3D11_PRESENT_FX_H
#define D3D11_PRESENT_FX_H

#include <windows.h>
#include <d3d11.h>
#include <stdint.h>

/* Reads the options. Call once, before the window is created. */
void pfx_read_options(void);

/* Display. */
void pfx_window_size(uint32_t *w, uint32_t *h, uint32_t def_w, uint32_t def_h);
int pfx_fullscreen(void);
/* RECOMP_WIDESCREEN: the picture's shape, width / height (16:9 or 4:3). */
int pfx_widescreen(void);
float pfx_aspect(void);
/* Puts `hwnd` in borderless fullscreen on its monitor, or back to the saved window. */
void pfx_set_fullscreen(HWND hwnd, int on);
/* Alt+Enter in the window procedure: returns 1 when it handled the key. */
int pfx_handle_key(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

/* Graphics. */
int pfx_title_stats(void);
/* 0 = the game's own filtering, else the D3D11 MaxAnisotropy (2..16). */
UINT pfx_anisotropy(void);
/* Non-zero when present needs the effect path (FXAA, CAS or FSR). */
int pfx_active(void);
/* "FXAA, FSR" etc. for the device-ready log line ("" when off). */
const char *pfx_describe(void);

/* Device objects; call after the device exists. Returns 0 on failure (effects off). */
int pfx_init(ID3D11Device *dev);
/* The size the scan-out should render at for a guest image of src_w x src_h shown in a
 * pic_w x pic_h picture. */
void pfx_source_size(uint32_t src_w, uint32_t src_h, uint32_t pic_w, uint32_t pic_h,
                     uint32_t *out_w, uint32_t *out_h);
/* Render target for the scan-out at w x h (null on failure). */
ID3D11RenderTargetView *pfx_source_target(uint32_t w, uint32_t h);
/* Runs FXAA / scaling from the source target into `back` at `pic` (the picture
 * rectangle; the caller has cleared the back buffer). Leaves no SRVs bound. */
void pfx_present(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *back, const D3D11_VIEWPORT *pic,
                 ID3D11VertexShader *fullscreen_vs, ID3D11SamplerState *linear_clamp);

#endif
