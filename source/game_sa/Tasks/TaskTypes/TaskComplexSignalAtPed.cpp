#include "StdInc.h"
#include "TaskComplexSignalAtPed.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleDoHandSignal.h"
#include "TaskSimpleRunAnim.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexGangLeader.h"
#include "Ragdoll/IKChainManager.h"
#include "Streaming.h"

void CTaskComplexSignalAtPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSignalAtPed, 0x86fa8c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660A30);
    RH_ScopedInstall(Destructor, 0x660AB0);

    RH_ScopedVMTInstall(Clone, 0x662140);
    RH_ScopedVMTInstall(GetTaskType, 0x660AA0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x660B30);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x660CC0);
    RH_ScopedVMTInstall(ControlSubTask, 0x660D80);
}

// 0x660A30
CTaskComplexSignalAtPed::CTaskComplexSignalAtPed(CPed* pedToSignalAt, int32 unused1, bool playAnimAtEnd) :
    m_pedToSignalAt{pedToSignalAt},
    m_initialPause{unused1},
    m_playAnimAtEnd{playAnimAtEnd}
{
    CEntity::SafeRegisterRef(m_pedToSignalAt);
}

CTaskComplexSignalAtPed::CTaskComplexSignalAtPed(const CTaskComplexSignalAtPed& o) :
    CTaskComplexSignalAtPed{o.m_pedToSignalAt, o.m_initialPause, o.m_playAnimAtEnd}
{
}

// 0x660AB0
CTaskComplexSignalAtPed::~CTaskComplexSignalAtPed() {
    if (m_areAnimsReferenced) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_areAnimsReferenced = false;
    }
    CEntity::SafeCleanUpRef(m_pedToSignalAt);
}

// 0x660B30
CTask* CTaskComplexSignalAtPed::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_TURN_TO_FACE_ENTITY: {
        int32 pause = m_initialPause;
        if (pause == -1) {
            pause = CGeneral::GetRandomNumberInRange(0, 2000);
        }
        return new CTaskSimpleStandStill(pause, false, false, 8.0f);
    }
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleDoHandSignal();
    case TASK_SIMPLE_DO_HAND_SIGNAL: {
        if (m_areAnimsReferenced && m_playAnimAtEnd) {
            const auto animId = static_cast<AnimationId>(ANIM_ID_PRTIAL_GNGTLKA + CGeneral::GetRandomNumberInRange(0, 8));
            return new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, animId, 4.0f, 0);
        }
        return nullptr;
    }
    default:
        return nullptr;
    }
}

// 0x660CC0
CTask* CTaskComplexSignalAtPed::CreateFirstSubTask(CPed* ped) {
    if (!m_pedToSignalAt) {
        return nullptr;
    }

    if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        return nullptr;
    }

    ped->StopPlayingHandSignal();

    g_ikChainMan.LookAt(
        "TaskSignalAtPed",
        ped,
        m_pedToSignalAt,
        5000,
        static_cast<eBoneTag32>(5),
        nullptr,
        true,
        0.15f,
        500,
        3,
        false
    );

    return new CTaskComplexTurnToFaceEntityOrCoord(m_pedToSignalAt, 0.5f, 0.2f);
}

// 0x660D80
CTask* CTaskComplexSignalAtPed::ControlSubTask(CPed* ped) {
    if (!m_areAnimsReferenced) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const int32 blockIndex = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CAnimManager::ms_aAnimBlocks[blockIndex].IsLoaded) {
                CAnimManager::AddAnimBlockRef(blockIndex);
                m_areAnimsReferenced = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blockIndex), STREAMING_KEEP_IN_MEMORY);
            }
        }
    } else {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const int32 blockIndex = CAnimManager::GetAnimationBlockIndex("gangs");
            CAnimManager::RemoveAnimBlockRef(blockIndex);
            m_areAnimsReferenced = false;
        }
    }

    if (m_pedToSignalAt) {
        const CVector& pedPos = ped->GetPosition();
        const CVector& targetPos = m_pedToSignalAt->GetPosition();
        const float angle = CGeneral::LimitRadianAngle(
            CGeneral::GetRadianAngleBetweenPoints(targetPos.x - pedPos.x, targetPos.y - pedPos.y, 0.0f, 0.0f)
        );
        ped->m_fCurrentRotation = angle;
    }

    return m_pSubTask;
}
