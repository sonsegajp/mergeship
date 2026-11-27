#pragma once

// MergeShip2: the Ocarina of Time 3D port is switched off for now (the user, 2026-11-27: "comment out 3ds stuff
// atm"). With 0, every function below is a stub that leaves the game as it draws without the pack
// (MergeShipPort3ds.cpp, the end of the file), and the "3DS Assets" options are hidden (SohMenuMergeShip.cpp).
// Real-time shadows are not part of the port and keep working. 1 brings the port back as it was.
#ifndef MERGESHIP_PORT3DS
#define MERGESHIP_PORT3DS 0
#endif

// MergeShip2: Ocarina of Time 3D assets (tools/port3ds writes them to mods/port3ds.o2r). Nothing here changes how
// the game works: the pack only supplies display lists, and the options below choose them over the N64 ones.
//
// Rooms, option CVAR_ENHANCEMENT("MergeShip.Port3ds.Rooms") (default off): a room whose 3DS model is in the pack is
// drawn from "port3ds/<room resource path>/opa" and ".../xlu" instead of its own mesh header. Rooms the pack does
// not hold draw as always.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// The 3DS display lists of `room`, when the option is on and the pack has them. Either list may come back NULL
// (a room with nothing translucent). Returns 0 when the room draws as usual.
//
// The scene's 3DS light settings for the current scene setup are "port3ds/<scene folder>/lights_<setup>", arranged
// by the light index Ocarina of Time uses. MergeShipPort3ds_UpdateSceneLight blends them once a frame the way the game
// blends its own (for the current room, whichever option is on), and MergeShipPort3ds_SceneLight gives the result
// (0 when the pack has none for the scene: the game's own light is used then).
//
// How the 3DS rooms are drawn (tools/port3ds/roomconv.py, picacomb.py):
// - Every material pushes its own shader, the 3DS game's texture combiner for that material, and sets its own blend.
//   "<list>/anim" names those shaders and the commands the material animations rewrite each frame (texture
//   scrolls, combiner constant colours); this file patches them in.
// - Most surfaces take the ambient light alone (vertex colour * 2 * ambient in their shader). The ambient is the key
//   colour's centre, which each list takes from segment 0x08 at its top; the room draw fills that each frame.
// - The rest add the two directional lights by their normals. Their vertex colours are computed, as the 3DS game
//   does per vertex, by MergeShipPort3ds_RelightRoom whenever the light changes ("<room>/opa/lit" and ".../xlu/lit"
//   name those vertices; each keeps its normal in the vertex's unused flag field). The pack ships them lit for noon
//   outdoors and the first setting indoors.
//
// `handle`: the room's data for MergeShipPort3ds_RelightRoom.
s32 MergeShipPort3ds_RoomLists(PlayState* play, Room* room, Gfx** opa, Gfx** xlu, void** handle);
void MergeShipPort3ds_UpdateSceneLight(PlayState* play, Room* room);
s32 MergeShipPort3ds_SceneLight(EnvLightSettings* out);

// Relights the room's directionally lit vertices for `light` (ambient and both lights' colours and directions).
// Does nothing while the light is what it was at the last call.
void MergeShipPort3ds_RelightRoom(void* handle, EnvLightSettings* light);

// 1 while the current room was drawn from the pack this frame. Interiors that Ocarina of Time shows as a pre-rendered
// panorama drawn over the room are real geometry on the 3DS, so Play_Draw leaves the panorama out then.
s32 MergeShipPort3ds_RoomDrawn(void);
void MergeShipPort3ds_SetRoomDrawn(s32 drawn);

// Characters, option CVAR_ENHANCEMENT("MergeShip.Port3ds.Models") (default off): a skeleton whose 3DS model is in
// the pack ("port3ds/skel/<skeleton resource path>/...") is drawn from that model. The game walks its own skeleton
// as always, with its own animation and limb callbacks; between SkeletonBegin and SkeletonEnd, SkeletonLimb takes
// each limb's matrix and returns 1 so that the limb's own display list is left out, and SkeletonEnd poses the
// model's bones from those matrices and draws it, lit as the 3DS game lights a character.
//
// OnSkeletonLoaded: the game loaded a skeleton resource (ResourceMgr_LoadSkeletonByName).
//
// What the model has nothing for stays the game's: a limb the rig lists as kept (Navi's glowing ball, which both
// games draw from code and no model holds) gets 0 from SkeletonLimb and draws its own list.
//
// An actor's own colours reach the model the way they reach its limbs. The game's code sets the colour registers
// (environment, primitive) before the skeleton or, limb by limb, in its limb callback (Gohma's eye goes red while
// the body keeps its colour); a material of the model that stands for limbs reading such a register has a constant
// bound to it (tools/port3ds/creatures.py). SkeletonLimb leaves a mark in the display buffer at each limb that makes
// the renderer keep the registers as they are there, and the model's lists take them from the mark of the limb each
// part belongs to (libultraship's G_REGS_EX).
void MergeShipPort3ds_OnSkeletonLoaded(const char* path, void** skeleton);
void MergeShipPort3ds_SkeletonBegin(PlayState* play, void** skeleton);
s32 MergeShipPort3ds_SkeletonLimb(s32 limb, Gfx* dList);
void MergeShipPort3ds_SkeletonEnd(PlayState* play);
// The same for the skeleton draws that write into a display buffer of the caller's (SkelAnime_Draw,
// SkelAnime_DrawFlex: an actor that fades draws itself into the translucent buffer through these): the model and its
// light binding go into `gfx`, and the advanced pointer comes back.
s32 MergeShipPort3ds_SkeletonLimbGfx(s32 limb, Gfx* dList, Gfx** gfx);
Gfx* MergeShipPort3ds_SkeletonEndGfx(PlayState* play, Gfx* gfx);

