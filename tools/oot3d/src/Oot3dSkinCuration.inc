// Hand-curated mesh visibility for 3DS characters (Oot3dSkin.cpp, ApplyCuration). Grezzo keeps every alternative of a
// part in one model (open and closed hands, mouths, held props) and its game code picks one; the N64 picks with
// display lists instead. Each entry maps CMB mesh indices (tools/oot3d/mesh_sheets.py renders them) to when they show:
//   { mesh, "", "" }                   always
//   { mesh, "", "-" }                  never
//   { mesh, "<limb's own list>", "<list>" }   while the limb whose own list is the first draws the second
//   { mesh, "@actor", "<list>" }       while the actor draws that list itself during the skeleton draw (a held prop;
//                                      the engine then drops the N64 list's call)
// Meshes a curated skeleton does not name always show. Lists are resource paths without "__OTR__".
// Mesh sheets checked against the decomp's draw code and noclip (src/OcarinaOfTime3D/oot3d_scenes.ts).

// Boss_Ganon / Demo_Gj etc.: closed left fist (open only through the young Ganondorf hand list); right hand closed or
// open (z_boss_ganon.c swaps gGanondorfRightHandOpenDL in). Heads: 9 mouth closed (the N64 opens its mouth with the
// jaw limb), 10 open mouth and 11 its teeth.
{ "objects/object_ganon/gGanondorfSkel",
  { { 5, "objects/object_ganon/gGanondorfLeftHandDL", "objects/object_ganon/gGanondorfLeftHandDL" },
    { 6, "objects/object_ganon/gGanondorfLeftHandDL", "objects/object_gndd/gYoungGanondorfOpenLeftHandDL" },
    { 7, "objects/object_ganon/gGanondorfRightHandClosedDL", "objects/object_ganon/gGanondorfRightHandClosedDL" },
    { 8, "objects/object_ganon/gGanondorfRightHandClosedDL", "objects/object_ganon/gGanondorfRightHandOpenDL" },
    { 10, "", "-" },
    { 11, "", "-" } } },

// En_Zl2 (adult Zelda): the Ocarina of Time only while she holds it (z_en_zl2.c draws gZelda2OcarinaDL).
{ "objects/object_zl2/gZelda2Skel", { { 10, "@actor", "objects/object_zl2/gZelda2OcarinaDL" } } },

