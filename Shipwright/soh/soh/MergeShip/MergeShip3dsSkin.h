#pragma once


// MergeShip2 -- Ocarina of Time 3D characters (MergeShip3dsSkin.cpp).
//
// tools/oot3d converts each 3DS character into a "3DS skin" package (Blob resource oot3d/skin/<vanilla skeleton>,
// under alt/ so SoH's Alternate Assets toggle switches it): the 3DS mesh with bone weights, its materials, the 3DS
// skeleton, and every vanilla animation of the skeleton paired with the 3DS animation of the same motion. While a
// skeleton has a package, its N64 skeleton keeps running for gameplay (limb matrices, OverrideLimbDraw/PostLimbDraw
// callbacks, held items) but draws none of its limb meshes; the 3DS mesh is drawn instead, posed by the paired 3DS
// animation at the N64 animation's frame (the 3DS animations are authoritative). Gated by
// CVAR_ENHANCEMENT("MergeShip.Oot3dCharacters"), default on.

#include <libultraship/libultra/types.h>

// The OoT3D port's engine hooks (skins, drops, tree sprites, room scrolls): 0 compiles every call site out,
// leaving plain MergeShip.
#ifndef MERGESHIP_OOT3D
#define MERGESHIP_OOT3D 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct SkelAnime;

// SkelAnime_Init/InitFlex: `skeletonPath` is the resource path the actor passed ("__OTR__objects/...").
void MergeShip3dsSkin_OnInit(struct SkelAnime* skelAnime, const char* skeletonPath);
void MergeShip3dsSkin_OnFree(struct SkelAnime* skelAnime);

// At the start of a skeleton draw (before its matrix push): 1 when the skeleton has a package, which then draws at
// the end; the caller raises gMergeShip3dsSuppressLimbs for its traversal so no limb display list is drawn, and each
// limb reports the list it would have drawn (after OverrideLimbDraw) so the package shows the matching variants (a
// head, a held prop).
// `gfx`: the Gfx** the traversal writes opaque commands to (NULL: POLY_OPA), so lists the actor draws itself from
// limb callbacks can be found and, when the package has them, replaced.
s32 MergeShip3dsSkin_Begin(struct PlayState* play, void** skeleton, void* jointTable, void* gfx);
// `limb` is 0-based (the skeleton's limb order); `dList` may be NULL (nothing drawn).
void MergeShip3dsSkin_RecordLimb(s32 limb, void* dList);
// After the traversal (the matrix stack back at the actor's matrix): draws the package. `gfx` is the Gfx** the
// opaque parts go into (NULL: POLY_OPA); translucent parts go to POLY_XLU.
void MergeShip3dsSkin_End(struct PlayState* play, void** skeleton, void* jointTable, void* gfx);

// gSPSegment's wrappers report every segment set (eyes and mouths are textures actors put in segments 8-13): a
// package's face materials show the 3DS texture paired with the N64 one last set.
void MergeShip3dsSkin_NoteSegment(int segment, uintptr_t target);

// Whether a limb's own list is skipped in the skeleton being traversed (its 3DS package draws instead; effect limbs the
// package keeps still draw). `limb` is 0-based.
s32 MergeShip3dsSkin_SuppressLimb(s32 limb);

// Around a skeleton's post-limb callback: when a limb-skinned body (Link) draws, the callback's matrix becomes the
// N64 limb frame (pushed; returns 1, the caller pops), so game code placing things in N64 limb space still fits.
s32 MergeShip3dsSkin_PushN64Frame(s32 limb);

// A skeleton resource was loaded (path without "__OTR__"): an OoT3D body skinned onto its limbs ("zzskin_<name>"
// beside it: Link) is registered for the skeleton's limb table.
void MergeShip3dsSkin_OnSkeletonLoaded(const char* path, void** limbs);

// Once per frame from Room_Draw: moves the tile origins of OoT3D rooms' scrolling materials (water, lava).
void MergeShip3dsScroll_Update(u32 gameplayFrames);

// The tunic Player is drawing (0 Kokiri, 1 Goron, 2 Zora; -1 outside Player's draw): Link's OoT3D body wears
// Grezzo's painted tunic textures.
extern s32 gMergeShip3dsTunic;

// A Player animation path ("__OTR__..."): for the child, its "_child" version when the OoT3D pack has one.
const void* MergeShip3dsSkin_AgeAnimation(const char* path, s32 isChild);

// Nonzero while a skeleton whose 3DS package drew is traversed: limb display lists are skipped.
extern s32 gMergeShip3dsSuppressLimbs;

#ifdef __cplusplus
}
#endif
