/*
 * MergeShip2 engine API: the C interface between mergeship.exe (the host) and a game module
 * (soh.dll = Ship of Harkinian / OoT, 2ship.dll = 2 Ship 2 Harkinian / MM).
 *
 * Both modules link one libultraship.dll. The host owns the process, the frame loop and the window
 * switch; each module owns its game and its own Ship::Context (config, CVars, resources, console).
 * See Shipwright/mergeship2/docs/HOST_DESIGN.md.
 *
 * Pure C on purpose: the two modules are built by different CMake trees, and nothing but plain data
 * and function pointers crosses this boundary.
 */
#ifndef MERGESHIP_ENGINE_API_H
#define MERGESHIP_ENGINE_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped whenever a struct below or an export's meaning changes.
 * 2: MsTransfer.saveSlot/playerName, MS_TRANSFER_FLAG_TERMINA_DOOR, MsHostServices shared-save calls (M3, M4). */
#define MS_ENGINE_API_VERSION 2
/* (MsHostServices.net was appended without a bump: engines test MS_HOST_HAS(services, net), so an older host or
 * engine keeps working with its own Anchor connection. Likewise MsHostServices.GetEngineContext: without it, settings
 * are shared only when SoH is the first engine, through shareContext.) */

struct MsHostNet; /* AnchorNet.h */

#if defined(_WIN32)
#if defined(MS_ENGINE_BUILDING)
#define MS_ENGINE_EXPORT __declspec(dllexport)
#else
#define MS_ENGINE_EXPORT __declspec(dllimport)
#endif
#else
#define MS_ENGINE_EXPORT __attribute__((visibility("default")))
#endif

typedef enum MsEngineId {
    MS_ENGINE_NONE = 0,
    MS_ENGINE_SOH = 1,   /* Ocarina of Time, soh.dll */
    MS_ENGINE_2SHIP = 2, /* Majora's Mask, 2ship.dll */
} MsEngineId;

/* What MsEngine_RunFrame reports after running one game frame. */
typedef enum MsFrameResult {
    MS_FRAME_RUNNING = 0,  /* keep calling RunFrame */
    MS_FRAME_TRANSFER = 1, /* the game asked (through RequestTransfer) to hand over to another engine */
    MS_FRAME_EXITED = 2,   /* the game ended or its window was closed */
    MS_FRAME_ERROR = 3,    /* unrecoverable; the host should shut down */
} MsFrameResult;

/* Transfer flags. */
#define MS_TRANSFER_FLAG_NONE 0u
/* Through the door between the worlds: Hyrule's Lost Woods <-> Termina's Clock Tower (M3, HOST_DESIGN.md).
 * Each game gates its side with the CVar MS_CVAR_TERMINA_DOOR (same name in both games' configs). */
#define MS_TRANSFER_FLAG_TERMINA_DOOR 1u
/* The sender captured the player's state into the slot's shared save (M4): the receiver applies it. */
#define MS_TRANSFER_FLAG_SHARED_SAVE 2u
/* playerAge and playerForm are set (a zero-initialized MsTransfer would otherwise read as adult Link). */
#define MS_TRANSFER_FLAG_PLAYER_BODY 4u
/* Entrance randomizer (unified randomizer M8): `entrance` is a key of include/mergeship/MsEntrances.h (an Ocarina of
 * Time index for SoH, a Termina key for 2Ship), not a raw entrance of the receiving game. */
#define MS_TRANSFER_FLAG_RANDO_ENTRANCE 0x100u
/* entranceTable / entranceTableCount are set (MS_TRANSFER_HAS them too). */
#define MS_TRANSFER_FLAG_ENTRANCE_TABLE 0x200u

#define MS_CVAR_TERMINA_DOOR "gEnhancements.MergeShip.TerminaDoor"

/* "Share items during Termina's first cycle" (both games' Mergeship menus, default on; SharedSave.h): the shared save
 * takes part from the first arrival in Termina instead of after Majora's Mask's first Song of Time. 2Ship decides (the
 * first cycle is its own); SoH, whose config holds its own copy of the option, turns it off for the trip by sending
 * MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE. Sharing happens when neither menu has it off. */
