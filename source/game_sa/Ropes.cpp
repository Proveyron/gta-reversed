/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"

void CRopes::InjectHooks() {
    RH_ScopedClass(CRopes);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x555DC0);
    RH_ScopedInstall(Shutdown, 0x556B10);
    RH_ScopedInstall(Update, 0x558D70);
    RH_ScopedInstall(Render, 0x556AE0);
    RH_ScopedInstall(RegisterRope, 0x556B40);
    RH_ScopedInstall(FindPickupHeight, 0x556760);
    RH_ScopedInstall(FindRope, 0x556000);
    RH_ScopedInstall(FindCoorsAlongRope, 0x555E40);
    RH_ScopedInstall(CreateRopeForSwatPed, 0x558D10);
    RH_ScopedInstall(IsCarriedByRope, 0x555F80);
    RH_ScopedInstall(SetSpeedOfTopNode, 0x555DF0);
}

// 0x555DC0
void CRopes::Init() {
    for (auto& rope : aRopes) {
        rope.m_nType = eRopeType::NONE;
    }
    PlayerControlsCrane = eControlledCrane::NONE;
}

// 0x556B10
void CRopes::Shutdown() {
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE)
            continue;

        rope.Remove();
    }
}

// 0x558D70
void CRopes::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Update();
    }
}

// 0x556AE0
void CRopes::Render() {
    ZoneScoped;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Render();
    }
}

// Must be used in loop to make attached to holder
// 0x556B40
bool CRopes::RegisterRope(uint32 ropeID, uint32 ropeType, CVector startPos, bool bExpires, uint8 segmentCount, uint8 flags, CPhysical* holder, uint32 timeExpire) {
    int32 existingIdx = FindRope(ropeID);
    if (existingIdx != -1) {
        auto& rope = aRopes[existingIdx];
        rope.m_aSegments[0] = startPos;
        rope.m_aSpeed[0] = CVector(0.0f, 0.0f, 0.0f);
        rope.m_nFlags2 |= 1;
        rope.m_nSegments = segmentCount;
        for (int32 seg = 0; seg <= segmentCount; seg++) {
            rope.m_aSegments[seg] = startPos;
            rope.m_aSpeed[seg] = CVector(0.0f, 0.0f, 0.0f);
        }
        rope.CreateHookObjectForRope();
        return true;
    }

    int32 freeIdx = -1;
    for (int32 i = 0; i < MAX_NUM_ROPES; i++) {
        if (aRopes[i].m_nType == eRopeType::NONE) {
            freeIdx = i;
            break;
        }
    }
    if (freeIdx == -1) {
        return false;
    }

    auto& rope = aRopes[freeIdx];
    rope.m_nId = ropeID;
    rope.m_aSegments[0] = startPos;
    rope.m_aSpeed[0] = CVector(0.0f, 0.0f, 0.0f);
    rope.m_nFlags2 = (rope.m_nFlags2 & 0xF9) | ((flags & 1) << 2) | 1;
    rope.m_nSegments = segmentCount;
    rope.m_fGroundZ = 0.0f;
    rope.m_pAttachedEntity = nullptr;
    rope.m_pRopeAttachObject = nullptr;

    if (holder && holder->IsVehicle()) {
        rope.m_fSegmentLength = 0.9f;
    } else {
        rope.m_fSegmentLength = 0.5f;
    }

    rope.m_nFlags1 = 0;
    rope.m_pRopeHolder = holder;
    rope.m_nType = static_cast<eRopeType>(ropeType);
    if (holder) {
        holder->RegisterReference(reinterpret_cast<CEntity**>(&rope.m_pRopeHolder));
    }

    if (bExpires) {
        rope.m_nTime = CTimer::m_snTimeInMilliseconds + timeExpire;
    } else {
        rope.m_nTime = 0;
    }

    switch (rope.m_nType) {
    case eRopeType::MAGNET:
        rope.m_fMass = 10.0f;
        rope.m_fTotalLength = 10.0f / 31.0f;
        break;
    case eRopeType::CRANE_MAGNO:
        rope.m_fMass = 50.0f;
        rope.m_fTotalLength = 50.0f / 31.0f;
        break;
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY:
        rope.m_fMass = 68.0f;
        rope.m_fTotalLength = 68.0f / 31.0f;
        break;
    default:
        rope.m_fMass = 20.0f;
        rope.m_fTotalLength = 20.0f / 31.0f;
        break;
    }

    if (rope.m_nType >= eRopeType::CRANE_MAGNO && rope.m_nType <= eRopeType::CRANE_TROLLEY) {
        for (auto i = 1u; i < NUM_ROPE_SEGMENTS; i++) {
            rope.m_aSegments[i] = rope.m_aSegments[i - 1] - CVector(0.0f, 0.0f, rope.m_fTotalLength);
            rope.m_aSpeed[i] = CVector(0.0f, 0.0f, 0.0f);
        }
    } else {
        for (auto i = 1u; i < NUM_ROPE_SEGMENTS; i++) {
            if (i % 2 == 1) {
                rope.m_aSegments[i] = rope.m_aSegments[i - 1] + CVector(rope.m_fTotalLength, 0.0f, 0.0f);
            } else {
                rope.m_aSegments[i] = rope.m_aSegments[i - 1] - CVector(rope.m_fTotalLength, 0.0f, 0.0f);
            }
            rope.m_aSpeed[i] = CVector(0.0f, 0.0f, 0.0f);
        }
    }

    rope.CreateHookObjectForRope();
    return true;
}

