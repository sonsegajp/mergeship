// MergeShip2 -- ported from 2ship2harkinian/mm/2s2h/resource/type/KeyFrame.cpp.
#include "KeyFrame.h"

namespace SOH {

KeyFrameSkel::~KeyFrameSkel() {
    if (skelType == KeyframeSkelType::Normal) {
        delete[] skelData.limbsStandard;
    } else {
        delete[] skelData.limbsFlex;
    }
}

KeyFrameSkeletonData* KeyFrameSkel::GetPointer() {
    return &skelData;
}

size_t KeyFrameSkel::GetPointerSize() {
    return sizeof(skelData);
}

KeyFrameAnim::~KeyFrameAnim() {
    delete[] animData.bitFlags;
    delete[] animData.keyFrames;
    delete[] animData.kfNums;
    delete[] animData.presetValues;
}

KeyFrameAnimationData* KeyFrameAnim::GetPointer() {
    return &animData;
}

size_t KeyFrameAnim::GetPointerSize() {
    return sizeof(animData);
}

} // namespace SOH