#define MS_CVAR_SHARE_FIRST_CYCLE "gEnhancements.MergeShip.ShareFirstCycle"
#define MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE 0x400u
/* worldCatalog is set (MS_TRANSFER_HAS it too): the sender's world's checks for the receiver's hints and trackers
 * (MsWorldCatalog.h; the unified randomizer, SoH -> 2Ship). */
#define MS_TRANSFER_FLAG_WORLD_CATALOG 0x800u
/* The Hyrule file plays a unified randomizer seed (SoH -> 2Ship, every trip): unifiedSeed / unifiedSeedFile name it,
 * and Termina's file of this slot is a randomizer file of that seed (2Ship: Rando/MergeShip/UnifiedSeed.h). A trip
 * from SoH whose MsTransfer has those fields (MS_TRANSFER_HAS) but not this flag comes from a file that is not unified:
 * 2Ship forgets any seed an earlier trip named. */
#define MS_TRANSFER_FLAG_UNIFIED_SEED 0x1000u
/* The Song of Soaring from Hyrule (SoH -> 2Ship): `entrance` is the owl statue (Majora's Mask's OWL_WARP_*, 0-9) the
 * player soared to; 2Ship arrives at that statue's owl-warp entrance (sOwlWarpEntrances), as Majora's Mask's own warp. */
#define MS_TRANSFER_FLAG_OWL_WARP 0x2000u
/* Singleplayer mode (SoH -> 2Ship; SoH MergeShipSingleplayer.h, never on a randomizer file): Termina is Ocarina of
 * Time's New Game Plus -- Navi in Tatl's place (her colours, voice and lines). With SP_FD_FROM_MOON the moon's child
 * still gives the Fierce Deity's Mask; without it only the Happy Mask Salesman does. Sent on every trip. */
#define MS_TRANSFER_FLAG_SINGLEPLAYER 0x4000u
#define MS_TRANSFER_FLAG_SP_FD_FROM_MOON 0x8000u
/* Anchor's warp to a teammate in the other world (BUG-0036): `entrance` is the receiving game's own entrance, and Link
 * arrives at the teammate's place there (anchorRoom / anchorPos / anchorYaw, as a void-out respawn without damage). */
#define MS_TRANSFER_FLAG_ANCHOR_WARP 0x10000u
#define MS_TRANSFER_SEED_FILE_MAX 1024

struct MsWorldCatalog; /* MsWorldCatalog.h */

/* The door's arrivals, each in the receiving game's own entrance numbering. */
#define MS_ENTRANCE_SOH_LOST_WOODS 0x011E  /* ENTR_LOST_WOODS_SOUTH_EXIT: the woods' first clearing */
#define MS_ENTRANCE_2SHIP_CLOCK_TOWN 0xD800 /* ENTRANCE(SOUTH_CLOCK_TOWN, 0): out of the Clock Tower's door */

/* A handoff between engines (M3+). `size` is sizeof(MsTransfer) as the sender knew it. */
typedef struct MsTransfer {
    uint32_t size;
    int32_t from;           /* MsEngineId */
    int32_t to;             /* MsEngineId */
    int32_t entrance;       /* destination entrance in the receiving game's own numbering; -1 = none */
    uint32_t flags;         /* MS_TRANSFER_FLAG_* */
    void* sharedSave;       /* host-owned MsSharedSave (SharedSave.h) of saveSlot; may be NULL */
    uint32_t sharedSaveSize;
    /* The Ocarina of Time file (0-2) the trip belongs to; each has its own Majora's Mask file (the same slot
     * number). -1: an unsaved file (SoH's debug/test file). */
    int32_t saveSlot;
    char playerName[12];    /* the file's player name, ASCII, NUL-terminated (names a new MM file) */
    /* The player's body, with MS_TRANSFER_FLAG_PLAYER_BODY. Appended: a receiver reads them only if the flag is set
     * and MS_TRANSFER_HAS(transfer, field). */
    int32_t playerAge;      /* Ocarina of Time LINK_AGE_*: 0 adult, 1 child; -1 not given */
    int32_t playerForm;     /* MS_FORM_*: the age's human form, or a transformation; -1 not given */
    /* With MS_TRANSFER_FLAG_ENTRANCE_TABLE (appended, M8): the seed's overrides of Termina's entrances as
     * entranceTableCount pairs {key, override} (MsEntrances.h). Owned by the sender, valid for the process. */
    const int16_t* entranceTable;
    int32_t entranceTableCount;
    /* With MS_TRANSFER_FLAG_WORLD_CATALOG (appended, unified randomizer M6/M7): the sender's world's checks, names,
     * areas and placements (MsWorldCatalog.h). Owned by the sender, valid until its next trip: copy it on arrival. */
    const struct MsWorldCatalog* worldCatalog;
    /* With MS_TRANSFER_FLAG_UNIFIED_SEED (appended): the unified seed the Hyrule file plays. unifiedSeed is SoH's
     * ctx->GetSeed() (== the seed's "mm" finalSeed == MsSharedSave.ledgerSeed); unifiedSeedFile is its MERGESHIP_SEED
     * file as an absolute UTF-8 path, NUL-terminated ("" if SoH found none: 2Ship then looks for the seed itself). */
    uint32_t unifiedSeed;
    char unifiedSeedFile[MS_TRANSFER_SEED_FILE_MAX];
    /* With MS_TRANSFER_FLAG_ANCHOR_WARP (appended): where the teammate stands, in the receiving game's scene. */
    int32_t anchorRoom;
    float anchorPos[3];
    int32_t anchorYaw;
} MsTransfer;

