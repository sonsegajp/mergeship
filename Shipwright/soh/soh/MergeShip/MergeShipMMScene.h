#pragma once

// MergeShip2 -- MM scene-drawing helpers that MM objects' display lists depend on.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// MM Scene_SetRenderModeXlu (z_scene_proc.c). MM display lists call into segment 0x0C by index
// (G_DL_INDEX 0x0C, n), so the segment must point at MM's exact four-entry arrays: a one-entry
// list such as gEmptyDL makes index 2 run whatever follows it in memory.
//   index 0: none, 1: XLU single cycle, 2: XLU two cycle. flags: 1 = OPA, 2 = XLU.
void MergeShip_Scene_SetRenderModeXlu(PlayState* play, s32 index, u32 flags);

#ifdef __cplusplus
}
#endif
