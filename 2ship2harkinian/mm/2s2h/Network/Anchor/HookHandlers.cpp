// MergeShip2 -- Anchor's hooks in 2 Ship 2 Harkinian, ported from Ship of Harkinian's HookHandlers.cpp.
//
// SoH hooks what 2Ship has no hook for (a player sound, an ocarina note, a key used, a check's status, a save). Here
// those are read once a frame instead (PollGameState): the scene the player is in, the dungeon items and keys, the
// randomizer's obtained checks and the ocarina's note, each sent when it changes. Only a player sound is observed
// through a vanilla-behaviour hook (VB_LINK_VOICE_PITCH_MULTIPLIER, asked by every Player_PlaySfx).
#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h" // MergeShip2: ResolvedItemOf
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "overlays/actors/ovl_En_Si/z_en_si.h"
extern PlayState* gPlayState;
// The ocarina the player is playing (src/audio/code_8019AF00.c)
extern u8 sCurOcarinaPitch;
extern s8 sOcarinaInstrumentId;
extern f32 sCurOcarinaBendFreq;
extern s8 sCurOcarinaBendIndex;
}

namespace {
// Set while Anchor itself plays a teammate's sound on a dummy (not to be sent back).
bool sPlayingRemoteSfx = false;
// Between Majora's Mask's Song of Time save and its end: the cycle reset is not a teammate's business.
bool sEndOfCycle = false;
// A Song of Time save happened: Termina's part of the team state is kept for Hyrule on the next frame.
bool sPersistTeamStatePending = false;
} // namespace

// MergeShip2: Termina's part of the team state is kept for Hyrule after every Song of Time save, whether Anchor is
// connected or not (MergeShipAnchor_PersistTerminaTeamState: SoH sends it while the player is in Hyrule, this run or the
// next). On the frame after: Majora's Mask's own options put back what they keep through the Song of Time (rupees,
// consumables, Enhancements/Cycle/EndOfCycle.cpp) in hooks of the same moment, in no set order with this one. The frame
// hook also tells Anchor which thread is the game's (Anchor::MergeShipOnGameThread).
static RegisterShipInitFunc sAnchorPersistInit([]() {
    COND_HOOK(AfterEndOfCycleSave, true, []() { sPersistTeamStatePending = true; });
    // The Hyrule part last received belongs to the file it came with: another file (of the same seed, or a file without
    // one) must not send it as its own. Connected or not: one received earlier would otherwise outlive the file.
    COND_HOOK(OnSaveLoad, true, [](s16 fileNum) {
        if (Anchor::Instance != nullptr) {
            Anchor::Instance->ootTeamState = nlohmann::json();
        }
    });
    COND_HOOK(OnGameStateUpdate, true, []() {
        Anchor* anchor = Anchor::Instance;
        if (anchor == nullptr) {
            return;
        }
        anchor->MergeShipNoteGameThread();
        if (sPersistTeamStatePending && anchor->IsSaveLoaded()) {
            sPersistTeamStatePending = false;
            MergeShipAnchor_PersistTerminaTeamState(anchor);
        }
    });
});

void MergeShipAnchor_PlayRemoteSfx(Player* dummy, u16 sfxId) {
    sPlayingRemoteSfx = true;
    Player_PlaySfx(dummy, sfxId);
    sPlayingRemoteSfx = false;
}