// What a model shows besides its body is chosen by the game's own rules: the rig names, per limb, the game's
// display lists and the model's groups that stand for each (Link's left hand holding the Master Sword, his sheath
// with the Hylian Shield and so on), and SkeletonLimb looks up the list the game is about to draw on that limb.
// The same holds for a list an actor's code draws itself from a limb callback (a Gerudo guard's hair after her head
// limb, the gravedigger's shovel after his hand): the walk reads what the callbacks put into the display buffers, and
// a list the rig has groups for is shown from the model and taken out of the buffer. A list the rig does not know
// stays the game's, drawn with the limb's matrix as always (SkeletonLimb loads it although the limb's list is left out).
// NoteLimbList: the list the game's own rules chose for a limb, where a later step replaces it with one the rig
// cannot know (Majora's Mask's young Link gets his hands composed each frame); called before the limb is drawn.
// SkeletonDrawn: 1 when the skeleton draw that just ended was drawn from a model.
void MergeShipPort3ds_NoteLimbList(s32 limb, Gfx* dList);
s32 MergeShipPort3ds_SkeletonDrawn(void);

// The 3DS game's own animations for whatever is drawn from a skeleton (the option "3DS Animations",
// CVAR_ENHANCEMENT("MergeShip.Port3ds.Animations"); tools/port3ds/actoranim.py, the pack's
// "port3ds/skel/<skeleton path>/anims"). The 3DS models are built anew, on skeletons of their own (Darunia has 35
// bones where the game has 17 limbs, legs that hang from his hips where the game's stand on feet of their own), so
// the game's limbs can only bend them roughly. With the option on, a model is posed by its own skeleton from the
// 3DS animation that the pack pairs with the animation the game is playing, at the game's frame, blended as the
// game blends from one animation into the next; what the game's code turns by hand on a limb (a head that follows
// Link) is turned on that limb's bone. The game's skeleton goes on being walked for everything the code needs it
// for. An animation the pack has no 3DS one for is posed from the limbs as before.
// OnSkelAnime: where the game sets a SkelAnime up (alive 1) or frees it (0): which joint table is whose.
// SkeletonJoints: straight after SkeletonBegin, the joint table that skeleton draw poses its limbs from.
void MergeShipPort3ds_OnSkelAnime(SkelAnime* skelAnime, s32 alive);
void MergeShipPort3ds_SkeletonJoints(Vec3s* jointTable);

// Link's look for his next skeleton draw (Player_DrawImpl): his tunic (0 Kokiri, 1 Goron, 2 Zora), what he wears
// that the model has pieces for, and the gauntlets' colour.
#define PORT3DS_WEAR_GAUNTLETS 0x01
#define PORT3DS_WEAR_IRON_BOOTS 0x02
#define PORT3DS_WEAR_HOVER_BOOTS 0x04
#define PORT3DS_WEAR_BRACELET 0x08
#define PORT3DS_WEAR_LONGSHOT 0x10 // (his Hookshot is the Longshot)
#define PORT3DS_WEAR_LONG_SWORD 0x20 // (his sword is the Biggoron's Sword or the Giant's Knife)
// (not something he wears: he is seen through his own eyes, aiming or looking around. The game's camera is at its
// own head limb then, and the 3DS skeleton's head is not where that is, so the model is held by its head.)
#define PORT3DS_LOOK_FIRST_PERSON 0x40
void MergeShipPort3ds_SetPlayerLook(s32 tunic, u32 wear, u8 r, u8 g, u8 b);

// 3DS animations, option CVAR_ENHANCEMENT("MergeShip.Port3ds.Animations") (default off): Link's animations as the
// 3DS game has them, converted to the game's own frame tables (tools/port3ds/linkanim.py, "port3ds/anim/..."), so
// everything the game does with an animation goes on working. PlayerAnimFrame fills `frameTable` with one frame of
// `animation` (a resource path) and returns 1, or returns 0 when the pack has no 3DS version of it.
s32 MergeShipPort3ds_PlayerAnimFrame(PlayState* play, const char* animation, s32 frame, s32 limbCount,
                                     Vec3s* frameTable);
