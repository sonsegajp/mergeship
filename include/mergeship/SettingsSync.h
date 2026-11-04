/*
 * MergeShip2 settings sharing: the options a player sets in Ship of Harkinian (SoH) carry over to 2 Ship 2 Harkinian
 * (2Ship) inside mergeship.exe -- frame rate, V-Sync, MSAA, resolution, volumes, D-pad items and boots, D-pad ocarina,
 * aiming and camera options, time savers, cheats, colours, MergeShip's own shared options...
 *
 * The table is include/mergeship/SettingsMap.def (checked by tools/check_settings_map.py). The copying is done by
 * each game's MergeShipSettingsSync.cpp:
 *   - 2Ship (2ship2harkinian/mm/2s2h/MergeShip): at its start and on every resume, reads SoH's CVars (SoH's
 *     Ship::Context from MsHostServices.GetEngineContext) and writes the mapped values into its own, then re-runs what
 *     depends on them (ShipInit, the window's MSAA and resolution, the volumes). On suspend, a mapped value the player
 *     changed in 2Ship's own menu is written back into SoH's CVars (last writer wins).
 *   - SoH (Shipwright/soh/soh/MergeShip): on resume, re-runs what depends on a CVar 2Ship wrote back.
 * SoH's Mergeship menu option MS_CVAR_SHARE_SETTINGS (default on) turns it all off. soh.exe and 2ship.exe are
 * untouched (the files are compiled only with MERGESHIP_HOSTED).
 *
 * Pure C (no libultraship types): both engines include it.
 */
#ifndef MERGESHIP_SETTINGS_SYNC_H
#define MERGESHIP_SETTINGS_SYNC_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* SoH's Mergeship menu, "Share Settings with Termina" (default on). Read from SoH's config only. */
#define MS_CVAR_SHARE_SETTINGS "gEnhancements.MergeShip.ShareSettings"
/* SoH's Mergeship menu, "Form Tunic Colours": gates the MS_X_FORM_TUNIC rows (any form) and the MS_X_FD_TUNIC rows
 * (MS_SETTINGS_FORM_TUNIC_ALL only: 0 off, 1 the Deku, Goron and Zora, 2 every form -- SoH's default). */
#define MS_SETTINGS_FORM_TUNIC_GATE "gEnhancements.MergeShip.FormTunicColors"
#define MS_SETTINGS_FORM_TUNIC_ALL 2

typedef enum MsSettingType {
    MS_T_INT,
    MS_T_FLOAT,
    MS_T_COLOR,
} MsSettingType;

typedef enum MsSettingXform {
    MS_X_SAME,
    MS_X_NOT,
    MS_X_BOOL,
    MS_X_PCT,
    MS_X_DAMAGE,
    MS_X_MIRROR,
    MS_X_CROUCHSTAB,
    MS_X_SPEEDMODE,
    MS_X_FORM_TUNIC,
    MS_X_FD_TUNIC,
} MsSettingXform;

#define MS_F_NONE 0u
#define MS_F_TO_2SHIP 1u /* one-way: never written back into SoH */
#define MS_F_IF_SET 2u   /* only while SoH has the CVar set */
#define MS_F_UNSET 4u    /* same name and code default in both: SoH unset -> 2Ship unset */

typedef struct MsSettingMapping {
    const char* soh;
    const char* ship;
    MsSettingType sohType;
    MsSettingType shipType;
    double sohDefault;
    double shipDefault;
    MsSettingXform xform;
    uint32_t flags;
} MsSettingMapping;

static const MsSettingMapping kMsSettingsMap[] = {
#define MS_SETTING(soh, ship, sohType, shipType, sohDefault, shipDefault, xform, flags) \
    { soh, ship, sohType, shipType, sohDefault, shipDefault, xform, flags },
#include "SettingsMap.def"
#undef MS_SETTING
};

#define MS_SETTINGS_COUNT (sizeof(kMsSettingsMap) / sizeof(kMsSettingsMap[0]))

/* SoH's value -> 2Ship's (numbers; colours are copied as they are). */
static inline double MsSetting_ToShip(const MsSettingMapping* m, double v) {
    static const double kMirror[] = { 0, 1, 2, 3, 6, 6, 0, 7, 8 }; /* SoH MirroredWorldMode -> 2Ship's */
    int i = (int)lround(v);
    switch (m->xform) {
        case MS_X_NOT:
            return (i != 0) ? 0 : 1;
        case MS_X_BOOL:
            return (i != 0) ? 1 : 0;
        case MS_X_PCT:
            return v / 100.0;
        case MS_X_DAMAGE:
            /* SoH DamageMultType 0..7 (1x..128x), 8 OHKO -> 2Ship 0..4 (1x..16x, beyond: 16x), 10 "1 Hit KO" */
            return (i <= 4) ? (i < 0 ? 0 : i) : (i == 8) ? 10 : 4;
        case MS_X_MIRROR:
            /* all dungeons / vanilla-only dungeons -> all of Termina's; MQ-only -> off (Termina has none) */
            return ((i >= 0) && (i < (int)(sizeof(kMirror) / sizeof(kMirror[0])))) ? kMirror[i] : 0;
        case MS_X_CROUCHSTAB:
            return (i != 0) ? 0 : 2; /* removed -> "Patched (US/EU)"; kept -> "Unpatched (OoT)" */
        case MS_X_SPEEDMODE:
            return (i != 0) ? 3 : 2; /* "Toggle Buttons" / "Hold Buttons" */
        default:
            return v;
    }
}

/* 2Ship's value -> SoH's (rows without MS_F_TO_2SHIP). */
static inline double MsSetting_ToSoh(const MsSettingMapping* m, double v) {
    static const double kMirror[] = { 0, 1, 2, 3, 4, 4, 4, 7, 8 }; /* 2Ship MirroredWorldModeOptions -> SoH's */
    int i = (int)lround(v);
    switch (m->xform) {
        case MS_X_NOT:
            return (i != 0) ? 0 : 1;
        case MS_X_BOOL:
            return (i != 0) ? 1 : 0;
        case MS_X_PCT:
            return (double)lround(v * 100.0);
        case MS_X_DAMAGE:
            return (i <= 4) ? (i < 0 ? 0 : i) : (i == 10) ? 8 : 4;
        case MS_X_MIRROR:
            return ((i >= 0) && (i < (int)(sizeof(kMirror) / sizeof(kMirror[0])))) ? kMirror[i] : 0;
        case MS_X_CROUCHSTAB:
            return (i == 0) ? 1 : 0;
        case MS_X_SPEEDMODE:
            return (i == 3) ? 1 : 0;
        default:
            return v;
    }
}

#endif /* MERGESHIP_SETTINGS_SYNC_H */
