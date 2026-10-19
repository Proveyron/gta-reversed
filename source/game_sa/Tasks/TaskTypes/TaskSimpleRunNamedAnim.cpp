#include "StdInc.h"

#include "TaskSimpleRunNamedAnim.h"

void CTaskSimpleRunNamedAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleRunNamedAnim, 0x86D54C, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "Default", 0x6674B0, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)());
    RH_ScopedOverloadedInstall(Constructor, "Anim", 0x61A990, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)(char const*, char const*, int32, float, int32, bool, bool, bool, bool));
    RH_ScopedInstall(Destructor, 0x61BF10);

    //RH_ScopedInstall(FinishRunAnimMovePedCB, 0x61AAA0, { .Reversed = false });
    //RH_ScopedInstall(StartAnim, 0x61BB10, { .Reversed = false });
    RH_ScopedVMTInstall(Clone, 0x61B770);
    RH_ScopedVMTInstall(GetTaskType, 0x61AA90);
    RH_ScopedVMTInstall(ProcessPed, 0x61BF20);
    RH_ScopedInstall(OffsetPedPosition, 0x61AB00);
}

// 0x6674B0
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim() :
    CTaskSimpleAnim{ false }
{
    // Rest done in header
}

// 0x61A990
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim(
    const char* animName,
    const char* animGroupName,
    uint32 animFlags,
    float blendDelta,
    uint32 endTime,
    bool bDontInterrupt,
    bool bRunInSequence,
    bool bOffsetPed,
    bool bHoldLastFrame
) :
    CTaskSimpleAnim(bHoldLastFrame),
    m_Time{ endTime },
    m_animFlags{ animFlags }
{
    m_bDontInterrupt = bDontInterrupt;
    m_bRunInSequence = bRunInSequence;
    m_bOffsetAtEnd = bOffsetPed;
    m_fBlendDelta = blendDelta;
    strcpy_s(m_animName, animName);
    strcpy_s(m_animGroupName, animGroupName);
    if (const auto block = CAnimManager::GetAnimationBlock(m_animGroupName)) {
        m_pAnimHierarchy = CAnimManager::GetAnimation(m_animName, block);
    }
}

// 0x61BF20
bool CTaskSimpleRunNamedAnim::ProcessPed(CPed* ped) {
    if (m_bOffsetAvailable) {
        OffsetPedPosition(ped);
    }

    if (m_bIsFinished) {
        if (!m_bOffsetAtEnd) {
            auto anim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), m_pAnimHierarchy);
            CVector prevPos{};
            ped->GetTransformedBonePosition(prevPos, BONE_ROOT, false);
            if (m_fBlendDelta <= 0.0f && anim) {
                if ((ped->GetPosition() - prevPos).Magnitude() <= 1.0f) {
                    anim->m_nFlags |= ANIM_FLAG_FREEZE_LAST_FRAME;
                    anim->m_fBlendDelta = -4.0f;
                }
            }
        }
        return true;
    }

    if (!m_pAnimHierarchy || !m_pAnimHierarchy->m_pSequences) {
        return true;
    }

    if (m_bDontInterrupt && (m_animFlags & 8) && (m_animFlags & 2)) {
        ped->GetIntelligence()->GetEventGroup().Flush(false);
    }

    if (m_Timer.m_bStarted && m_Timer.IsOutOfTime()) {
        if (m_animFlags & 8) {
            MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
        }
    }

    if (!m_pAnimBlendAssoc) {
        auto anim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), m_pAnimHierarchy);
        if (!anim || !anim->m_pPrev) {
            plugin::CallMethod<0x61BB10, CTaskSimpleRunNamedAnim*, CPed*>(this, ped);
        }
    }

    return m_bIsFinished;
}

// 0x61AB00
void CTaskSimpleRunNamedAnim::OffsetPedPosition(CPed* ped) {
    ped->UpdateRpHAnim();
    ped->m_bDontUpdateHierarchy = true;
    auto& pos = ped->GetPosition();
    pos += ped->m_matrix->TransformVector(m_vecOffsetAtEnd);
    m_bOffsetAvailable = false;
}
