#pragma once

/**
 * @file SharedAbi.h
 * @brief Identity of a shared (LUS_SHARED) libultraship build.
 *
 * When several game modules share one libultraship DLL inside a host process, each of them must
 * have been compiled against the same libultraship commit and ABI-relevant options as the DLL the
 * host actually loaded. The DLL reports its identity here; a module reports the identity it was
 * built against from its own LUS_ABI_ID define (set from the CMake cache entry of the same name).
 */

#include "ship/Api.h"

#ifdef LUS_SHARED
/** @brief Returns "<libultraship commit>-<option hash>" of the loaded libultraship DLL. */
API_EXPORT const char* LUS_GetAbiId(void);
#endif
