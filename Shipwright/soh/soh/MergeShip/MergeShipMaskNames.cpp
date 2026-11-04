// MergeShip2 -- " (MM)" after the names of Majora's Mask's masks on the Masks page.
//
// Every MM mask's name carries " (MM)" (the Deku Mask and Fierce Deity's Mask excepted), so they are told apart
// from Hyrule's Keaton, Bunny, Goron, Zora and Truth masks. The Masks page shows MM's own name textures (mm.o2r's
// item_name_static, IA4 128x16); the suffix is added at run time, as the Deku Beans name is put together: the name
// is copied, and "(MM)" is written after it -- the "M" cut from MM's own "Mask of Truth", the parentheses drawn in
// the same style (a bright core, a soft edge and a dim outline). A name that would run past the panel is narrowed.

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/Texture.h>
#include <algorithm>
#include <cstring>
#include <memory>

extern "C" {
#include "z64.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "mm/archives/item_name_static/item_name_static.h"
}

namespace {

constexpr s32 kW = 128;
constexpr s32 kH = 16;
constexpr size_t kSize = kW * kH / 2;

struct Composed {
    u8 tex[kSize];
    s32 state; // 0 not tried, 1 made, -1 failed
};

Composed sNames[ITEM_MM_LAST + 1 - ITEM_MM_MASK_FIERCE_DEITY];

std::shared_ptr<Fast::Texture> LoadName(const char* path) {
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true);
    if ((res == nullptr) || (res->GetInitData() == nullptr) ||
        (res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::Texture))) {
        return nullptr;
    }
    auto tex = std::static_pointer_cast<Fast::Texture>(res);
    if ((tex->Type != Fast::TextureType::GrayscaleAlpha4bpp) || (tex->Width != kW) || (tex->Height != kH) ||
        (tex->ImageData == nullptr) || (tex->ImageDataSize < kSize)) {
        return nullptr;
    }
    return tex;
}

u8 Get(const u8* tex, s32 x, s32 y) {
    u8 byte = tex[(y * kW + x) / 2];
    return (x & 1) ? (byte & 0xF) : (byte >> 4);
}

void Set(u8* tex, s32 x, s32 y, u8 v) {
    u8* byte = &tex[(y * kW + x) / 2];
    *byte = (x & 1) ? ((*byte & 0xF0) | v) : ((*byte & 0x0F) | (v << 4));
}

const char* NameTexture(u8 item) {
    switch (item) {
        case ITEM_MM_MASK_GORON:
            return mm_gItemNameGoronMaskENGTex;
        case ITEM_MM_MASK_ZORA:
            return mm_gItemNameZoraMaskENGTex;
        case ITEM_MM_MASK_TRUTH:
            return mm_gItemNameMaskOfTruthENGTex;
        case ITEM_MM_MASK_KAFEIS_MASK:
            return mm_gItemNameKafeisMaskENGTex;
        case ITEM_MM_MASK_ALL_NIGHT:
            return mm_gItemNameAllNightMaskENGTex;
        case ITEM_MM_MASK_BUNNY:
            return mm_gItemNameBunnyHoodENGTex;
        case ITEM_MM_MASK_KEATON:
            return mm_gItemNameKeatonMaskENGTex;
        case ITEM_MM_MASK_GARO:
            return mm_gItemNameGarosMaskENGTex;
        case ITEM_MM_MASK_ROMANI:
            return mm_gItemNameRomanisMaskENGTex;
        case ITEM_MM_MASK_CIRCUS_LEADER:
            return mm_gItemNameCircusLeadersMaskENGTex;
        case ITEM_MM_MASK_POSTMAN:
            return mm_gItemNamePostmansHatENGTex;
        case ITEM_MM_MASK_COUPLE:
            return mm_gItemNameCouplesMaskENGTex;
        case ITEM_MM_MASK_GREAT_FAIRY:
            return mm_gItemNameGreatFairysMaskENGTex;
        case ITEM_MM_MASK_GIBDO:
            return mm_gItemNameGibdoMaskENGTex;
        case ITEM_MM_MASK_DON_GERO:
            return mm_gItemNameDonGerosMaskENGTex;
        case ITEM_MM_MASK_KAMARO:
            return mm_gItemNameKamarosMaskENGTex;
        case ITEM_MM_MASK_CAPTAIN:
            return mm_gItemNameCaptainsHatENGTex;
        case ITEM_MM_MASK_STONE:
            return mm_gItemNameStoneMaskENGTex;
        case ITEM_MM_MASK_BREMEN:
            return mm_gItemNameBremenMaskENGTex;
        case ITEM_MM_MASK_BLAST:
            return mm_gItemNameBlastMaskENGTex;
        case ITEM_MM_MASK_SCENTS:
            return mm_gItemNameMaskOfScentsENGTex;
        case ITEM_MM_MASK_GIANT:
            return mm_gItemNameGiantsMaskENGTex;
        default:
            return nullptr; // the Deku Mask and Fierce Deity's Mask keep their names
    }
}

// A glyph drawn from a core bitmap: core 15 with a dim outline (3) around it, as the item name font is drawn.
struct Glyph {
    s32 w;
    u8 px[kH][8];
};

