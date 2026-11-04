#pragma once

// MergeShip2 — Majora's Mask asset namespace.
//
// Ocarina of Time and Majora's Mask name 2562 resources identically (object_link_child,
// object_link_boy, gameplay_keep, the audio sample/soundfont tables, ...). libultraship's
// resource namespace is flat and last-mounted-wins, so mounting a stock mm.o2r beside
// oot.o2r silently replaces Hyrule's assets with Termina's.
//
// Every MM resource is therefore exported under this namespace. The prefix is applied at
// the points that name a resource, which are the same points that emit references embedded
// inside exported data, so archive entries and internal references stay consistent.
//
// SoH addresses MM assets as "__OTR__mm/objects/object_link_goron/gLinkGoronSkel".
// Keep this value in sync with MERGESHIP_MM_NAMESPACE on the SoH side.
#define MERGESHIP_MM_NAMESPACE "mm"

// Prefix form, for the few paths that are spelled out literally instead of being built by
// OTRExporter_DisplayList::GetPrefix. Empty for non-MM builds so the OoT exporter is unchanged.
#ifdef GAME_MM
#define MERGESHIP_MM_PREFIX MERGESHIP_MM_NAMESPACE "/"
#else
#define MERGESHIP_MM_PREFIX ""
#endif