/* True if the sender's MsTransfer (whose `size` it set to its own sizeof) includes `field`. */
#define MS_TRANSFER_HAS(transfer, field) \
    ((transfer)->size >= offsetof(MsTransfer, field) + sizeof(((MsTransfer*)0)->field))

/* MsTransfer.playerForm: SoH's PlayerForm numbering (MergeShipForms.h), which extends Ocarina of Time's ages. */
#define MS_FORM_ADULT 0        /* Ocarina of Time's adult Link */
#define MS_FORM_CHILD 1        /* young Link (Majora's Mask's human) */
#define MS_FORM_DEKU 2
#define MS_FORM_GORON 3
#define MS_FORM_ZORA 4
#define MS_FORM_FIERCE_DEITY 5

/* Returned by MsEngine_GetAbi. The host refuses to run a module whose values do not match. */
typedef struct MsEngineAbi {
    uint32_t size;          /* sizeof(MsEngineAbi) */
    uint32_t apiVersion;    /* MS_ENGINE_API_VERSION the module was built with */
    int32_t engineId;       /* MsEngineId */
    const char* engineName; /* "Ship of Harkinian", "2 Ship 2 Harkinian" */
    const char* lusAbiId;   /* libultraship "<commit>-<option hash>" the module was compiled against */
} MsEngineAbi;

/* Host flags passed in MsHostServices.flags. */
#define MS_HOST_FLAG_NONE 0u
#define MS_HOST_FLAG_MUTED 1u /* start with audio muted (automated tests) */