void Anchor::RegisterHooks() {

    // #region Hooks that are required for basic Anchor functionality

    // The loopback test player (MergeShipAnchorHost.cpp) needs the dummy-player hooks without a server.
    bool dummyPlayers = isConnected || mergeShipLoopback;

    COND_HOOK(OnSceneInit, dummyPlayers, [&](s8 sceneId, s8 spawnNum) {
        // The dummies are spawned once the player exists (OnActorUpdate below).
        shouldRefreshActors = true;
        SendPacket_UpdateClientState();
    });

    COND_ID_HOOK(ShouldActorInit, ACTOR_PLAYER, dummyPlayers, [&](Actor* actor, bool* should) {
        if (spawningDummyPlayerForClientId != 0) {
            SetDummyPlayerClientId(actor, spawningDummyPlayerForClientId);

            // By the time we get here, the actor was already added to the ACTORCAT_PLAYER list, so we need to move it
            Actor_ChangeCategory(gPlayState, &gPlayState->actorCtx, actor, ACTORCAT_NPC);
            actor->id = ANCHOR_DUMMY_ACTOR_ID;
            actor->category = ACTORCAT_NPC;
            actor->init = DummyPlayer_Init;
            actor->update = DummyPlayer_Update;
            actor->draw = DummyPlayer_Draw;
            actor->destroy = DummyPlayer_Destroy;
        }
    });

    // Player_Update of the player himself (a dummy's id is not ACTOR_PLAYER; Kafei is En_Test3).
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, dummyPlayers, [&](Actor* actor) {
        if (gPlayState == nullptr || actor != (Actor*)GET_PLAYER(gPlayState)) {
            return;
        }
        if (justLoadedSave) {
            justLoadedSave = false;
            SendPacket_RequestTeamState();
        }

        if (shouldRefreshActors) {
            shouldRefreshActors = false;
            RefreshClientActors();
        }

        SendPacket_PlayerUpdate();
        MergeShipLoopbackFrame();

        if (isConnected) {
            MergeShip_CheckAmmoSync(); // MergeShip2: the shared ammo pool converging across the team (AnchorNet.h)
        }
    });

    COND_HOOK(OnGameStateUpdate, isConnected, [&]() {
        ProcessIncomingPacketQueue();
        PollGameState();
    });

    // Every Player_PlaySfx asks this (for Link's voice pitch); sfxId is its only argument.
    COND_VB_SHOULD(VB_LINK_VOICE_PITCH_MULTIPLIER, isConnected, {
        u16* sfxId = va_arg(args, u16*);
        if (!sPlayingRemoteSfx && sfxId != nullptr) {
            Anchor::Instance->SendPacket_PlayerSfx(*sfxId);
        }
    });

    COND_HOOK(OnSaveLoad, isConnected, [&](s16 fileNum) {
        justLoadedSave = true;
        ResetPollState();
    });

    COND_HOOK(BeforeEndOfCycleSave, isConnected, [&]() { sEndOfCycle = true; });

    COND_HOOK(AfterEndOfCycleSave, isConnected, [&]() {
        sEndOfCycle = false;
        // What the Song of Time took away was not taken from the team: start comparing afresh.
        ResetPollState();
        SendPacket_UpdateTeamState();
    });

    COND_HOOK(OnFlagSet, isConnected, [&](FlagType flagType, u32 flag) {
        if (!sEndOfCycle) {
            SendPacket_MMSetFlag(-1, flagType, (s32)flag);
        }
    });

    COND_HOOK(OnFlagUnset, isConnected, [&](FlagType flagType, u32 flag) {
        if (!sEndOfCycle) {
            SendPacket_MMUnsetFlag(-1, flagType, (s32)flag);
        }
    });

    COND_HOOK(OnSceneFlagSet, isConnected, [&](s16 sceneId, FlagType flagType, u32 flag) {
        if (!sEndOfCycle) {
            SendPacket_MMSetFlag(sceneId, flagType, (s32)flag);
        }
    });

    COND_HOOK(OnSceneFlagUnset, isConnected, [&](s16 sceneId, FlagType flagType, u32 flag) {
        if (!sEndOfCycle) {
            SendPacket_MMUnsetFlag(sceneId, flagType, (s32)flag);
        }
    });

    // Vanilla item gets (the randomizer's are read from its checks in PollGameState). MergeShip2: a bottle catch never
    // comes here (MM Player_UpdateBottleHeld does not go through Item_Give), and SendPacket_MMGiveItem drops a refill:
    // only a bottle itself is a found item (AnchorNet.h, "Bottles").
    COND_HOOK(OnItemGive, isConnected, [&](u8 item) {
        if (!IS_RANDO && !sEndOfCycle) {
            SendPacket_MMGiveItem(-1, item);
        }
    });

    COND_HOOK(OnGameCompletion, isConnected, [&]() { SendPacket_GameComplete(); });

    // #endregion

    // #region Hooks that are purely to sync actor states across the clients, not super essential

    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_ITEM00, isConnected, [&](Actor* actor) {
        EnItem00* item = (EnItem00*)actor;
        if (item->collectibleFlag > 0 && Flags_GetCollectible(gPlayState, item->collectibleFlag)) {
            Actor_Kill(actor);
        }
    });

    COND_ID_HOOK(ShouldActorUpdate, ACTOR_ITEM_B_HEART, isConnected, [&](Actor* actor, bool* should) {
        if (Flags_GetCollectible(gPlayState, 0x1F)) {
            Actor_Kill(actor);
            *should = false;
        }
    });

    // A teammate's gold skulltula token (the token's flag is a chest flag of the scene)
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_EN_SI, isConnected, [&](Actor* actor, bool* should) {
        if (Flags_GetTreasure(gPlayState, ENSI_GET_CHEST_FLAG(actor))) {
            Actor_Kill(actor);
            *should = false;
        }
    });

    // #endregion
}

