#include "StdInc.h"

#include "TaskLeanOnVehicle.h"

void CTaskLeanOnVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskLeanOnVehicle, 0x86FAF4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660F90);
    RH_ScopedInstall(Destructor, 0x661030);

    RH_ScopedInstall(FinishAnimCB, 0x661160);

    RH_ScopedVMTInstall(MakeAbortable, 0x661110);
    RH_ScopedVMTInstall(ProcessPed, 0x6648C0);

}

// 0x660F90
CTaskLeanOnVehicle::CTaskLeanOnVehicle(CEntity* vehicle, int32 leanAnimDurationInMs, uint8 a4) {
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_Vehicle = vehicle->AsVehicle(); // todo: strange
    field_10 = a4;
    m_LeanAnim = nullptr;
    m_LastAnimId = ANIM_ID_UNDEFINED;
    m_bFinished = 0;
    m_LeanAnimId = 0;
    m_StopLeaning = 0;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x661030
CTaskLeanOnVehicle::~CTaskLeanOnVehicle() {
    CEntity::SafeCleanUpRef(m_Vehicle);
    if (m_LeanAnim) {
        m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x661160
void CTaskLeanOnVehicle::FinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = notsa::cast<CTaskLeanOnVehicle>(static_cast<CTask*>(data));

    self->m_LastAnimId = assoc->m_AnimId;

    if (assoc->m_AnimId == ANIM_ID_LEANOUT) {
        assoc->m_BlendDelta = -1000.0f;
        self->m_bFinished = true;
    }

    if (self->m_LeanAnimId) {
        if (assoc->m_AnimId == ANIM_ID_LEANIN) {
            assoc->m_BlendDelta = -1000.0f;
            self->m_bFinished = true;
        }
    }

    self->m_LeanAnim = nullptr;
}

// 0x661110
bool CTaskLeanOnVehicle::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_LeanAnim) {
            m_LeanAnim->m_BlendDelta = -1000.0f;
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_LeanAnim = nullptr;
        }
        m_StopLeaning = true;
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    m_LeanAnimId = 1;
    return false;
}

// 0x6648C0
bool CTaskLeanOnVehicle::ProcessPed(CPed* ped) {
    if (!m_Vehicle) {
        m_StopLeaning = true;
    }
    ped->m_pEntityIgnoredCollision = m_Vehicle;

    const auto animId = m_LeanAnim ? static_cast<AnimationId>(m_LeanAnim->m_AnimId) : ANIM_ID_UNDEFINED;

    if (m_StopLeaning) {
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    ped->SetMoveState(PEDMOVE_STILL);

    if (m_bFinished && !RpAnimBlendClumpGetAssociation(ped->m_pRwClump, ANIM_ID_LEANOUT)) {
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    if (m_LeanAnimId) {
        if (animId == ANIM_ID_LEANIN) {
            m_LeanAnim->m_BlendDelta = -8.0f;
        } else if (animId == ANIM_ID_LEANIDLE) {
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_LeanAnim = CAnimManager::BlendAnimation(ped->m_pRwClump, ANIM_GROUP_GANGS, ANIM_ID_LEANOUT, 1000.0f);
            m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
            return false;
        } else if (animId == ANIM_ID_LEANOUT) {
            m_LeanAnim->m_Speed = 3.0f;
        }
    }

    if (!m_LeanAnim) {
        if (m_LastAnimId == ANIM_ID_UNDEFINED) {
            if (field_10) {
                if (m_LeanAnimDurationInMs >= 0) {
                    m_LeanTimer.Start(m_LeanAnimDurationInMs);
                }
                m_LeanAnim = CAnimManager::BlendAnimation(ped->m_pRwClump, ANIM_GROUP_GANGS, ANIM_ID_LEANIDLE, 1000.0f);
                m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
            } else {
                m_LeanAnim = CAnimManager::BlendAnimation(ped->m_pRwClump, ANIM_GROUP_GANGS, ANIM_ID_LEANIN, 4.0f);
                m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
            }
        } else if (m_LastAnimId == ANIM_ID_LEANIN) {
            if (m_LeanAnimDurationInMs >= 0) {
                m_LeanTimer.Start(m_LeanAnimDurationInMs);
            }
            m_LeanAnim = CAnimManager::BlendAnimation(ped->m_pRwClump, ANIM_GROUP_GANGS, ANIM_ID_LEANIDLE, 1000.0f);
            m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
        }
    } else {
        if (m_LeanTimer.m_bStarted && m_LeanTimer.IsOutOfTime() && m_LeanAnim->m_AnimId != ANIM_ID_LEANOUT) {
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_LeanAnim = CAnimManager::BlendAnimation(ped->m_pRwClump, ANIM_GROUP_GANGS, ANIM_ID_LEANOUT, 1000.0f);
            m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
        }
    }

    return false;
}
