#pragma once
// OoT3D asset converter: skinned 3DS characters (NPCs, enemies, bosses) as "3DS skin" packages for Mergeship's engine.
//
// A package replaces a vanilla skeleton's look: the 3DS mesh with its bone weights, material state lists, the 3DS
// skeleton, and every vanilla animation of the skeleton paired with the 3DS animation of the same motion (3DS
// animations are authoritative). The engine (soh/MergeShip/MergeShip3dsSkin.cpp) keeps the N64 skeleton running for
// gameplay, suppresses its limb meshes, and draws the package posed by the paired 3DS animation.
//
// Blob layout ("O3SK", version 2, little-endian), in the Blob resource at oot3d/skin/<vanilla skeleton path>:
//   u32 magic, version, boneCount, vertexCount, groupCount, animCount, limbCount
//   bones:    s16 parent, u16 0, f32 scale[3], rotation[3] (radians), translation[3], f32 inverseBind[12] (3x4 rows)
//   vertices: f32 position[3], f32 normal[3] (bind pose, model space), u8 bone[4], u8 weight[4] (sum 255)
//   groups:   str statePath, u8 lit, u8 translucent, s16 limb, str list (v2: drawn only while vanilla limb `limb`
//             draws display list `list`; limb -1: always), u16 batchCount, then per batch
//             u8 vertexCount, u8 triCount, u16 0, {u32 vertex, s16 s, s16 t, u8 rgba[4]} x vertexCount,
//             {u8 a, b, c} x triCount, zero padding to 4 bytes
//   anims:    str n64Path (as SkelAnime keeps it, without "__OTR__"), s16 n64Frames, u16 0, f32 frames3ds,
//             per bone: u16 channelMask (bit k: channel k of tx ty tz rx ry rz sx sy sz), per set channel:
//             u8 type (1 linear, 2 hermite), u8 0, u16 keyCount, f32 timeEnd, keys {f32 t, v} or {f32 t, v, in, out}
//   limbToBone: s16 x limbCount
//   str = u32 length + bytes

#include "Oot3dExtract.h"
#include "Oot3dZip.h"

#include <set>
#include <string>

namespace Oot3d {

class RomFS;
class TextureStore;

class RomIndex;

struct SkinResult {
    uint32_t skins = 0, animations = 0, unpaired = 0, skipped = 0, warnings = 0;
};

// Display lists the actor sources name (gen_anim_table.py): limb overrides and lists actors draw themselves.
const std::set<std::string>& ActorReferencedLists();

SkinResult ConvertSkins(const RomFS& rom, const RomIndex& index, const ZipReader& oot, ZipWriter& zip,
                        TextureStore& textures, const ExtractOptions& opt);

} // namespace Oot3d
