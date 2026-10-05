/*
 * The title screen's logo, over its 3D scene (randomizer_title, see randomizer_title.h):
 * the game's own logo is painted into the title picture, which the scene covers.
 *
 * The logo is in the ROM block at _6CA730, which the game never reads, put there by the
 * build (tools/randomizer/gen_title_logo.py): artwork from assets/randomizer/title_logo.png
 * if there's one, otherwise the logo cut out of the title picture. A 256-colour texture
 * (CI8 with an RGBA16 palette, each colour transparent or not), Yay0-compressed after a
 * header that says where it goes. It's read as the title screen starts and drawn every
 * frame, in strips of rows: the texture memory holds the palette and 2 KB of pixels.
 *
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_title.h"

#ifdef RANDOMIZER

#include "src/3FB0.h"
#include "src/memory.h"

#define LOGO_MAGIC 0x4C4F474F // "LOGO"
#define LOGO_PALETTE_SIZE (256 * sizeof(u16))
#define LOGO_TMEM 2048 // bytes of pixels beside a 256-colour palette

typedef struct RandomizerLogoHeader {
    /* 0x00 */ u32 magic;
    /* 0x04 */ u16 x;
    /* 0x06 */ u16 y;
    /* 0x08 */ u16 width; // a multiple of 8
    /* 0x0A */ u16 height;
    /* 0x0C */ u32 size; // of the compressed data after the header
} RandomizerLogoHeader;   // size = 0x10

static RandomizerLogoHeader sLogo;
static u8* sLogoData; // the palette, then the pixels; NULL without a logo

// Reads the logo from the ROM, as the title screen starts
void Randomizer_LogoLoad(void) {
    static u64 sHeader[sizeof(RandomizerLogoHeader) / sizeof(u64)]; // DMA needs 8-byte alignment
    u32 rom = (u32)_6CA730_ROM_START;
    u32 size;
    u8* packed;

    sLogoData = NULL;
    osInvalDCache(sHeader, sizeof(sHeader));
    func_80003B30((u32)sHeader, rom, rom + sizeof(sHeader), 0);
    sLogo = *(RandomizerLogoHeader*)sHeader;
    if (sLogo.magic != LOGO_MAGIC) {
        return;
    }

    // The compressed data in a block of its own after the logo's, freed once it's unpacked
    sLogoData = main_pool_alloc(LOGO_PALETTE_SIZE + (sLogo.width * sLogo.height), 0);
    size = ALIGN16(sLogo.size);
    packed = main_pool_alloc(size, 0);
    osInvalDCache(packed, size);
    func_80003B30((u32)packed, rom + sizeof(sHeader), rom + sizeof(sHeader) + size, 0);
    Yay0_Decompress(packed, sLogoData);
    main_pool_free(packed, 0);
}

// Draws the logo, faded to alpha (0 to 255)
void Randomizer_LogoDraw(s32 alpha) {
    u8* pixels;
    s32 rows;
    s32 row;

    if (sLogoData == NULL) {
        return;
    }
    pixels = sLogoData + LOGO_PALETTE_SIZE;
    rows = LOGO_TMEM / sLogo.width;

    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_POINT);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 255, 255, 255, alpha);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gDisplayListHead++, sLogoData);

    for (row = 0; row < sLogo.height; row += rows) {
        s32 last = ((row + rows) < sLogo.height) ? (row + rows - 1) : (sLogo.height - 1);

        gDPLoadTextureTile(gDisplayListHead++, pixels, G_IM_FMT_CI, G_IM_SIZ_8b, sLogo.width, sLogo.height, 0, row,
                           sLogo.width - 1, last, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gDisplayListHead++, sLogo.x << 2, (sLogo.y + row) << 2, (sLogo.x + sLogo.width) << 2,
                            (sLogo.y + last + 1) << 2, G_TX_RENDERTILE, 0, row << 5, 1 << 10, 1 << 10);
    }

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
}

#endif
