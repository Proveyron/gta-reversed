#include "StdInc.h"

#include "TaskComplexGangJoinRespond.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleStandStill.h"

void CTaskComplexGangJoinRespond::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangJoinRespond, 0x86FB5C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6616F0);
    RH_ScopedInstall(Destructor, 0x661720);

    RH_ScopedVMTInstall(Clone, 0x662290);
    RH_ScopedVMTInstall(MakeAbortable, 0x661790);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6617A0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6618D0);
    RH_ScopedVMTInstall(ControlSubTask, 0x661950);
}

// 0x6616F0
CTaskComplexGangJoinRespond::CTaskComplexGangJoinRespond(bool response) :
    m_response{response}
{
    m_animsReferenced = false;
    m_attempts = 0;
}

// 0x661720
CTaskComplexGangJoinRespond::~CTaskComplexGangJoinRespond() {
    if (m_animsReferenced) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_animsReferenced = false;
    }
}

// 0x6618D0
CTask* CTaskComplexGangJoinRespond::CreateFirstSubTask(CPed* ped) {
    m_attempts = 0;
    return new CTaskComplexTurnToFaceEntityOrCoord{FindPlayerPed(0), 0.5f, 0.2f};
}

// 0x6617A0
CTask* CTaskComplexGangJoinRespond::CreateNextSubTask(CPed* ped) {
    if (!m_pSubTask) {
        return nullptr;
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_NAMED_ANIM) {
        return nullptr;
    }

    if (m_animsReferenced) {
        if (m_response) {
            return new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, ANIM_ID_INVITE_YES, 4.0f, false};
        } else {
            return new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, ANIM_ID_INVITE_NO, 4.0f, false};
        }
    }

    m_attempts++;
    if (m_attempts <= 10) {
        return new CTaskSimpleStandStill{250, false, false, 8.0f};
    }

    return nullptr;
}

// 0x661950
CTask* CTaskComplexGangJoinRespond::ControlSubTask(CPed* ped) {
    auto player = FindPlayerPed(0);
    if (player) {
        CVector diff = player->GetPosition() - ped->GetPosition();
        diff.z = 0.0f;
        diff.Normalise();
        ped->m_fAimingHeading = CGeneral::GetRadianAngleBetweenPoints(diff.x, diff.y, 0.0f, 0.0f);
    }

    if (!m_animsReferenced) {
        int32 blockIndex = CAnimManager::GetAnimationBlockIndex("gangs");
        if (CStreaming::IsModelLoaded(blockIndex + 25575)) {
            CAnimManager::AddAnimBlockRef(blockIndex);
            m_animsReferenced = true;
            if (m_pSubTask) {
                m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
            }
        } else {
            CStreaming::RequestModel(blockIndex + 25575, STREAMING_KEEP_IN_MEMORY);
        }
    }

    return m_pSubTask;
}