Glyph MakeGlyph(const char* const rows[kH], s32 w) {
    Glyph g = {};
    g.w = w;
    for (s32 y = 0; y < kH; y++) {
        for (s32 x = 0; x < w; x++) {
            if (rows[y][x] == '#') {
                g.px[y][x] = 15;
            }
        }
    }
    for (s32 y = 0; y < kH; y++) {
        for (s32 x = 0; x < w; x++) {
            if (g.px[y][x] != 0) {
                continue;
            }
            bool nearCore = false;
            for (s32 dy = -1; dy <= 1; dy++) {
                for (s32 dx = -1; dx <= 1; dx++) {
                    s32 yy = y + dy;
                    s32 xx = x + dx;
                    if ((yy >= 0) && (yy < kH) && (xx >= 0) && (xx < w) && (rows[yy][xx] == '#')) {
                        nearCore = true;
                    }
                }
            }
            if (nearCore) {
                g.px[y][x] = 3;
            }
        }
    }
    return g;
}

// Rows 1..13 hold the letters of MM's names (caps from row 2, baseline row 11); the parentheses reach a little
// above and below, as they do in MM's message font.
const char* const kOpenParen[kH] = {
    "      ", "   #  ", "  ##  ", "  #   ", " ##   ", " ##   ", " ##   ", " ##   ",
    " ##   ", " ##   ", " ##   ", "  #   ", "  ##  ", "   #  ", "      ", "      ",
};
const char* const kCloseParen[kH] = {
    "      ", "  #   ", "  ##  ", "   #  ", "   ## ", "   ## ", "   ## ", "   ## ",
    "   ## ", "   ## ", "   ## ", "   #  ", "  ##  ", "  #   ", "      ", "      ",
};

void Compose(u8 item, Composed* out) {
    const char* path = NameTexture(item);
    auto name = (path != nullptr) ? LoadName(path) : nullptr;
    auto truth = LoadName(mm_gItemNameMaskOfTruthENGTex);
    u8 canvas[kH][256] = {};
    s32 first = kW;
    s32 last = -1;
    s32 x;

    out->state = -1;
    if ((name == nullptr) || (truth == nullptr)) {
        return;
    }

    for (x = 0; x < kW; x++) {
        for (s32 y = 0; y < kH; y++) {
            if (Get(name->ImageData, x, y) != 0) {
                first = std::min(first, x);
                last = std::max(last, x);
            }
        }
    }
    if (last < first) {
        return;
    }

    // The name.
    s32 width = 0;
    for (x = first; x <= last; x++, width++) {
        for (s32 y = 0; y < kH; y++) {
            canvas[y][width] = Get(name->ImageData, x, y);
        }
    }
    // The gap between words, then "(MM)".
    width += 2;
    Glyph open = MakeGlyph(kOpenParen, 6);
    Glyph close = MakeGlyph(kCloseParen, 6);
    auto put = [&](const Glyph& g) {
        for (s32 gx = 0; gx < g.w; gx++) {
            for (s32 y = 0; y < kH; y++) {
                canvas[y][width + gx] = std::max(canvas[y][width + gx], g.px[y][gx]);
            }
        }
        width += g.w;
    };
    auto putM = [&]() {
        // "M" is columns 24..35 of MM's "Mask of Truth" (its outline included).
        for (s32 gx = 0; gx < 12; gx++) {
            for (s32 y = 0; y < kH; y++) {
                canvas[y][width + gx] = std::max(canvas[y][width + gx], Get(truth->ImageData, 24 + gx, y));
            }
        }
        width += 11; // outlines overlap by a column, as the font's letters do
    };
    width -= 1;
    put(open);
    width -= 2;
    putM();
    putM();
    width -= 1;
    put(close);

    // Centred on the panel as the names are; narrowed (nearest column) when it would not fit.
    memset(out->tex, 0, kSize);
    if (width <= kW) {
        s32 start = (kW - width) / 2;
        for (x = 0; x < width; x++) {
            for (s32 y = 0; y < kH; y++) {
                Set(out->tex, start + x, y, canvas[y][x]);
            }
        }
    } else {
        for (x = 0; x < kW; x++) {
            s32 src = (x * width) / kW;
            for (s32 y = 0; y < kH; y++) {
                Set(out->tex, x, y, canvas[y][src]);
            }
        }
    }
    out->state = 1;
}

} // namespace

// The Masks page's name panel: the item's name with " (MM)" after it, or `nameSegment` unchanged.
extern "C" void* MergeShip_MaskNames_NameTexture(PlayState* play, void* nameSegment) {
    u16 item = play->pauseCtx.namedItem;

    // The user's rule (2026-09-23): masks carry no " (MM)"; the page shows MM's own names.
    if (true) {
        return nameSegment;
    }

    if ((play->pauseCtx.pageIndex != MERGESHIP_PAUSE_MASK) || (item < ITEM_MM_MASK_FIERCE_DEITY) ||
        (item > ITEM_MM_LAST) || (NameTexture((u8)item) == nullptr)) {
        return nameSegment;
    }
    Composed* c = &sNames[item - ITEM_MM_MASK_FIERCE_DEITY];
    if (c->state == 0) {
        Compose((u8)item, c);
    }
    return (c->state > 0) ? (void*)c->tex : nameSegment;
}
