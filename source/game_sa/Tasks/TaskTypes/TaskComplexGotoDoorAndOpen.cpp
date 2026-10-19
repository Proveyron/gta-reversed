#include "StdInc.h"

#include "TaskComplexGotoDoorAndOpen.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleRunAnim.h"
#include "EventAreaCodes.h"
#include "EventGroup.h"
#include "AnimManager.h"
#include "Pad.h"
#include "Timer.h"
#include "RwHelper.h"

void CTaskComplexGotoDoorAndOpen::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGotoDoorAndOpen, 0x86FFC0, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "0", 0x66BB20, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CObject *));
    RH_ScopedOverloadedInstall(Constructor, "1", 0x66BBA0, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CVector const&, CVector const&));
    RH_ScopedInstall(Destructor, 0x66BC00);
    RH_ScopedInstall(Clone, 0x66BCA0);
    RH_ScopedInstall(CreateNextSubTask, 0x66C0D0);
    RH_ScopedInstall(CreateFirstSubTask, 0x66BD40);
    RH_ScopedInstall(ControlSubTask, 0x66C1F0);
}

// 0x66BB20
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(CObject* door) : CTaskComplex() {
    m_Object = door;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags = (m_nFlags & 0xF0) | 1;
    CEntity::SafeRegisterRef(m_Object);
}

// 0x66BBA0
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(const CVector& start, const CVector& end) : CTaskComplex() {
    m_Object = nullptr;
    m_Start = start;
    m_End = end;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags &= 244u;
}

// 0x66BC00
CTaskComplexGotoDoorAndOpen::~CTaskComplexGotoDoorAndOpen() {
    CEntity::SafeCleanUpRef(m_Object);
    if ((m_nFlags & 8) != 0) { // transition finished
        CPad::GetPad(0)->bPlayerOnInteriorTransition = false;
    }
}

// 0x66BCA0
CTask* CTaskComplexGotoDoorAndOpen::Clone() const {
    if ((m_nFlags & 1) != 0) {
        return new CTaskComplexGotoDoorAndOpen(m_Object);
    } else {
        return new CTaskComplexGotoDoorAndOpen(m_Start, m_End);
    }
}

// 0x66BC80
bool CTaskComplexGotoDoorAndOpen::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
}

// 0x66C0D0
CTask* CTaskComplexGotoDoorAndOpen::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
        return nullptr;
    }

    if (m_nFlags & 2) {
        if (ped->IsPlayer()) {
            CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
        }
        return nullptr;
    }

    if (!ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        auto* animTask = new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_WALK_DOORPARTIAL, 8.0f, false);
        ped->GetTaskManager().SetTaskSecondary(animTask, TASK_SECONDARY_PARTIAL_ANIM);
    }

    m_nFlags |= 2;
    return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_End, 0.2f, 0.0f, false, false);
}

// 0x66BD40
CTask* CTaskComplexGotoDoorAndOpen::CreateFirstSubTask(CPed* ped) {
    if (ped->IsPlayer()) {
        CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
        m_nFlags |= 8;
    }

    CEventAreaCodes event(ped);
    GetEventGlobalGroup()->Add(&event, false);

    m_nStartTime = CTimer::GetTimeInMS();
    m_nOffsetTime = 1000;
    byte30 = 1;

    if (m_Object) {
        CVector doorPos = m_Object->GetPosition();
        CVector right   = m_Object->GetRightVector();
        CVector forward = m_Object->GetForwardVector();

        CVector diff = doorPos - ped->GetPosition();
        float dot = DotProduct(diff, forward);

        doorPos += right * 0.75f;

        if (dot > 0.0f) {
            m_Start = doorPos - forward * 0.5f;
            m_End   = doorPos + forward * 2.0f;
        } else {
            m_Start = doorPos + forward * 0.5f;
            m_End   = doorPos - forward * 2.0f;
        }

        if (m_Object->physicalFlags.bCollidable) {
            m_nFlags |= 4;
            m_Object->physicalFlags.bCollidable = false;
            m_Object->physicalFlags.bDisableCollisionForce = false;
        }

        if (RpAnimBlendClumpGetAssociation(ped->m_pRwClump, ANIM_ID_SPRINT)) {
            CAnimManager::BlendAnimation(ped->m_pRwClump, ped->m_nAnimGroup, ANIM_ID_SPRINT, 1000.0f);
        }

        return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_Start, 0.35f, 0.0f, false, false);
    } else {
        if (m_nFlags & 1) {
            return nullptr;
        }

        m_nFlags |= 2;
        return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_End, 0.5f, 0.0f, false, false);
    }
}

// 0x66C1F0
CTask* CTaskComplexGotoDoorAndOpen::ControlSubTask(CPed* ped) {
    bool bShouldAbort = false;

    if (byte30) {
        if (m_bRefreshTime) {
            m_nStartTime = CTimer::GetTimeInMS();
            m_bRefreshTime = false;
        }
        if (CTimer::GetTimeInMS() >= m_nStartTime + m_nOffsetTime) {
            bShouldAbort = true;
        }
    }

    if (!bShouldAbort && (!m_Object && (m_nFlags & 1))) {
        bShouldAbort = true;
    }

    if (bShouldAbort) {
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            if (ped->IsPlayer()) {
                CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
            }
            return nullptr;
        }
    }

    return m_pSubTask;
}
