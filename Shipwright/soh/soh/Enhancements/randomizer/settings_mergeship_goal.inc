// MergeShip2: the unified randomizer's goal (soh/MergeShip/MergeShipGoal.h). Included inside Settings::CreateOptions
// (settings.cpp) after settings_termina.inc. Every key here follows RSK_MERGESHIP_TERMINA, so each is a Termina setting:
// hashed and written to the spoiler only in a unified seed (MergeShip_Termina_SettingCounts), never touched by
// "Randomize All". Names and descriptions: MergeShip_Goal_SettingText.
OPT_U8(RSK_MS_GOAL, { "Ganon", "Majora", "Ganon and Majora", "Triforce Hunt" }, OptionCategory::Setting, CVAR_RANDOMIZER_SETTING("MergeShip.Goal"), WIDGET_CVAR_COMBOBOX, RO_MS_GOAL_BOTH);
OPT_CALLBACK(RSK_MS_GOAL, {
    if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("MergeShip.Goal"), RO_MS_GOAL_BOTH) == RO_MS_GOAL_TRIFORCE) {
        mOptions[RSK_MS_TRIFORCE_PIECES_TOTAL].Unhide();
        mOptions[RSK_MS_TRIFORCE_PIECES_REQUIRED].Unhide();
    } else {
        mOptions[RSK_MS_TRIFORCE_PIECES_TOTAL].Hide();
        mOptions[RSK_MS_TRIFORCE_PIECES_REQUIRED].Hide();
    }
});
// Index + 1 pieces (1-100): 30 in the pool, 20 required by default.
OPT_U8(RSK_MS_TRIFORCE_PIECES_TOTAL, {NumOpts(1, 100)}, OptionCategory::Setting, CVAR_RANDOMIZER_SETTING("MergeShip.TriforcePiecesTotal"), WIDGET_CVAR_SLIDER_INT, 29, true);
OPT_U8(RSK_MS_TRIFORCE_PIECES_REQUIRED, {NumOpts(1, 100)}, OptionCategory::Setting, CVAR_RANDOMIZER_SETTING("MergeShip.TriforcePiecesRequired"), WIDGET_CVAR_SLIDER_INT, 19, true);
OPT_BOOL(RSK_MS_REMAINS_AS_MEDALLIONS, CVAR_RANDOMIZER_SETTING("MergeShip.RemainsAsMedallions"));
OPT_U8(RSK_MS_MAJORA_FORM_MASKS, {NumOpts(0, 4)}, OptionCategory::Setting, CVAR_RANDOMIZER_SETTING("MergeShip.MajoraFormMasks"), WIDGET_CVAR_SLIDER_INT, 0);
