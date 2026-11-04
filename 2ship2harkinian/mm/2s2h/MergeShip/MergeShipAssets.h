#pragma once
#define OOT_ASSET(path) "__OTR__oot/" path
#ifdef __cplusplus
extern "C" {
#endif
int MergeShip_IsOcarinaMounted(void);
#ifdef __cplusplus
}
#include <memory>
namespace Ship { class ArchiveManager; }
void MergeShip_MountOcarinaAssets(const std::shared_ptr<Ship::ArchiveManager>& manager);
void MergeShip_VerifyOcarinaAssets();
#endif