// 0x556760
float CRopes::FindPickupHeight(CEntity* entity) {
    return CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMax.z;
}

// Returns id to array
// 0x556000
int32 CRopes::FindRope(uint32 id) {
    for (auto ropeId = 0; ropeId < MAX_NUM_ROPES; ropeId++) {
        if (aRopes[ropeId].m_nType != eRopeType::NONE && aRopes[ropeId].m_nId == id)
            return ropeId;
    }
    return -1;
}

// a4 always nullptr
// 0x555E40
bool CRopes::FindCoorsAlongRope(uint32 ropeId, float fDistAlongRope, CVector* outPosn, CVector* outSpeed) {
    int32 ropeIdx = FindRope(ropeId);
    if (ropeIdx == -1)
        return false;

    float clampedDist = std::clamp(fDistAlongRope, 0.0f, 0.999f);
    float scaled = clampedDist * 31.0f;
    int32 segIdx = static_cast<int32>(scaled);
    float frac = scaled - static_cast<float>(segIdx);

    auto& rope = aRopes[ropeIdx];
    if (outPosn) {
        *outPosn = lerp(rope.m_aSegments[segIdx], rope.m_aSegments[segIdx + 1], frac);
    }

    if (outSpeed) {
        *outSpeed = rope.m_aSpeed[segIdx + 1];
    }

    return true;
}

// 0x558D10
int32 CRopes::CreateRopeForSwatPed(const CVector& startPos) {
    int32 newRopeId = m_nRopeIdCreationCounter + 100;
    if (RegisterRope(newRopeId, static_cast<uint32>(eRopeType::SWAT), startPos, true, 0, 0, nullptr, 4000)) {
        return -1;
    }

    m_nRopeIdCreationCounter += 1;
    return newRopeId;
}

// 0x555F80
bool CRopes::IsCarriedByRope(CPhysical* entity) {
    if (!entity)
        return false;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_pRopeAttachObject == entity)
            return true;
    }
    return false;
}

// 0x555DF0
void CRopes::SetSpeedOfTopNode(uint32 ropeId, CVector dirSpeed) {
    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_nId == ropeId) {
            rope.m_aSpeed[0] = dirSpeed;
            return;
        }
    }
}
