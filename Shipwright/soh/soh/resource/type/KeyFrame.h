#pragma once

// MergeShip2 -- Majora's Mask keyframe skeletons and animations (c_keyframe.c data) as stored in mm.o2r.
// Ported from 2ship2harkinian/mm/2s2h/resource/type/KeyFrame.h. The layouts match MM's KeyFrameFlexSkeleton,
// KeyFrameSkeleton and KeyFrameAnimation (soh/MergeShip/MergeShipKeyframe.h), which the game side casts
// GetPointer() to.

#include <ship/resource/Resource.h>
#include <libultraship/libultra/types.h>

namespace SOH {

// 2S2H names this SOH::Vec3s; renamed so that no translation unit that also sees the game's global Vec3s
// can hit an ambiguity. Same layout.
typedef struct KeyFrameVec3s {
    s16 x;
    s16 y;
    s16 z;
} KeyFrameVec3s;

enum class KeyframeSkelType {
    Normal,
    Flex,
    Error,
};

// MM KeyFrameStandardLimb
typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ u8 numChildren;
    /* 0x05 */ u8 flags;
    /* 0x06 */ KeyFrameVec3s translation; // FlexLimbs have translation data in their animations instead
} KeyFrameStandardLimb;                   // size = 0xC

// MM KeyFrameFlexLimb
typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ u8 numChildren;
    /* 0x05 */ u8 flags;
    /* 0x06 */ u8 callbackIndex; // transform callback index
} KeyFrameFlexLimb;              // size = 0x8

// MM KeyFrameSkeleton / KeyFrameFlexSkeleton
typedef struct {
    /* 0x00 */ u8 limbCount;
    /* 0x01 */ u8 dListCount; // number of limbs with a display list, how many matrices to allocate for drawing
    /* 0x04 */ union {
        KeyFrameStandardLimb* limbsStandard;
        KeyFrameFlexLimb* limbsFlex;
    };
} KeyFrameSkeletonData; // Size = 0x8

// MM KeyFrame
typedef struct {
    /* 0x00 */ s16 frame;
    /* 0x02 */ s16 value;
    /* 0x04 */ s16 velocity;
} KeyFrame; // Size = 0x6

// MM KeyFrameAnimation
typedef struct {
    union {
        /* 0x00 */ u8* bitFlags;      // standard: whether to interpolate keyframes or pull from preset values
        /* 0x00 */ u16* bitFlagsFlex; // flex: whether to interpolate keyframes or pull from preset values
    };
    /* 0x04 */ KeyFrame* keyFrames; // keyframes array
    /* 0x08 */ s16* kfNums;         // number of keyframes for each limb
    /* 0x0C */ s16* presetValues;   // preset rotation (standard) or scale/rotation/translation (flex) values
    /* 0x10 */ char unk_10[0x2];
    /* 0x12 */ s16 duration;
} KeyFrameAnimationData; // Size = 0x14

class KeyFrameSkel : public Ship::Resource<KeyFrameSkeletonData> {
  public:
    using Resource::Resource;

    KeyFrameSkel() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }
    ~KeyFrameSkel();

    KeyFrameSkeletonData* GetPointer();
    size_t GetPointerSize();

    KeyFrameSkeletonData skelData;
    // Which union member skelData holds, so the destructor frees the array it allocated (2S2H always frees
    // limbsFlex).
    KeyframeSkelType skelType = KeyframeSkelType::Flex;
};

class KeyFrameAnim : public Ship::Resource<KeyFrameAnimationData> {
  public:
    using Resource::Resource;

    KeyFrameAnim() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }
    ~KeyFrameAnim();

    KeyFrameAnimationData* GetPointer();
    size_t GetPointerSize();

    KeyFrameAnimationData animData;
};

} // namespace SOH
