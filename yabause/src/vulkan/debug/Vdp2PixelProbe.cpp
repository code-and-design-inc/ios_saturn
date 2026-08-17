// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2PixelProbe implementation.
//
// Reference sources used for bit-exact transcription (no runtime calls):
//   vidsoft.c:209-238  Vdp2ColorRamGetColorSoft  -> cramReadColor()
//   vidsoft.c:242-323  Vdp2PatternAddr            -> decodePatternAddr()
//   vidsoft.c:388-436  Vdp2FetchPixel             -> fetchTilePixel()
//   vidsoft.c:574-687  Vdp2MapCalcXY              -> mapCalcXY()
//   vidsoft.c:691-735  SetupScreenVars            -> setupScreenInfo()
//   vidsoft.c:1556-1688 Vdp2DrawNBG0              -> probeNBGn() NBG0 branch
//   vidsoft.c:1706-1793 Vdp2DrawNBG1              -> probeNBGn() NBG1 branch
//   vidsoft.c:1812-1865 Vdp2DrawNBG2              -> probeNBGn() NBG2 branch
//   vidsoft.c:1884-1939 Vdp2DrawNBG3              -> probeNBGn() NBG3 branch
//   vidshared.c:53-153  Vdp2NBGnPlaneAddr         -> nbgnPlaneAddr()
//   vidshared.h:399-438 CalcPlaneAddr             -> calcPlaneAddr()
//   vidshared.h:490-544 ReadPlaneSize / ReadPatternData / ReadBitmapSize
//   vidsoft.c:1450-1494 Vdp2DrawBackScreen        -> decodeBackScreen()
//   vulkan/Vdp2ColorCalcState.h decodeVdp2ColorCalc -> fill ccEnable / ccRatio
//
// Corrections vs task-brief summary (verified against vidogl.c / vidsoft.c):
//   * NBG2 colornumber: `(CHCTLB & 0x2) >> 1`  (vidsoft.c:1823)
//   * NBG3 colornumber: `(CHCTLB & 0x20) >> 5` (vidsoft.c:1895)
//   * NBG2 char base from MPOFN: offset = `(MPOFN & 0x700) >> 2` (vidshared.c:107)
//   * NBG3 char base from MPOFN: offset = `(MPOFN & 0x7000) >> 6` (vidshared.c:133)
//   * NBG0 coordincx/y: `(ZMXN0.all & 0x7FF00) / 65536.0f` (vidsoft.c:1629-1630)
//     note: if the field is zero the result is 0, which breaks zoom; treat 0 as 1.
//   * CRAM color mode: derive from `(RAMCTL >> 12) & 0x3` since Vdp2Internal is
//     not available during pause (we replicate the formula inline).
//
// ASCII-only source (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include "Vdp2PixelProbe.h"
#include "../Vdp2ColorCalcState.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers -- all work on const uint8_t* buffers, no globals.
// ---------------------------------------------------------------------------
namespace {

// ---------------------------------------------------------------------------
// Byte-swap helpers matching memory.h T1ReadWord / T2ReadWord on little-endian.
// The host (Windows x86-64) is little-endian; Saturn VRAM is stored big-endian
// as T1ReadWord / T1ReadByte expect (BSWAP at read).
// ---------------------------------------------------------------------------

// T1ReadByte: no swap needed (byte is byte).
static inline uint8_t vramByte(const uint8_t* mem, uint32_t addr)
{
    return mem[addr & 0x7FFFF];
}

// T1ReadWord: big-endian 16-bit from VRAM (swap on little-endian host).
static inline uint16_t vramWord(const uint8_t* mem, uint32_t addr)
{
    addr &= 0x7FFFF;
    uint16_t raw;
    memcpy(&raw, mem + addr, 2);
    // byte-swap to convert big-endian Saturn -> host little-endian
    return static_cast<uint16_t>((raw >> 8) | (raw << 8));
}

// T2ReadWord: CRAM uses type-2 memory (no swap, direct u16 read).
static inline uint16_t cramWord(const uint8_t* mem, uint32_t addr)
{
    addr &= 0xFFF;
    uint16_t raw;
    memcpy(&raw, mem + addr, 2);
    return raw; // T2ReadWord: no byte swap on LE or BE
}

// T2ReadLong: for CRAM 32bpp entries.
static inline uint32_t cramLong(const uint8_t* mem, uint32_t addr)
{
    addr &= 0xFFF;
    // T2ReadLong on LE: WSWAP32 (swap the two 16-bit halves, keep each half as-is)
    uint32_t raw;
    memcpy(&raw, mem + addr, 4);
    // WSWAP32: ((raw >> 16) | (raw << 16))
    return (raw >> 16) | (raw << 16);
}

// ---------------------------------------------------------------------------
// Derive CRAM color mode from RAMCTL register (mirrors Vdp2Internal.ColorMode).
//   RAMCTL bits 13:12 = color mode
//     0 = RGB555 (16-bit CRAM, 1024 entries)
//     1 = RGB555 (same, bank-split variant)
//     2 = RGB888 (32-bit CRAM, 512 entries)
// ---------------------------------------------------------------------------
static inline int cramColorMode(const Vdp2& regs)
{
    return (regs.RAMCTL >> 12) & 0x3;
}

// ---------------------------------------------------------------------------
// Vdp2ColorRamGetColorSoft -- vidsoft.c:209-238
// Returns 0x00RRGGBB with the Saturn MSB in bit 31 for mode 0/1,
// or raw 32-bit long for mode 2.
// ---------------------------------------------------------------------------
static uint32_t cramReadColor(uint32_t addr, const uint8_t* cram, int colorMode)
{
    switch (colorMode) {
    case 0:
    case 1: {
        // 16-bit CRAM: addr is a 16-bit index, multiply by 2.
        uint32_t byteAddr = (addr << 1) & 0xFFF;
        uint16_t tmp = cramWord(cram, byteAddr);
        // Expand RGB555 -> 0x00RRGGBB, preserve MSB in bit 31.
        uint32_t r = (tmp & 0x001F) << 3;
        uint32_t g = (tmp & 0x03E0) << 6;
        uint32_t b = (tmp & 0x7C00) << 9;
        uint32_t msb = (tmp & 0x8000) ? 0x80000000u : 0u;
        return msb | b | g | r;
    }
    case 2: {
        // 32-bit CRAM: addr is a 32-bit index, multiply by 4.
        uint32_t byteAddr = (addr << 2) & 0xFFF;
        return cramLong(cram, byteAddr);
    }
    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Local copies of CalcPlaneAddr / ReadPlaneSize / ReadPatternData.
// These replicate the inline functions from vidshared.h exactly.
// ---------------------------------------------------------------------------

struct PlaneInfo {
    int planew, planew_bits;
    int planeh, planeh_bits;
};

struct PatternInfo {
    int patterndatasize, patterndatasize_bits; // 1 or 2
    int patternwh, patternwh_bits;             // 1 or 2
    int pagewh, pagewh_bits;
    int cellw, cellh, cellw_bits, cellh_bits;
    uint16_t supplementdata;
    int auxmode;
};

static void readPlaneSize(PlaneInfo& pi, uint16_t reg)
{
    switch (reg & 0x3) {
    case 0: pi.planew = pi.planeh = 1; pi.planew_bits = pi.planeh_bits = 0; break;
    case 1: pi.planew = 2; pi.planew_bits = 1; pi.planeh = 1; pi.planeh_bits = 0; break;
    case 3: pi.planew = pi.planeh = 2; pi.planew_bits = pi.planeh_bits = 1; break;
    default: pi.planew = pi.planeh = 1; pi.planew_bits = pi.planeh_bits = 0; break;
    }
}

static void readPatternData(PatternInfo& pat, uint16_t pnc, int chctlwh)
{
    if (pnc & 0x8000) { pat.patterndatasize = 1; pat.patterndatasize_bits = 0; }
    else               { pat.patterndatasize = 2; pat.patterndatasize_bits = 1; }

    if (chctlwh) { pat.patternwh = 2; pat.patternwh_bits = 1; }
    else          { pat.patternwh = 1; pat.patternwh_bits = 0; }

    pat.pagewh       = 64 >> pat.patternwh_bits;
    pat.pagewh_bits  = 6 - pat.patternwh_bits;
    pat.cellw = pat.cellh = 8;
    pat.cellw_bits = pat.cellh_bits = 3;
    pat.supplementdata = pnc & 0x3FF;
    pat.auxmode = (pnc & 0x4000) >> 14;
}

// Calculate plane base address from the map entry `tmp`.
// Mirrors CalcPlaneAddr in vidshared.h.
static uint32_t calcPlaneAddr(const PlaneInfo& pi, const PatternInfo& pat,
                              uint32_t tmp, bool vrsize8Mbit)
{
    int deca  = pi.planeh + pi.planew - 2;
    int multi = pi.planeh * pi.planew;
    if (vrsize8Mbit) {
        if (pat.patterndatasize == 1) {
            if (pat.patternwh == 1)
                return ((tmp & 0x3F) >> deca) * (uint32_t)(multi * 0x2000);
            else
                return (tmp >> deca) * (uint32_t)(multi * 0x800);
        } else {
            if (pat.patternwh == 1)
                return ((tmp & 0x1F) >> deca) * (uint32_t)(multi * 0x4000);
            else
                return ((tmp & 0x7F) >> deca) * (uint32_t)(multi * 0x1000);
        }
    } else {
        if (pat.patterndatasize == 1) {
            if (pat.patternwh == 1)
                return ((tmp & 0x3F) >> deca) * (uint32_t)(multi * 0x2000);
            else
                return ((tmp & 0xFF) >> deca) * (uint32_t)(multi * 0x800);
        } else {
            if (pat.patternwh == 1)
                return ((tmp & 0x1F) >> deca) * (uint32_t)(multi * 0x4000);
            else
                return ((tmp & 0x7F) >> deca) * (uint32_t)(multi * 0x1000);
        }
    }
}

// ---------------------------------------------------------------------------
// Build the plane address table for one scroll layer (up to 4 planes for
// NBG0-3 with mapwh=2).
// planeAddrs[4] is filled on return.
// `mapwh` = 2 for NBG0-3.
// `offset` and `mapReg{AB,CD}` are the decoded MPOFN field and map registers.
// ---------------------------------------------------------------------------
static void buildNBGPlaneTable(uint32_t planeAddrs[4],
                               uint32_t mpofnOffset,
                               uint16_t mpabn,
                               uint16_t mpcdn,
                               const PlaneInfo& pi,
                               const PatternInfo& pat,
                               bool vrsize8Mbit)
{
    uint32_t tmps[4];
    tmps[0] = mpofnOffset | (mpabn & 0xFF);
    tmps[1] = mpofnOffset | (mpabn >> 8);
    tmps[2] = mpofnOffset | (mpcdn & 0xFF);
    tmps[3] = mpofnOffset | (mpcdn >> 8);
    for (int i = 0; i < 4; ++i)
        planeAddrs[i] = calcPlaneAddr(pi, pat, tmps[i], vrsize8Mbit);
}

// ---------------------------------------------------------------------------
// Decode one pattern name entry (1-word or 2-word) at `addr` in VRAM.
// Fills charaddr, paladdr, flipfunction.
// Mirrors Vdp2PatternAddr in vidsoft.c:242-323.
// On return, charaddr is the byte offset into VRAM for the character data.
// ---------------------------------------------------------------------------
struct PatternName {
    uint32_t charaddr;
    uint32_t paladdr;
    int flipfunction;
    int specialfunction;
    int specialcolorfunction;
};

static PatternName decodePatternAddr(uint32_t addr,
                                     const PatternInfo& pat,
                                     int colornumber,
                                     bool vrsize8Mbit,
                                     const uint8_t* vram)
{
    PatternName pn{};
    if (pat.patterndatasize == 1) {
        uint16_t tmp = vramWord(vram, addr);
        pn.specialfunction      = (pat.supplementdata >> 9) & 0x1;
        pn.specialcolorfunction = (pat.supplementdata >> 8) & 0x1;

        if (colornumber == 0) {
            // 16 colors: paladdr uses high nibble + supplementdata
            pn.paladdr = ((tmp & 0xF000) >> 8) | ((pat.supplementdata & 0xE0) << 3);
        } else {
            pn.paladdr = (tmp & 0x7000) >> 4;
        }

        switch (pat.auxmode) {
        case 0:
            pn.flipfunction = (tmp & 0xC00) >> 10;
            if (pat.patternwh == 1)
                pn.charaddr = (tmp & 0x3FF) | ((pat.supplementdata & 0x1F) << 10);
            else
                pn.charaddr = ((tmp & 0x3FF) << 2) | (pat.supplementdata & 0x3)
                              | ((pat.supplementdata & 0x1C) << 10);
            break;
        case 1:
            pn.flipfunction = 0;
            if (pat.patternwh == 1)
                pn.charaddr = (tmp & 0xFFF) | ((pat.supplementdata & 0x1C) << 10);
            else
                pn.charaddr = ((tmp & 0xFFF) << 2) | (pat.supplementdata & 0x3)
                              | ((pat.supplementdata & 0x10) << 10);
            break;
        }
    } else {
        // 2-word pattern name
        uint16_t tmp1 = vramWord(vram, addr);
        uint16_t tmp2 = vramWord(vram, addr + 2);
        pn.charaddr = tmp2 & 0x7FFF;
        pn.flipfunction = (tmp1 & 0xC000) >> 14;
        if (colornumber == 0)
            pn.paladdr = (tmp1 & 0x7F) << 4;
        else
            pn.paladdr = (tmp1 & 0x70) << 4;
        pn.specialfunction      = (tmp1 & 0x2000) >> 13;
        pn.specialcolorfunction = (tmp1 & 0x1000) >> 12;
    }

    // Mask charaddr to 14 bits in 4Mbit mode.
    if (!vrsize8Mbit)
        pn.charaddr &= 0x3FFF;

    pn.charaddr *= 0x20; // selon Runik (vidsoft.c:319)
    return pn;
}

// ---------------------------------------------------------------------------
// Fetch one tile pixel.  Mirrors Vdp2FetchPixel (vidsoft.c:388-436).
// Returns true if the pixel is opaque (non-transparent).
// `color` receives 0x00RRGGBB on success.
// `dot` receives the raw palette index.
// ---------------------------------------------------------------------------
static bool fetchTilePixel(int tx, int ty,
                           int colornumber, int cellw,
                           uint32_t charaddr, uint32_t paladdr,
                           uint32_t coloroffset,
                           bool transparencyEnable,
                           const uint8_t* vram, const uint8_t* cram,
                           int colorMode,
                           uint32_t& outColor, uint32_t& outDot)
{
    switch (colornumber) {
    case 0: { // 4bpp palette
        uint32_t b = vramByte(vram, (charaddr + ((ty * cellw) + tx) / 2) & 0x7FFFF);
        outDot = b;
        if (!(tx & 0x1)) outDot >>= 4;
        if (!(outDot & 0xF) && transparencyEnable) return false;
        outColor = cramReadColor(coloroffset + (paladdr | (outDot & 0xF)), cram, colorMode);
        return true;
    }
    case 1: { // 8bpp palette
        outDot = vramByte(vram, (charaddr + (ty * cellw) + tx) & 0x7FFFF);
        if (!(outDot & 0xFF) && transparencyEnable) return false;
        outColor = cramReadColor(coloroffset + (paladdr | (outDot & 0xFF)), cram, colorMode);
        return true;
    }
    case 2: { // 16bpp palette (CRAM index)
        outDot = vramWord(vram, (charaddr + ((ty * cellw) + tx) * 2) & 0x7FFFF);
        if (outDot == 0 && transparencyEnable) return false;
        outColor = cramReadColor(coloroffset + outDot, cram, colorMode);
        return true;
    }
    case 3: { // 16bpp RGB (direct color, MSB = opaque)
        outDot = vramWord(vram, (charaddr + ((ty * cellw) + tx) * 2) & 0x7FFFF);
        if (!(outDot & 0x8000) && transparencyEnable) return false;
        // Expand RGB555 -> 0x00RRGGBB (same as COLSAT2YAB16 on LE)
        uint32_t r = (outDot & 0x001F) << 3;
        uint32_t g = (outDot & 0x03E0) << 6;
        uint32_t b = (outDot & 0x7C00) << 9;
        outColor = b | g | r;
        return true;
    }
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Apply flip inside a single 8x8 cell (patternwh==1).
// Mirrors vidsoft.c:624-643.
// ---------------------------------------------------------------------------
static void applyFlip1x1(int& cx, int& cy, int flip)
{
    switch (flip & 0x3) {
    case 1: cx = 7 - cx; break;
    case 2: cy = 7 - cy; break;
    case 3: cx = 7 - cx; cy = 7 - cy; break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Apply flip for 2x2 (16x16) pattern (patternwh==2).
// Mirrors vidsoft.c:646-686.
// ---------------------------------------------------------------------------
static void applyFlip2x2(int& cx, int& cy, int flip)
{
    if (!flip) {
        cy &= 15;
        if (cy & 8) cy += 8;
        if (cx & 8) cy += 8;
        cx &= 7;
        return;
    }
    cy &= 15;
    if (flip & 0x2) {
        if (!(cy & 8)) cy = 8 - 1 - cy + 16;
        else            cy = 16 - 1 - cy;
    } else if (cy & 8) {
        cy += 8;
    }
    if (flip & 0x1) {
        if (!(cx & 8)) cy += 8;
        cx &= 7;
        cx = 7 - cx;
    } else if (cx & 8) {
        cy += 8;
        cx &= 7;
    } else {
        cx &= 7;
    }
}

// ---------------------------------------------------------------------------
// Decode one NBG layer pixel at Saturn coordinates (sx, sy).
//
// Parameters (all from the frozen Vdp2 register struct):
//   layerIdx    -- 0=NBG0, 1=NBG1, 2=NBG2, 3=NBG3
//   enabled     -- layer enabled in BGON
//   colornumber -- 0=4bpp, 1=8bpp, 2=16bpp-pal, 3=16bpp-rgb
//   transparencyEnable
//   scrollX, scrollY -- integer scroll offset
//   coordincx, coordincy -- zoom inverse (1.0 = no zoom)
//   planeAddrs[4], pi, pat -- pre-built geometry
//   coloroffset -- CRAOFA/B field * 256
//   priority    -- raw priority value
//   specialprimode -- SFPRMD bits
//   specialfunction -- from pattern name (filled per cell)
//
// Returns a partially filled Vdp2LayerProbe (name / enabled / supported /
// opaque / priority / color filled; ccEnable/ccRatio left for caller).
// ---------------------------------------------------------------------------

struct NBGConfig {
    int   layerIdx;
    const char* name;
    bool  enabled;
    bool  transparencyEnable;
    int   colornumber;
    int   scrollX, scrollY;
    float coordincx, coordincy;
    uint32_t planeAddrs[4];
    PlaneInfo  pi;
    PatternInfo pat;
    uint32_t coloroffset;
    int   priority;
    int   specialprimode;
    bool  vrsize8Mbit;
};

static Vdp2LayerProbe decodeNBGTile(int sx, int sy,
                                    const NBGConfig& cfg,
                                    const uint8_t* vram,
                                    const uint8_t* cram,
                                    int colorMode)
{
    Vdp2LayerProbe out;
    out.name    = cfg.name;
    out.enabled = cfg.enabled;
    out.priority = cfg.priority;

    if (!cfg.enabled) {
        out.supported = true; // enabled=false is valid; just not drawn
        return out;
    }

    // Reject unsupported color modes now.
    if (cfg.colornumber >= 4) {
        out.supported = false;
        static_cast<void>(snprintf(out.note, sizeof(out.note), "32bpp direct not decoded"));
        return out;
    }

    // Apply scroll and zoom to get virtual coordinates.
    // coordincx is the reciprocal zoom factor used in vidsoft.c:
    //   info.coordincx = (ZMXN.all & 0x7FF00) / 65536.0f
    // which gives 0 when the integer part is 0. We clamp to 1.0 to avoid /0.
    float coix = (cfg.coordincx <= 0.0f) ? 1.0f : cfg.coordincx;
    float coiy = (cfg.coordincy <= 0.0f) ? 1.0f : cfg.coordincy;

    int vx = cfg.scrollX + static_cast<int>(static_cast<float>(sx) / coix);
    int vy = cfg.scrollY + static_cast<int>(static_cast<float>(sy) / coiy);

    // Map dimensions for NBG0-3: mapwh=2, planew x planeh pages.
    const int mapwh = 2;
    const int pagepixelwh      = 64 * 8; // 512
    const int pagepixelwh_bits = 9;
    const int pagepixelwh_mask = 511;

    int planepixelwidth       = cfg.pi.planew * pagepixelwh;
    int planepixelwidth_bits  = 8 + cfg.pi.planew;
    int planepixelwidth_mask  = (1 << planepixelwidth_bits) - 1;

    int planepixelheight       = cfg.pi.planeh * pagepixelwh;
    int planepixelheight_bits  = 8 + cfg.pi.planeh;
    int planepixelheight_mask  = (1 << planepixelheight_bits) - 1;

    int screenwidth  = mapwh * planepixelwidth;
    int screenheight = mapwh * planepixelheight;
    int xmask = screenwidth - 1;
    int ymask = screenheight - 1;

    // Wrap virtual coordinate to screen.
    vx &= xmask;
    vy &= ymask;

    // --- Vdp2MapCalcXY ---
    // Determine plane.
    int planenum = ((vy >> planepixelheight_bits) * mapwh)
                 + (vx >> planepixelwidth_bits);
    if (planenum < 0 || planenum >= 4) {
        out.supported = false;
        static_cast<void>(snprintf(out.note, sizeof(out.note), "planenum out of range"));
        return out;
    }
    int px = vx & planepixelwidth_mask;
    int py = vy & planepixelheight_mask;

    // Page within the plane.
    const int cellwh = 2 + cfg.pat.patternwh; // bits for cell size
    const int pagesize_bits = cfg.pat.pagewh_bits * 2;

    uint32_t pnameAddr = cfg.planeAddrs[planenum];
    pnameAddr += (((  (py >> pagepixelwh_bits) << pagesize_bits) << cfg.pi.planew_bits)
                + ((  (px >> pagepixelwh_bits) << pagesize_bits))
                + (((py & pagepixelwh_mask) >> cellwh) << cfg.pat.pagewh_bits)
                + ((px & pagepixelwh_mask) >> cellwh))
               << (cfg.pat.patterndatasize_bits + 1);

    PatternName pn = decodePatternAddr(pnameAddr, cfg.pat, cfg.colornumber,
                                       cfg.vrsize8Mbit, vram);

    // Apply special priority mode 1 (SFPRMD).
    int effectivePriority = cfg.priority;
    if (cfg.specialprimode == 1) {
        effectivePriority = (effectivePriority & 0xE) | (pn.specialfunction & 1);
    }
    out.priority = effectivePriority;

    // --- Figure out pixel offset within the tile ---
    int cx = px;
    int cy = py;

    if (cfg.pat.patternwh == 1) {
        cx &= 7;
        cy &= 7;
        applyFlip1x1(cx, cy, pn.flipfunction);
    } else {
        applyFlip2x2(cx, cy, pn.flipfunction);
    }

    // --- Fetch the pixel ---
    uint32_t color = 0, dot = 0;
    bool opaque = fetchTilePixel(cx, cy,
                                  cfg.colornumber, cfg.pat.cellw,
                                  pn.charaddr, pn.paladdr,
                                  cfg.coloroffset,
                                  cfg.transparencyEnable,
                                  vram, cram, colorMode,
                                  color, dot);
    out.opaque    = opaque;
    out.color     = color & 0x00FFFFFFu; // strip MSB (special-cc bit)
    out.supported = true;
    return out;
}

// ---------------------------------------------------------------------------
// Decode back screen color from BKTAU/BKTAL -> VRAM -> RGB.
// Mirrors vidsoft.c:1450-1494 (single-color path only; per-line is noted).
// ---------------------------------------------------------------------------
static uint32_t decodeBackScreen(const Vdp2& regs,
                                  const uint8_t* vram,
                                  const uint8_t* cram,
                                  int colorMode)
{
    if ((regs.TVMD & 0x8000) == 0 && (regs.TVMD & 0x100) == 0) {
        return 0; // display disabled -> black
    }

    bool vrsize8Mbit = (regs.VRSIZE & 0x8000) != 0;
    uint32_t scrAddr;
    if (vrsize8Mbit)
        scrAddr = (((uint32_t)(regs.BKTAU & 0x7) << 16) | regs.BKTAL) * 2;
    else
        scrAddr = (((uint32_t)(regs.BKTAU & 0x3) << 16) | regs.BKTAL) * 2;

    // Read the first word regardless of per-line flag.
    // (Per-line back screen would need the scanline -- ignored here; we read line 0.)
    uint16_t dot = vramWord(vram, scrAddr);

    // dot is RGB555; expand to 0x00RRGGBB.
    uint32_t r = (dot & 0x001F) << 3;
    uint32_t g = (dot & 0x03E0) << 6;
    uint32_t b = (dot & 0x7C00) << 9;
    return b | g | r;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
Vdp2PixelReport probeVdp2Pixel(int x, int y,
                               const Vdp2&    regs,
                               const uint8_t* vram,
                               const uint8_t* cram)
{
    Vdp2PixelReport rpt;
    rpt.x = x;
    rpt.y = y;

    const bool vrsize8Mbit = (regs.VRSIZE & 0x8000) != 0;
    const int  colorMode   = (regs.RAMCTL >> 12) & 0x3;

    // -----------------------------------------------------------------------
    // Color-calc state (Vdp2ColorCalcState.h decodeVdp2ColorCalc).
    // We use the vdp2cc::LayerIndex mapping:
    //   kNBG3=0, kNBG2=1, kNBG1=2, kNBG0=3, kRBG0=4, kSprite=5
    // Our report layers[]: [0]=NBG0,[1]=NBG1,[2]=NBG2,[3]=NBG3,[4]=RBG0,[5]=Sprite
    // so we map: layers[0]=NBG0 -> kNBG0=3, layers[1]=NBG1 -> kNBG1=2, etc.
    // -----------------------------------------------------------------------
    const std::array<uint16_t, 4> ccrs = {
        regs.CCRSA, regs.CCRSB, regs.CCRSC, regs.CCRSD
    };
    vdp2cc::State ccState = vdp2cc::decodeVdp2ColorCalc(
        regs.CCCTL, regs.CCRNA, regs.CCRNB, regs.CCRR, ccrs, regs.CCRLB);

    // Helper: fill ccEnable/ccRatio into a Vdp2LayerProbe from vdp2cc state.
    // vdp2cc LayerIndex: kNBG0=3, kNBG1=2, kNBG2=1, kNBG3=0, kRBG0=4, kSprite=5
    auto fillCC = [&](Vdp2LayerProbe& probe, int vdp2ccIdx) {
        const auto& lc = ccState.perLayer[vdp2ccIdx];
        probe.ccEnable = lc.ccEnable;
        // ratio stored by decodeLayer: (5bit << 1) + 1 => range 1..63.
        // Saturate to 0..63 for the report.
        probe.ccRatio = lc.ratio & 0x3F;
    };

    // -----------------------------------------------------------------------
    // NBG0
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[0];
        out.name = "NBG0";

        bool en = (regs.BGON & 0x1) != 0;
        bool isBitmap = (regs.CHCTLA & 0x2) != 0;

        out.enabled = en;
        fillCC(out, vdp2cc::kNBG0);

        if (!en) {
            out.supported = true;
        } else if (isBitmap) {
            out.supported = false;
            static_cast<void>(snprintf(out.note, sizeof(out.note), "bitmap mode not decoded"));
        } else {
            int colornumber = (regs.CHCTLA & 0x70) >> 4;
            if (colornumber >= 4) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note), "32bpp not decoded"));
            } else {
                NBGConfig cfg;
                cfg.layerIdx  = 0;
                cfg.name      = "NBG0";
                cfg.enabled   = true;
                cfg.transparencyEnable = !(regs.BGON & 0x100);
                cfg.colornumber = colornumber;
                cfg.scrollX = regs.SCXIN0 & 0x7FF;
                cfg.scrollY = regs.SCYIN0 & 0x7FF;
                // coordincx: (ZMXN0.all & 0x7FF00) / 65536.0f (vidsoft.c:1629)
                // zero integer part -> treat as 1.0 (no zoom)
                float cx0 = (float)(regs.ZMXN0.all & 0x7FF00) / 65536.0f;
                float cy0 = (float)(regs.ZMYN0.all & 0x7FF00) / 65536.0f;
                cfg.coordincx = (cx0 <= 0.0f) ? 1.0f : cx0;
                cfg.coordincy = (cy0 <= 0.0f) ? 1.0f : cy0;
                readPlaneSize(cfg.pi, regs.PLSZ);
                readPatternData(cfg.pat, regs.PNCN0, regs.CHCTLA & 0x1);
                // MPOFN offset: bits 2:0 * 64 (vidshared.c:55)
                uint32_t offset = (regs.MPOFN & 0x7) << 6;
                buildNBGPlaneTable(cfg.planeAddrs, offset,
                                   regs.MPABN0, regs.MPCDN0,
                                   cfg.pi, cfg.pat, vrsize8Mbit);
                cfg.coloroffset   = (regs.CRAOFA & 0x7) << 8;
                cfg.priority      = regs.PRINA & 0x7;
                cfg.specialprimode = regs.SFPRMD & 0x3;
                cfg.vrsize8Mbit   = vrsize8Mbit;

                out = decodeNBGTile(x, y, cfg, vram, cram, colorMode);
                fillCC(out, vdp2cc::kNBG0);
                out.name = "NBG0";
            }
        }
    }

    // -----------------------------------------------------------------------
    // NBG1
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[1];
        out.name = "NBG1";

        bool en = (regs.BGON & 0x2) != 0;
        bool isBitmap = (regs.CHCTLA & 0x200) != 0;

        out.enabled = en;
        fillCC(out, vdp2cc::kNBG1);

        if (!en) {
            out.supported = true;
        } else if (isBitmap) {
            out.supported = false;
            static_cast<void>(snprintf(out.note, sizeof(out.note), "bitmap mode not decoded"));
        } else {
            int colornumber = (regs.CHCTLA & 0x3000) >> 12;
            if (colornumber >= 4) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note), "32bpp not decoded"));
            } else {
                // Skip draw if NBG0 is in 32bpp / 16M (colornumber>=4 or ==4) mode.
                // Mirrors vidsoft.c:1767-1769.
                int nbg0cn = (regs.CHCTLA & 0x70) >> 4;
                if ((regs.BGON & 0x1) && nbg0cn == 4) {
                    out.supported = false;
                    static_cast<void>(snprintf(out.note, sizeof(out.note),
                                    "suppressed: NBG0 in 16M mode"));
                } else {
                    NBGConfig cfg;
                    cfg.layerIdx  = 1;
                    cfg.name      = "NBG1";
                    cfg.enabled   = true;
                    cfg.transparencyEnable = !(regs.BGON & 0x200);
                    cfg.colornumber = colornumber;
                    cfg.scrollX = regs.SCXIN1 & 0x7FF;
                    cfg.scrollY = regs.SCYIN1 & 0x7FF;
                    float cx1 = (float)(regs.ZMXN1.all & 0x7FF00) / 65536.0f;
                    float cy1 = (float)(regs.ZMYN1.all & 0x7FF00) / 65536.0f;
                    cfg.coordincx = (cx1 <= 0.0f) ? 1.0f : cx1;
                    cfg.coordincy = (cy1 <= 0.0f) ? 1.0f : cy1;
                    readPlaneSize(cfg.pi, regs.PLSZ >> 2);
                    readPatternData(cfg.pat, regs.PNCN1, (regs.CHCTLA & 0x100) ? 1 : 0);
                    // MPOFN bits 6:4 -> offset = (MPOFN & 0x70) << 2 (vidshared.c:81)
                    uint32_t offset = (regs.MPOFN & 0x70) << 2;
                    buildNBGPlaneTable(cfg.planeAddrs, offset,
                                       regs.MPABN1, regs.MPCDN1,
                                       cfg.pi, cfg.pat, vrsize8Mbit);
                    cfg.coloroffset   = (regs.CRAOFA & 0x70) << 4;
                    cfg.priority      = (regs.PRINA >> 8) & 0x7;
                    cfg.specialprimode = (regs.SFPRMD >> 2) & 0x3;
                    cfg.vrsize8Mbit   = vrsize8Mbit;

                    out = decodeNBGTile(x, y, cfg, vram, cram, colorMode);
                    fillCC(out, vdp2cc::kNBG1);
                    out.name = "NBG1";
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // NBG2
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[2];
        out.name = "NBG2";

        bool en = (regs.BGON & 0x4) != 0;
        out.enabled = en;
        fillCC(out, vdp2cc::kNBG2);

        if (!en) {
            out.supported = true;
        } else {
            // colornumber: (CHCTLB & 0x2) >> 1 (vidsoft.c:1823)
            int colornumber = (regs.CHCTLB & 0x2) >> 1;

            // Suppression when NBG0 has >= 2048 colors (vidsoft.c:1853-1855).
            int nbg0cn = (regs.CHCTLA & 0x70) >> 4;
            if ((regs.BGON & 0x1) && nbg0cn >= 2) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note),
                                "suppressed: NBG0 in high-color mode"));
            } else if (colornumber >= 4) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note), "32bpp not decoded"));
            } else {
                NBGConfig cfg;
                cfg.layerIdx  = 2;
                cfg.name      = "NBG2";
                cfg.enabled   = true;
                cfg.transparencyEnable = !(regs.BGON & 0x400);
                cfg.colornumber = colornumber;
                cfg.scrollX = regs.SCXN2 & 0x7FF;
                cfg.scrollY = regs.SCYN2 & 0x7FF;
                cfg.coordincx = cfg.coordincy = 1.0f;
                readPlaneSize(cfg.pi, regs.PLSZ >> 4);
                readPatternData(cfg.pat, regs.PNCN2, (regs.CHCTLB & 0x1) ? 1 : 0);
                // MPOFN bits 10:8 -> offset = (MPOFN & 0x700) >> 2 (vidshared.c:107)
                uint32_t offset = (regs.MPOFN & 0x700) >> 2;
                buildNBGPlaneTable(cfg.planeAddrs, offset,
                                   regs.MPABN2, regs.MPCDN2,
                                   cfg.pi, cfg.pat, vrsize8Mbit);
                cfg.coloroffset   = regs.CRAOFA & 0x700;
                cfg.priority      = regs.PRINB & 0x7;
                cfg.specialprimode = (regs.SFPRMD >> 4) & 0x3;
                cfg.vrsize8Mbit   = vrsize8Mbit;

                out = decodeNBGTile(x, y, cfg, vram, cram, colorMode);
                fillCC(out, vdp2cc::kNBG2);
                out.name = "NBG2";
            }
        }
    }

    // -----------------------------------------------------------------------
    // NBG3
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[3];
        out.name = "NBG3";

        bool en = (regs.BGON & 0x8) != 0;
        out.enabled = en;
        fillCC(out, vdp2cc::kNBG3);

        if (!en) {
            out.supported = true;
        } else {
            // colornumber: (CHCTLB & 0x20) >> 5 (vidsoft.c:1895)
            int colornumber = (regs.CHCTLB & 0x20) >> 5;

            int nbg0cn = (regs.CHCTLA & 0x70) >> 4;
            int nbg1cn = (regs.CHCTLA & 0x3000) >> 12;
            // Suppression (vidsoft.c:1926-1928):
            //   NBG0 16M || NBG1 >= 2048 colors
            if ((regs.BGON & 0x1) && nbg0cn == 4) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note),
                                "suppressed: NBG0 in 16M mode"));
            } else if ((regs.BGON & 0x2) && nbg1cn >= 2) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note),
                                "suppressed: NBG1 in high-color mode"));
            } else if (colornumber >= 4) {
                out.supported = false;
                static_cast<void>(snprintf(out.note, sizeof(out.note), "32bpp not decoded"));
            } else {
                NBGConfig cfg;
                cfg.layerIdx  = 3;
                cfg.name      = "NBG3";
                cfg.enabled   = true;
                cfg.transparencyEnable = !(regs.BGON & 0x800);
                cfg.colornumber = colornumber;
                cfg.scrollX = regs.SCXN3 & 0x7FF;
                cfg.scrollY = regs.SCYN3 & 0x7FF;
                cfg.coordincx = cfg.coordincy = 1.0f;
                readPlaneSize(cfg.pi, regs.PLSZ >> 6);
                readPatternData(cfg.pat, regs.PNCN3, (regs.CHCTLB & 0x10) ? 1 : 0);
                // MPOFN bits 14:12 -> offset = (MPOFN & 0x7000) >> 6 (vidshared.c:133)
                uint32_t offset = (regs.MPOFN & 0x7000) >> 6;
                buildNBGPlaneTable(cfg.planeAddrs, offset,
                                   regs.MPABN3, regs.MPCDN3,
                                   cfg.pi, cfg.pat, vrsize8Mbit);
                cfg.coloroffset   = (regs.CRAOFA & 0x7000) >> 4;
                cfg.priority      = (regs.PRINB >> 8) & 0x7;
                cfg.specialprimode = (regs.SFPRMD >> 6) & 0x3;
                cfg.vrsize8Mbit   = vrsize8Mbit;

                out = decodeNBGTile(x, y, cfg, vram, cram, colorMode);
                fillCC(out, vdp2cc::kNBG3);
                out.name = "NBG3";
            }
        }
    }

    // -----------------------------------------------------------------------
    // RBG0: fill enabled / priority / cc state; pixel decode not supported.
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[4];
        out.name       = "RBG0";
        out.enabled    = (regs.BGON & 0x10) != 0;
        out.priority   = regs.PRIR & 0x7;
        out.isRotation = true;
        out.supported  = false;
        fillCC(out, vdp2cc::kRBG0);
        static_cast<void>(snprintf(out.note, sizeof(out.note),
                        "rotation pixel not decoded"));
    }

    // -----------------------------------------------------------------------
    // Sprite: always unsupported (VDP1 FB is GPU-side).
    // -----------------------------------------------------------------------
    {
        Vdp2LayerProbe& out = rpt.layers[5];
        out.name      = "Sprite";
        out.enabled   = true; // sprites may be active
        out.supported = false;
        fillCC(out, vdp2cc::kSprite);
        static_cast<void>(snprintf(out.note, sizeof(out.note),
                        "VDP1 FB GPU-side, not available"));
    }

    // -----------------------------------------------------------------------
    // Back screen color.
    // -----------------------------------------------------------------------
    rpt.backColor = decodeBackScreen(regs, vram, cram, colorMode);

    // -----------------------------------------------------------------------
    // topIdx / secondIdx selection.
    // Tiebreak matches Vdp2Compositor.cpp (vulkan/Vdp2Compositor.cpp:173-183):
    //   for priority = 7..1 (descending):
    //     for which = kSprite(5)..kNBG3(0) (descending):
    //       first opaque supported -> top; second -> second
    // Sprite (layers[5]) is excluded (always unsupported).
    // We map our report layers[0..4] to compositor indices [3,2,1,0,4].
    //   layers[0]=NBG0 -> compositor which=3
    //   layers[1]=NBG1 -> compositor which=2
    //   layers[2]=NBG2 -> compositor which=1
    //   layers[3]=NBG3 -> compositor which=0
    //   layers[4]=RBG0 -> compositor which=4
    // Iterate in compositor's inner loop order (which=4..0) for tiebreak.
    // -----------------------------------------------------------------------
    {
        // Map compositor `which` to our rpt.layers[] index.
        // compositor which: 0=NBG3, 1=NBG2, 2=NBG1, 3=NBG0, 4=RBG0
        // our layers[]:     0=NBG0, 1=NBG1, 2=NBG2, 3=NBG3, 4=RBG0
        static const int kCompositorToReport[5] = { 3, 2, 1, 0, 4 };

        int found = 0;
        for (int prio = 7; prio > 0 && found < 2; --prio) {
            for (int which = 4; which >= 0 && found < 2; --which) {
                int ri = kCompositorToReport[which];
                const Vdp2LayerProbe& lp = rpt.layers[ri];
                if (!lp.supported || !lp.opaque) continue;
                if (lp.priority != prio)         continue;
                if (found == 0) rpt.topIdx    = ri;
                else             rpt.secondIdx = ri;
                ++found;
            }
        }
    }

    return rpt;
}