// A drawn bow (or slingshot) under those animations. The game pins the nocked arrow to the hand that pulls and the
// string to the hand that holds the bow by constants made for its own animation and bow, and an arrow flies the way
// it lay. The 3DS bow sits in the fist turned against the game's and the 3DS animation is made for that, so with
// the game's constants the arrow lies and flies some 35 degrees off (tools/port3ds/linkanim.py, aim_numbers; the
// pack's "port3ds/anim/<age>/aim"). PlayerAim returns 1 while the option is on, Link is himself and the pack has
// the numbers, and gives (each where the pointer is not NULL): `atBow` the arrow against the hand that holds the
// bow; `sits` how the bow sits in that hand against the game's (identity while the game's own model is drawn);
// `turn` the turn of the whole upper body, after the game's own aiming turns, that puts that arrow where the game
// aims. `firstPerson`: the list of the hand is the first person's.
s32 MergeShipPort3ds_PlayerAim(PlayState* play, s32 firstPerson, MtxF* atBow, MtxF* sits, MtxF* turn);

// The sky a scene has in the 3DS game, where that is not the one Ocarina of Time gives it: the Market, its back
// alley, the Temple of Time's grounds, the castle courtyard, Zora's Fountain and the Haunted Wasteland have the
// ordinary sky of the time of day there, and the ruined Market the overcast sunset; Ocarina of Time paints those
// skies into a panorama or has none. ("port3ds/<scene folder>/sky_<setup>".) Play_InitScene asks before it sets the
// sky up; the answer is `skyboxId` itself when the rooms option is off or the pack says nothing else.
s16 MergeShipPort3ds_SceneSkybox(PlayState* play, s16 skyboxId);
// The same for a room's own switches (sky off, sun and moon off): Ocarina of Time switches the sky off in the rooms
// it backs with a panorama, where the 3DS room shows the sky ("port3ds/<room path>/sky_<setup>"). Called by the room
// command that sets them, with the room's number already current; leaves them alone when the rooms option is off.
void MergeShipPort3ds_RoomSkyboxDisables(PlayState* play, u8* skyboxDisabled, u8* sunMoonDisabled);
// The view distance, at the start of a frame's draw. Ocarina of Time shows the Market, its entrance, the back alley
// and the Temple of Time's grounds as painted backgrounds from fixed cameras, with a short view distance and near
// fog to suit; the 3DS rooms are those places in full, seen from anywhere, so while one is drawn the view reaches as
// far as it does outdoors. (Until the 3DS fog itself is drawn: its distances are in the pack's light lists.)
void MergeShipPort3ds_SceneView(PlayState* play);

// Objects, option CVAR_ENHANCEMENT("MergeShip.Port3ds.Objects") (default off): what the game draws from a plain
// display list (a pot, a crate, a sign, an item's model) is drawn from the 3DS model that stands for that list,
// where the pack has one ("port3ds/dl/<the list's resource path>", tools/port3ds/objects.py). The 3DS models are
// in the lists' own space, so the game's matrix, lights and draw order carry them as they are. Asked wherever the
// game turns a list's path into the list (ResourceMgr_LoadGfxByName); returns the pack's list, or NULL.
Gfx* MergeShipPort3ds_ObjectList(const char* path);

// z_kankyo.c: the light setting Environment_Update's blend gives for another settings list (ambient, both lights).
s32 Environment_BlendLightsOf(PlayState* play, EnvLightSettings* list, s32 count, EnvLightSettings* out);
// The same for a scene the 3DS game lights by the time of day while this game lights it with one fixed setting:
// `list` is the 3DS scene's own (dawn, day, dusk, night first), blended by the clock as an outdoor scene's is.
s32 Environment_BlendLightsByTime(PlayState* play, EnvLightSettings* list, s32 count, EnvLightSettings* out);

// Real-time shadows (z_play.c, Play_MergeShipShadows; the "Real-Time Shadows" option). Whether the renderer in use
// can draw them (Direct3D 11), and whether this frame's world is drawn with them (the actors' own round shadows are
// then left out).
s32 MergeShipPort3ds_ShadowsSupported(void);
// More grass (the "More Grass" option): tufts on the floors the game calls grass, around the camera. Drawn after
// the rooms.
void MergeShipPort3ds_DrawGrass(PlayState* play);
s32 Play_MergeShipShadowsOn(void);
// Whether the pass over the screen draws the game's lamps' light this frame (the "Real Light Sources" option): the
// game's own light from them on a character is then left out (z_lights.c).
s32 Play_MergeShipLampsOn(void);

#ifdef __cplusplus
}
#endif