/* Services the host offers a module. Valid from MsEngine_Init until MsEngine_Shutdown returns. */
typedef struct MsHostServices {
    uint32_t size;       /* sizeof(MsHostServices) */
    uint32_t apiVersion; /* MS_ENGINE_API_VERSION the host was built with */
    uint32_t flags;      /* MS_HOST_FLAG_* */
    void* host;          /* opaque; passed back to every callback */
    /* The Ship::Context* of the engine that owns the window, or NULL when this module is the first
     * (it then creates the window, audio device and controllers exactly as the standalone game does). */
    void* shareContext;
    /* Ask the host to hand over to another engine after the current frame. */
    void (*RequestTransfer)(void* host, const MsTransfer* transfer);
    /* Log through the host (goes to the host's log and console). level: 0 info, 1 warn, 2 error. */
    void (*Log)(void* host, int32_t level, const char* message);
    /* M4: the host-owned shared save (MsSharedSave, SharedSave.h) of an Ocarina of Time file slot (0-2, or -1
     * for the unsaved debug file). Loaded from disk on first use; valid until MsEngine_Shutdown. Never NULL. */
    void* (*GetSharedSave)(void* host, int32_t saveSlot);
    /* Writes the slot's shared save to disk (after an engine captured its state into it). No-op for -1. */
    void (*CommitSharedSave)(void* host, int32_t saveSlot);
    /* Appended (read it only if MS_HOST_HAS(services, net)): the host's one Anchor connection, shared by both engines
     * so that crossing between the worlds never leaves the room (AnchorNet.h). NULL: the engine connects itself. */
    const struct MsHostNet* net;
    /* Appended (read it only if MS_HOST_HAS(services, GetEngineContext)): the Ship::Context* of a loaded engine
     * (MsEngineId), or NULL before that engine's MsEngine_Init. 2Ship reads SoH's settings through it (SettingsSync.h). */
    void* (*GetEngineContext)(void* host, int32_t engineId);
    /* Appended (read it only if MS_HOST_HAS(services, ExtractRom)): builds a loaded engine's ROM archive with that
     * engine's own extractor (its optional export MsEngine_ExtractRom), even before that engine's MsEngine_Init. SoH, the
     * first engine, uses it on a first run so that one question per ROM produces every archive both games need (2Ship:
     * mm.o2r). Blocks until done. Returns 0 on success; nonzero when the engine is not loaded, has no extractor, or the
     * extraction failed. romPath NULL only asks whether the engine can extract (0: yes). */
    int32_t (*ExtractRom)(void* host, int32_t engineId, const char* romPath, const char* destDir,
                          void (*progress)(void* user, uint64_t done, uint64_t total), void* user);
    /* Appended (read them only if MS_HOST_HAS(services, GetMenuMirror)), BUG-0096: an engine's menu as plain data
     * (MenuMirror.hpp MsMenuMirror, owned by that engine, valid until its MsEngine_Shutdown), which the other engine's
     * menu draws. Set once by each engine at the end of its MsEngine_Init; NULL until then. */
    void (*SetMenuMirror)(void* host, int32_t engineId, const struct MsMenuMirror* mirror);
    const struct MsMenuMirror* (*GetMenuMirror)(void* host, int32_t engineId);
} MsHostServices;

/* True if the host's MsHostServices (whose `size` it set to its own sizeof) includes `field`. */
#define MS_HOST_HAS(services, field) \
    ((services)->size >= offsetof(MsHostServices, field) + sizeof(((MsHostServices*)0)->field))

/* The module's exports. The host looks them up by these names. */
MS_ENGINE_EXPORT const MsEngineAbi* MsEngine_GetAbi(void);
/* Starts the game up to (not including) its first frame. Returns 0 on success. The module is the
 * active engine when this returns. */
MS_ENGINE_EXPORT int32_t MsEngine_Init(const MsHostServices* host, int32_t argc, char** argv);
/* Runs one game frame (update, draw, present). Returns an MsFrameResult. */
MS_ENGINE_EXPORT int32_t MsEngine_RunFrame(void);
/* Stops being the active engine: parks audio, saves CVars, takes its GUI out of the shared window. */
MS_ENGINE_EXPORT void MsEngine_Suspend(void);
/* Becomes the active engine again. `arrival` is the transfer that caused it, or NULL. */
MS_ENGINE_EXPORT void MsEngine_Resume(const MsTransfer* arrival);
/* Saves and releases what the game can release without tearing down shared subsystems. */
MS_ENGINE_EXPORT void MsEngine_Shutdown(void);
/* The module's Ship::Context* (opaque to C), for MsHostServices.shareContext of later modules. */
MS_ENGINE_EXPORT void* MsEngine_GetContext(void);

typedef const MsEngineAbi* (*MsEngine_GetAbi_Fn)(void);
typedef int32_t (*MsEngine_Init_Fn)(const MsHostServices* host, int32_t argc, char** argv);
typedef int32_t (*MsEngine_RunFrame_Fn)(void);
typedef void (*MsEngine_Suspend_Fn)(void);
typedef void (*MsEngine_Resume_Fn)(const MsTransfer* arrival);
typedef void (*MsEngine_Shutdown_Fn)(void);
typedef void* (*MsEngine_GetContext_Fn)(void);
/* Optional export (MsHostServices.ExtractRom): extracts the game's ROM archive from romPath into destDir with the
 * module's own extractor. Callable before MsEngine_Init; blocks; progress (may be NULL) is called from the calling
 * thread. Returns 0 on success. */
typedef int32_t (*MsEngine_ExtractRom_Fn)(const char* romPath, const char* destDir,
                                          void (*progress)(void* user, uint64_t done, uint64_t total), void* user);

#ifdef __cplusplus
}
#endif

#endif /* MERGESHIP_ENGINE_API_H */