// En_Hy (Castle Town / Kakariko townsfolk): one body per object, the head swapped at limb 15 (0-based 14) from
// sHeadInfo by type (z_en_hy.c sModelInfo); each 3DS model holds every head. Matches noclip's per-type visibility.
// object_boj: heads 3+4+5 (BOJ_2: types 9, 10, 16), 6 (BOJ_3: Sakon, type 3, who also wears the cap the actor draws
// on limb 8), 7 (BOJ_4: type 5), 8 (BOJ_5: type 12), 9 (BOJ_6: type 14); 10 is no type's.
{ "objects/object_boj/object_boj_Skel_0000F0",
  { { 3, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_0026F0" },
    { 4, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_0026F0" },
    { 5, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_0026F0" },
    { 6, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_0052E0" },
    { 7, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_005528" },
    { 8, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_005738" },
    { 9, "objects/object_boj/object_boj_DL_0026F0", "objects/object_boj/object_boj_DL_0059B0" },
    { 10, "", "-" },
    { 11, "@actor", "objects/object_boj/object_boj_DL_005BC8" } } },
// object_ahg: heads 3+4 (AHG_7: type 2), 5 (AHG_8: types 4, 17), 6 (AHG_9: types 13, 20); 7 is no type's.
{ "objects/object_ahg/object_ahg_Skel_0000F0",
  { { 3, "objects/object_ahg/object_ahg_DL_0030F0", "objects/object_ahg/object_ahg_DL_0030F0" },
    { 4, "objects/object_ahg/object_ahg_DL_0030F0", "objects/object_ahg/object_ahg_DL_0030F0" },
    { 5, "objects/object_ahg/object_ahg_DL_0030F0", "objects/object_ahg/object_ahg_DL_005508" },
    { 6, "objects/object_ahg/object_ahg_DL_0030F0", "objects/object_ahg/object_ahg_DL_005728" },
    { 7, "", "-" } } },
// object_bji: heads 3+4 (BJI_13: type 7), 5 (BJI_14: types 15, 19).
{ "objects/object_bji/object_bji_Skel_0000F0",
  { { 3, "objects/object_bji/object_bji_DL_002560", "objects/object_bji/object_bji_DL_002560" },
    { 4, "objects/object_bji/object_bji_DL_002560", "objects/object_bji/object_bji_DL_002560" },
    { 5, "objects/object_bji/object_bji_DL_002560", "objects/object_bji/object_bji_DL_003F68" } } },
// object_cne: brown hair 4 (type 8), orange hair 5 (type 11).
{ "objects/object_cne/gCneSkel",
  { { 4, "objects/object_cne/gCneHeadBrownHairDL", "objects/object_cne/gCneHeadBrownHairDL" },
    { 5, "objects/object_cne/gCneHeadBrownHairDL", "objects/object_cne/gCneHeadOrangeHairDL" } } },

// En_Ko (Kokiri): boys always draw gKm1DL at the head limb; 3DS boy head 4 + eyes 3 (noclip hides 2, a bald-hat
// variant no N64 type uses). Girls draw object_kw1_DL_002C10 (head 5 + eyes 6); Fado draws gFaDL from object_fa on the
// girl's body (blonde head 2 + eyes 3 + hair ties 4).
{ "objects/object_km1/gKm1Skel", { { 2, "", "-" } } },
{ "objects/object_kw1/gKw1Skel",
  { { 2, "objects/object_kw1/object_kw1_DL_002C10", "objects/object_fa/gFaDL" },
    { 3, "objects/object_kw1/object_kw1_DL_002C10", "objects/object_fa/gFaDL" },
    { 4, "objects/object_kw1/object_kw1_DL_002C10", "objects/object_fa/gFaDL" },
    { 5, "objects/object_kw1/object_kw1_DL_002C10", "objects/object_kw1/object_kw1_DL_002C10" },
    { 6, "objects/object_kw1/object_kw1_DL_002C10", "objects/object_kw1/object_kw1_DL_002C10" } } },
// En_Sa: the Fairy Ocarina (9) only while the right hand draws gSariaRightHandAndOcarinaDL.
{ "objects/object_sa/gSariaSkel",
  { { 9, "objects/object_sa/gSariaRightHandDL", "objects/object_sa/gSariaRightHandAndOcarinaDL" } } },

// En_Xc (Sheik): hands come in mirrored pairs (shapecmp). Left hand 4; with the harp (limb 12 draws gSheikHarpDL)
// the harp-holding hand 7, the harp 8 and its strings 9. Right hand 6; the Triforce pose (limb 15 draws
// gSheikDL_011620) 5; 3 is no list's.
{ "objects/object_xc/gSheikSkel",
  { { 4, "objects/object_xc/gSheikSkelLimbsLimb_013494DL_00F278", "objects/object_xc/gSheikSkelLimbsLimb_013494DL_00F278" },
    { 7, "objects/object_xc/gSheikSkelLimbsLimb_013494DL_00F278", "objects/object_xc/gSheikHarpDL" },
    { 8, "objects/object_xc/gSheikSkelLimbsLimb_013494DL_00F278", "objects/object_xc/gSheikHarpDL" },
    { 9, "objects/object_xc/gSheikSkelLimbsLimb_013494DL_00F278", "objects/object_xc/gSheikHarpDL" },
    { 6, "objects/object_xc/gSheikSkelLimbsLimb_0134B8DL_00EC40", "objects/object_xc/gSheikSkelLimbsLimb_0134B8DL_00EC40" },
    { 5, "objects/object_xc/gSheikSkelLimbsLimb_0134B8DL_00EC40", "objects/object_xc/gSheikDL_011620" },
    { 3, "", "-" } } },
// Demo_Im / En_Viewer (Impa): fists 3 (left) and 5 (right), the pointing hand 2 (Demo_Im's gImpaHandPointingDL); the
// flat pair 4/6 is no list's. Unmasked head 8 + eyes 7, masked head 9 + eyes 10 (En_Viewer's gImpaHeadMaskedDL).
{ "objects/object_im/gImpaSkel",
  { { 4, "", "-" },
    { 6, "", "-" },
    { 5, "objects/object_im/gImpaRightHandDL", "objects/object_im/gImpaRightHandDL" },
    { 2, "objects/object_im/gImpaRightHandDL", "objects/object_im/gImpaHandPointingDL" },
    { 7, "objects/object_im/gImpaHeadUnmaskedDL", "objects/object_im/gImpaHeadUnmaskedDL" },
    { 8, "objects/object_im/gImpaHeadUnmaskedDL", "objects/object_im/gImpaHeadUnmaskedDL" },
    { 9, "objects/object_im/gImpaHeadUnmaskedDL", "objects/object_im/gImpaHeadMaskedDL" },
    { 10, "objects/object_im/gImpaHeadUnmaskedDL", "objects/object_im/gImpaHeadMaskedDL" } } },

// En_Ma2 (adult Malon): the basket only while she holds it (gMalonAdultBasketDL from the left hand).
{ "objects/object_ma2/gMalonAdultSkel", { { 2, "@actor", "objects/object_ma2/gMalonAdultBasketDL" } } },
// En_In (Ingo): the child-era outfit 0 and the adult-era one 1 (the actor swaps every limb to sAdultEraDLs; the torso
// says which); basket 2 and pitchfork 3+4 only while he carries them.
{ "objects/object_in/gIngoSkel",
  { { 0, "objects/object_in/gIngoChildEraTorsoDL", "objects/object_in/gIngoChildEraTorsoDL" },
    { 1, "objects/object_in/gIngoChildEraTorsoDL", "objects/object_in/gIngoAdultEraTorsoDL" },
    { 2, "@actor", "objects/object_in/gIngoChildEraBasketDL" },
    { 3, "@actor", "objects/object_in/gIngoChildEraPitchForkDL" },
    { 4, "@actor", "objects/object_in/gIngoChildEraPitchForkDL" } } },

// En_Nb (Nabooru): mouth closed 6 + eyes 5 (the head limb's own list), open 8 + eyes 7 (gNabooruHeadMouthOpenDL).
{ "objects/object_nb/gNabooruSkel",
  { { 5, "objects/object_nb/gNabooruHeadMouthClosedDL", "objects/object_nb/gNabooruHeadMouthClosedDL" },
    { 6, "objects/object_nb/gNabooruHeadMouthClosedDL", "objects/object_nb/gNabooruHeadMouthClosedDL" },
    { 7, "objects/object_nb/gNabooruHeadMouthClosedDL", "objects/object_nb/gNabooruHeadMouthOpenDL" },
    { 8, "objects/object_nb/gNabooruHeadMouthClosedDL", "objects/object_nb/gNabooruHeadMouthOpenDL" } } },
// Child Zelda (zl4: En_Zl4 / En_Viewer; zl1 shares the 3DS model): the Ocarina of Time 9 only while the actor draws it.
{ "objects/object_zl4/gChildZeldaSkel", { { 9, "@actor", "objects/object_zl4/gChildZeldaOcarinaOfTimeDL" } } },
{ "objects/object_zl1/gChildZelda1Skel", { { 9, "@actor", "objects/object_zl1/gChildZelda1OcarinaDL" } } },

// Young Ganondorf (gndd; En_Viewer opens his left hand): fist 5, open hand 4 (shapecmp 0.07); 10 the open mouth's teeth.
{ "objects/object_gndd/gYoungGanondorfSkel",
  { { 5, "objects/object_gndd/gYoungGanondorfLeftHandDL", "objects/object_gndd/gYoungGanondorfLeftHandDL" },
    { 4, "objects/object_gndd/gYoungGanondorfLeftHandDL", "objects/object_gndd/gYoungGanondorfOpenLeftHandDL" },
    { 10, "", "-" } } },
// En_Torch2 (Dark Link): 4 and 5 are Grezzo's placeholder-textured ("gen") helper meshes.
{ "objects/object_torch2/gDarkLinkSkel", { { 4, "", "-" }, { 5, "", "-" } } },

// Boss_Tw sisters: Grezzo's mesh ids group the alternatives. Battle head (bald, with the ice/fire hair) while limb 21
// draws its own ...HeadInvisibleHairDL (the N64 hair is an effect then); the normal head when the actor swaps in
// ...HeadDL. The scepter (stick + ice/fire head) while limb 14 draws its own ...BroomInvisibleHeadDL; the plain
// broom when the actor swaps in ...BroomDL. The 3DS ice/fire hair and scepter heads are static meshes; the N64's
// animated translucent effects (IceHairDL, IceBroomHeadDL, ...) stay instead.
{ "objects/object_tw/gTwinrovaKotakeSkel",
  { { 3, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL" },
    { 4, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL" },
    { 5, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL" },
    { 6, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL" },
    { 7, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL" },
    { 15, "", "-" },
    { 16, "", "-" },
    { 8, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadDL" },
    { 9, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadDL" },
    { 10, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadDL" },
    { 11, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadDL" },
    { 12, "objects/object_tw/gTwinrovaKotakeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKotakeHeadDL" },
    { 1, "objects/object_tw/gTwinrovaKotakeBroomInvisibleHeadDL", "objects/object_tw/gTwinrovaKotakeBroomInvisibleHeadDL" },
    { 13, "", "-" },
    { 14, "", "-" },
    { 2, "objects/object_tw/gTwinrovaKotakeBroomInvisibleHeadDL", "objects/object_tw/gTwinrovaKotakeBroomDL" } } },
{ "objects/object_tw/gTwinrovaKoumeSkel",
  { { 3, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL" },
    { 4, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL" },
    { 5, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL" },
    { 6, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL" },
    { 7, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL" },
    { 14, "", "-" },
    { 8, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadDL" },
    { 9, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadDL" },
    { 10, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadDL" },
    { 11, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadDL" },
    { 12, "objects/object_tw/gTwinrovaKoumeHeadInvisibleHairDL", "objects/object_tw/gTwinrovaKoumeHeadDL" },
    { 1, "objects/object_tw/gTwinrovaKoumeBroomInvisibleHeadDL", "objects/object_tw/gTwinrovaKoumeBroomInvisibleHeadDL" },
    { 13, "", "-" },
    { 2, "objects/object_tw/gTwinrovaKoumeBroomInvisibleHeadDL", "objects/object_tw/gTwinrovaKoumeBroomDL" } } },

// En_Po_Sisters: limb 9 draws the sister's body and limb 10 her face (D_80ADD7C8/D_80ADD7D8 by sister); the 3DS
// model holds all four, grouped by texture: Meg _00 (body 6 + crown 7, face 0 + 11), Joelle _01 (8, 1), Beth _02
// (9, 2), Amy _03 (10, 3). The lantern 4 is the N64 gPoSistersTorchDL drawn apart from the skeleton (it can drop).
{ "objects/object_po_sisters/gPoeSistersSkel",
  { { 6, "objects/object_po_sisters/gPoeSistersMegBodyDL", "objects/object_po_sisters/gPoeSistersMegBodyDL" },
    { 7, "objects/object_po_sisters/gPoeSistersMegBodyDL", "objects/object_po_sisters/gPoeSistersMegBodyDL" },
    { 8, "objects/object_po_sisters/gPoeSistersMegBodyDL", "objects/object_po_sisters/gPoeSistersJoelleBodyDL" },
    { 9, "objects/object_po_sisters/gPoeSistersMegBodyDL", "objects/object_po_sisters/gPoeSistersBethBodyDL" },
    { 10, "objects/object_po_sisters/gPoeSistersMegBodyDL", "objects/object_po_sisters/gPoeSistersAmyBodyDL" },
    { 0, "objects/object_po_sisters/gPoeSistersMegFaceDL", "objects/object_po_sisters/gPoeSistersMegFaceDL" },
    { 11, "objects/object_po_sisters/gPoeSistersMegFaceDL", "objects/object_po_sisters/gPoeSistersMegFaceDL" },
    { 1, "objects/object_po_sisters/gPoeSistersMegFaceDL", "objects/object_po_sisters/gPoeSistersJoelleFaceDL" },
    { 2, "objects/object_po_sisters/gPoeSistersMegFaceDL", "objects/object_po_sisters/gPoeSistersBethFaceDL" },
    { 3, "objects/object_po_sisters/gPoeSistersMegFaceDL", "objects/object_po_sisters/gPoSistersAmyFaceDL" },
    { 4, "", "-" } } },
// En_Poh composers: Sharp (own head: swirl hat 7 + eyes 8, robe 0 + emblem 1, texture _00) and Flat
// (gPoeComposerFlatHeadDL: moon hat 9 + eyes 10, robe 2 + emblem 3, texture _01); the id-2 set 4-6 is neither's.
{ "objects/object_po_composer/gPoeComposerSkel",
  { { 7, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100" },
    { 8, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100" },
    { 0, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100" },
    { 1, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100" },
    { 9, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerFlatHeadDL" },
    { 10, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerFlatHeadDL" },
    { 2, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerFlatHeadDL" },
    { 3, "objects/object_po_composer/gPoeComposerSkelLimbsLimb_006F4CDL_004100", "objects/object_po_composer/gPoeComposerFlatHeadDL" },
    { 4, "", "-" },
    { 5, "", "-" },
    { 6, "", "-" } } },

// En_Po_Field: the Big Poe swaps limbs 1/8/9 to gBigPoeFaceDL/CloakDL/BodyDL. Texture _00 is the normal Poe (cloak 0,
// face 4), _01 the Big Poe (cloak 1, face 5). The lantern limb is nulled and gPoeFieldLanternDL drawn on its own, so
// the 3DS lanterns 6-9 stay out.
{ "objects/object_po_field/gPoeFieldSkel",
  { { 0, "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069F4DL_005328", "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069F4DL_005328" },
    { 1, "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069F4DL_005328", "objects/object_po_field/gBigPoeCloakDL" },
    { 4, "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069A0DL_004F60", "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069A0DL_004F60" },
    { 5, "objects/object_po_field/gPoeFieldSkelLimbsLimb_0069A0DL_004F60", "objects/object_po_field/gBigPoeFaceDL" },
    { 6, "", "-" }, { 7, "", "-" }, { 8, "", "-" }, { 9, "", "-" } } },

// Dampe (En_Tk alive, En_Po_Relay ghost, one skeleton): the shovel 6+7 (En_Tk draws gDampeShovelDL from the neck),
// the lantern 2-4 and the halo 0 (only the ghost draws gDampeLanternDL / gDampeHaloDL).
{ "objects/object_tk/gDampeSkel",
  { { 0, "@actor", "objects/object_tk/gDampeHaloDL" },
    { 2, "@actor", "objects/object_tk/gDampeLanternDL" },
    { 3, "@actor", "objects/object_tk/gDampeLanternDL" },
    { 4, "@actor", "objects/object_tk/gDampeLanternDL" },
    { 6, "@actor", "objects/object_tk/gDampeShovelDL" },
    { 7, "@actor", "objects/object_tk/gDampeShovelDL" } } },
// Fishing pond owner: his cap 1 or his hair 3, as the head's post-draw picks (gFishingOwnerHatDL / HairDL).
{ "objects/object_fish/gFishingOwnerSkel",
  { { 1, "@actor", "objects/object_fish/gFishingOwnerHatDL" },
    { 3, "@actor", "objects/object_fish/gFishingOwnerHairDL" } } },

// Boss_Ganondrof (Phantom Ganon): masked head (mesh ids 2-4: glowing eyes 2, skull helmet 3, hair 4) while the head
// limb draws its own list; the unmasked face (id 5: jewel 5, face 6, eyes 7) when the intro swaps in
// gPhantomGanonFaceDL. Id 6 (8-10) is a second face no N64 list uses.
{ "objects/object_gnd/gPhantomGanonSkel",
  { { 2, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8" },
    { 3, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8" },
    { 4, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8" },
    { 5, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonFaceDL" },
    { 6, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonFaceDL" },
    { 7, "objects/object_gnd/gPhantomGanonSkelLimbsLimb_00C628DL_0080F8", "objects/object_gnd/gPhantomGanonFaceDL" },
    { 8, "", "-" }, { 9, "", "-" }, { 10, "", "-" } } },

// En_Zf (Lizalfos / Dinolfos): sword in hand (sword limb's own list) or sheathed (the sword limb draws gZfEmptyHandDL
// and the scabbard limb gZfSheathedSwordDL). shapecmp: empty hands 5 / 4 (0.06 / 0.04); mesh ids pair each blade or
// sheathed sword with its guard.
{ "objects/object_zf/gZfLizalfosSkel",
  { { 4, "objects/object_zf/gZfLizalfosSwordDL", "objects/object_zf/gZfLizalfosSwordDL" },
    { 9, "objects/object_zf/gZfLizalfosSwordDL", "objects/object_zf/gZfLizalfosSwordDL" },
    { 10, "objects/object_zf/gZfLizalfosSwordDL", "objects/object_zf/gZfLizalfosSwordDL" },
    { 5, "objects/object_zf/gZfLizalfosSwordDL", "objects/object_zf/gZfEmptyHandDL" },
    { 6, "objects/object_zf/gZfLizalfosScabbardDL", "objects/object_zf/gZfLizalfosScabbardDL" },
    { 7, "objects/object_zf/gZfLizalfosScabbardDL", "objects/object_zf/gZfSheathedSwordDL" },
    { 8, "objects/object_zf/gZfLizalfosScabbardDL", "objects/object_zf/gZfSheathedSwordDL" } } },
{ "objects/object_zf/gZfDinolfosSkel",
  { { 3, "objects/object_zf/gZfDinolfosSwordDL", "objects/object_zf/gZfDinolfosSwordDL" },
    { 8, "objects/object_zf/gZfDinolfosSwordDL", "objects/object_zf/gZfDinolfosSwordDL" },
    { 9, "objects/object_zf/gZfDinolfosSwordDL", "objects/object_zf/gZfDinolfosSwordDL" },
    { 4, "objects/object_zf/gZfDinolfosSwordDL", "objects/object_zf/gZfEmptyHandDL" },
    { 5, "objects/object_zf/gZfDinolfosScabbardDL", "objects/object_zf/gZfDinolfosScabbardDL" },
    { 6, "objects/object_zf/gZfDinolfosScabbardDL", "objects/object_zf/gZfSheathedSwordDL" },
    { 7, "objects/object_zf/gZfDinolfosScabbardDL", "objects/object_zf/gZfSheathedSwordDL" } } },

// En_Ssh (cursed House of Skulltula family): the father swaps limbs 4/5/8 (object_ssh_DL_0046C0/004080/004DE8);
// shapecmp pairs his pieces 0, 8+9 (head) and 4 (0.01) against the others' 1, 6+7 and 3.
{ "objects/object_ssh/object_ssh_Skel_0052E0",
  { { 1, "objects/object_ssh/object_ssh_DL_004510", "objects/object_ssh/object_ssh_DL_004510" },
    { 0, "objects/object_ssh/object_ssh_DL_004510", "objects/object_ssh/object_ssh_DL_0046C0" },
    { 6, "objects/object_ssh/object_ssh_DL_003CD0", "objects/object_ssh/object_ssh_DL_003CD0" },
    { 7, "objects/object_ssh/object_ssh_DL_003CD0", "objects/object_ssh/object_ssh_DL_003CD0" },
    { 8, "objects/object_ssh/object_ssh_DL_003CD0", "objects/object_ssh/object_ssh_DL_004080" },
    { 9, "objects/object_ssh/object_ssh_DL_003CD0", "objects/object_ssh/object_ssh_DL_004080" },
    { 3, "objects/object_ssh/object_ssh_DL_004D28", "objects/object_ssh/object_ssh_DL_004D28" },
    { 4, "objects/object_ssh/object_ssh_DL_004D28", "objects/object_ssh/object_ssh_DL_004DE8" } } },

// En_Ge1 (white-clad Gerudo): the head's post-draw adds sHairstyleDLists[hairstyle]; bob 2, straight fringe 1,
// spiky 3 (by eye: the shapes are too alike for shapecmp).
{ "objects/object_ge1/gGerudoWhiteSkel",
  { { 2, "@actor", "objects/object_ge1/gGerudoWhiteHairstyleBobDL" },
    { 1, "@actor", "objects/object_ge1/gGerudoWhiteHairstyleStraightFringeDL" },
    { 3, "@actor", "objects/object_ge1/gGerudoWhiteHairstyleSpikyDL" } } },

// En_Daiku / En_Daiku_Kakariko (carpenters): the head's post-draw adds hairDLists[params & 3]; shapecmp pairs the
// four 3DS hairstyles one to one (0.06 / 0.15 / 0.12 / 0.34).
{ "objects/object_daiku/object_daiku_Skel_007958",
  { { 0, "@actor", "objects/object_daiku/object_daiku_DL_005BD0" },
    { 1, "@actor", "objects/object_daiku/object_daiku_DL_005AC0" },
    { 2, "@actor", "objects/object_daiku/object_daiku_DL_005990" },
    { 3, "@actor", "objects/object_daiku/object_daiku_DL_005880" } } },

// gObjectOssanSkel: the Bazaar shopkeeper (En_Ossan, the limb's own head: bearded 3 + eyes 4) and the shooting
// gallery man (En_Syateki_Man swaps in gObjectOssanEnSyatekiManDL_007E28: head 1 + eyes 2); shapecmp 0.19 / 0.14.
{ "objects/object_ossan/gObjectOssanSkel",
  { { 3, "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428", "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428" },
    { 4, "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428", "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428" },
    { 1, "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428", "objects/object_ossan/gObjectOssanEnSyatekiManDL_007E28" },
    { 2, "objects/object_ossan/gObjectOssanSkelLimbsLimb_009B0CDL_007428", "objects/object_ossan/gObjectOssanEnSyatekiManDL_007E28" } } },

// En_Goma (Gohma larva): the egg 4 is gObjectGolEggDL, drawn instead of the skeleton while unhatched.
{ "objects/object_gol/gObjectGolSkel", { { 4, "", "-" } } },

// En_Skj (Skull Kid): the Skull Mask 3 only once he has bought it (limb 11's post-draw adds gSkullKidSkullMaskDL).
{ "objects/object_skj/gSkullKidSkel", { { 3, "@actor", "objects/object_skj/gSkullKidSkullMaskDL" } } },

// En_Box (treasure chests): the post-draw adds boxBodyDL on limb 1 and boxLidDL on limb 3. Wooden chest 1 + lid 3,
// boss chest 0 + lid 2 (noclip shows the same pairs). SoH's chests-match-contents lists have no 3DS form: those
// chests keep the N64 lists.
{ "objects/object_box/gTreasureChestSkel",
  { { 1, "@actor", "objects/object_box/gTreasureChestChestFrontDL" },
    { 3, "@actor", "objects/object_box/gTreasureChestChestSideAndLidDL" },
    { 0, "@actor", "objects/object_box/gTreasureChestBossKeyChestFrontDL" },
    { 2, "@actor", "objects/object_box/gTreasureChestBossKeyChestSideAndTopDL" } } },

// En_Heishi (castle soldiers): the spear is one piece (shaft 3 + tip 4, one mesh id); the geometric pass gave the shaft
// to the king's guard list.
{ "objects/object_sd/gEnHeishiSkel", { { 3, "", "" }, { 4, "", "" } } },
