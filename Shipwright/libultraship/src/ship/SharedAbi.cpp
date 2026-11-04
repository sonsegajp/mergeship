#include "ship/SharedAbi.h"

#ifndef LUS_ABI_ID
#define LUS_ABI_ID "unknown"
#endif

const char* LUS_GetAbiId(void) {
    return LUS_ABI_ID;
}