void Anchor::ResetPollState() {
    lastDungeonItems.clear();
    lastDungeonKeys.clear();
    lastStrayFairies.clear();
    lastCheckObtained.clear();
}

// Once a frame on the game thread, after the incoming packets: what changed is sent.
void Anchor::PollGameState() {
    if (!IsSaveLoaded()) {
        if (lastStateSignature != -2) {
            lastStateSignature = -2;
            SendPacket_UpdateClientState(); // back at the title or file select
            ResetPollState();
        }
        return;
    }

    const s32 signature = ((s32)gPlayState->sceneId << 8) | (u8)gPlayState->roomCtx.curRoom.num;
    if (signature != lastStateSignature) {
        lastStateSignature = signature;
        SendPacket_UpdateClientState();
    }

    // The ocarina: a note started or stopped.
    if (sCurOcarinaPitch != lastOcarinaPitch) {
        lastOcarinaPitch = sCurOcarinaPitch;
        SendPacket_OcarinaSfx(sCurOcarinaPitch, sCurOcarinaBendFreq, sCurOcarinaBendIndex);
    }

    if (sEndOfCycle) {
        return;
    }

    // BUG-0049: an owl statue struck -- the team state (owlActivationFlags) goes at once, for teammates' Song of
    // Soaring in both worlds (they used to hear of it only with the next team state sent for something else).
    {
        static u16 sLastOwls = 0xFFFF;
        const u16 owls = gSaveContext.save.saveInfo.playerData.owlActivationFlags;
        if (sLastOwls == 0xFFFF) {
            sLastOwls = owls;
        } else if (owls != sLastOwls) {
            sLastOwls = owls;
            SendPacket_UpdateTeamState();
        }
    }

    // Dungeon items, small keys and stray fairies (SoH: UPDATE_DUNGEON_ITEMS on OnItemReceive / OnDungeonKeyUsed).
    const Inventory& inventory = gSaveContext.save.saveInfo.inventory;
    std::vector<u8> items(std::begin(inventory.dungeonItems), std::end(inventory.dungeonItems));
    std::vector<s8> keys(std::begin(inventory.dungeonKeys), std::end(inventory.dungeonKeys));
    std::vector<s8> fairies(std::begin(inventory.strayFairies), std::end(inventory.strayFairies));
    if (lastDungeonItems.empty()) {
        lastDungeonItems = items;
        lastDungeonKeys = keys;
        lastStrayFairies = fairies;
    } else if (items != lastDungeonItems || keys != lastDungeonKeys || fairies != lastStrayFairies) {
        lastDungeonItems = items;
        lastDungeonKeys = keys;
        lastStrayFairies = fairies;
        SendPacket_MMUpdateDungeonItems();
    }

    // The randomizer's checks (SoH: OnRandoSetCheckStatus, and OnItemReceive for the item).
    if (IS_RANDO) {
        if (lastCheckObtained.size() != RC_MAX) {
            lastCheckObtained.assign(RC_MAX, 0);
            for (s32 rc = 0; rc < RC_MAX; rc++) {
                lastCheckObtained[rc] = (RANDO_SAVE_CHECKS[rc].obtained ? 1 : 0) | (RANDO_SAVE_CHECKS[rc].skipped ? 2 : 0);
            }
        } else {
            for (s32 rc = 0; rc < RC_MAX; rc++) {
                const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
                const u8 now = (check.obtained ? 1 : 0) | (check.skipped ? 2 : 0);
                if (now == lastCheckObtained[rc]) {
                    continue;
                }
                const bool newlyObtained = (now & 1) && !(lastCheckObtained[rc] & 1);
                lastCheckObtained[rc] = now;
                if (newlyObtained) {
                    // The item this check actually gave (its progressive step; UnifiedDelivery.h ResolvedItemOf), so
                    // every teammate, in either world, gets that same item; else the placed one, which a teammate
                    // converts (progressive items, junk) for himself (Packets/GiveItem.cpp).
                    SendPacket_MMGiveItem(check.randoItemId, ITEM_NONE,
                                         Rando::MergeShip::ResolvedItemOf((RandoCheckId)rc));
                }
                SendPacket_MMSetCheckStatus(rc, check.obtained, check.skipped);
            }
        }
    }
}
