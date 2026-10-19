#include "StdInc.h"

#include "TaskGangHasslePed.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskComplexGangLeader.h"
#include "TaskSimpleRunAnim.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "Streaming.h"

void CTaskGangHasslePed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHasslePed, 0x86FA00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FED0);
    RH_ScopedInstall(Destructor, 0x65FF60);

    RH_ScopedVMTInstall(CreateNextSubTask, 0x6642C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664380);
    RH_ScopedVMTInstall(ControlSubTask, 0x65FFE0);
}

// 0x65FED0
CTaskGangHasslePed::CTaskGangHasslePed(CPed* ped, int32 a3, int32 a4, int32 a5) : CTaskComplex() {
    m_nTime = 0;
    m_nSomeRandomShit = 0;
    m_bFirstSubTaskInitialised = 0;
    m_bRefreshTime = 0;
    dword10 = a3;
    m_RndMin = a4;
    m_Ped = ped;
    m_RndMax = a5;
    m_bAnimBlockRefAdded = false;
    CEntity::SafeRegisterRef(m_Ped);
}

// 0x65FF60
CTaskGangHasslePed::~CTaskGangHasslePed() {
    CEntity::SafeCleanUpRef(m_Ped);

    if (m_bAnimBlockRefAdded) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bAnimBlockRefAdded = false;
    }
}

// 0x6642C0
CTask* CTaskGangHasslePed::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
        return nullptr;
    }

    const float radius = CGeneral::GetRandomNumberInRange(3.0f, 5.0f);
    return new CTaskComplexSeekEntityStandard(m_Ped, 999'999, 1000, radius, 2.0f, 2.0f, false, true);
}

// 0x664380
CTask* CTaskGangHasslePed::CreateFirstSubTask(CPed* ped) {
    if (!m_Ped) {
        return nullptr;
    }

    m_nTime = CTimer::GetTimeInMS();
    m_nSomeRandomShit = CGeneral::GetRandomNumberInRange(m_RndMin, m_RndMax);
    m_bFirstSubTaskInitialised = 1;

    const float radius = CGeneral::GetRandomNumberInRange(3.0f, 5.0f);
    return new CTaskComplexSeekEntityStandard(m_Ped, 999'999, 1000, radius, 2.0f, 2.0f, false, true);
}

// 0x65FFE0
CTask* CTaskGangHasslePed::ControlSubTask(CPed* ped) {
    if (!m_Ped) {
        return nullptr;
    }

    if (m_bFirstSubTaskInitialised) {
        if (m_bRefreshTime) {
            m_nTime = CTimer::GetTimeInMS();
            m_bRefreshTime = false;
        }

        if (CTimer::GetTimeInMS() >= static_cast<uint32>(m_nTime + m_nSomeRandomShit)) {
            if (dword10 == 2) {
                if (m_pSubTask->GetTaskType() != TASK_COMPLEX_KILL_PED_ON_FOOT) {
                    return new CTaskComplexKillPedOnFoot(m_Ped, -1, 0, 0, 0, 1);
                }
            } else if (dword10 != 1) {
                return nullptr;
            }
        }
    }

    if (!m_bAnimBlockRefAdded) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const int32 blockIndex = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CAnimManager::ms_aAnimBlocks[blockIndex].IsLoaded) {
                CAnimManager::AddAnimBlockRef(blockIndex);
                m_bAnimBlockRefAdded = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blockIndex), STREAMING_KEEP_IN_MEMORY);
            }
        }
    } else {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const int32 blockIndex = CAnimManager::GetAnimationBlockIndex("gangs");
            CAnimManager::RemoveAnimBlockRef(blockIndex);
            m_bAnimBlockRefAdded = false;
        }
    }

    if (m_bAnimBlockRefAdded && !ped->IsPlayingHandSignal() && !ped->GetIntelligence()->GetTaskManager()->GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        const int32 randVal = CGeneral::GetRandomNumberInRange(0, 200);
        CTask* task = nullptr;
        if (randVal > 50 && randVal < 56) {
            const auto animId = static_cast<AnimationId>(ANIM_ID_PRTIAL_GNGTLKA + CGeneral::GetRandomNumberInRange(0, 8));
            task = new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, animId, 4.0f, 0);
        } else if (randVal == 100) {
            task = new CTaskComplexPlayHandSignalAnim(ANIM_ID_UNDEFINED, 4.0f);
        }
        if (task) {
            ped->GetIntelligence()->GetTaskManager()->SetTaskSecondary(task, TASK_SECONDARY_PARTIAL_ANIM);
        }
    }

    if (dword10 == 0) {
        ped->Say(CTX_GLOBAL_EYEING_PED, 0, 1.0f, false, false, false);
    } else if (dword10 == 1 || dword10 == 2) {
        CTaskComplexGangLeader::DoGangAbuseSpeech(ped, m_Ped);
    }

    return m_pSubTask;
}
