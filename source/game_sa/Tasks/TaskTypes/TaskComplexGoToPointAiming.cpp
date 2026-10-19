#include "StdInc.h"

#include "TaskComplexGoToPointAiming.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"

void CTaskComplexGoToPointAiming::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointAiming, 0x86fe00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x668790);
    RH_ScopedInstall(Destructor, 0x668870);
    RH_ScopedInstall(CreateSubTask, 0x6688D0);

    RH_ScopedVMTInstall(Clone, 0x66CD80);
    RH_ScopedVMTInstall(GetTaskType, 0x668860);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66DD70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66DDB0);
    RH_ScopedVMTInstall(ControlSubTask, 0x6689E0);
}

// 0x668790
CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(
    eMoveState moveState,
    const CVector& movePos,
    CEntity* aimAtEntity,
    CVector aimPos,
    const float targetRadius,
    const float slowDownDist
) :
    m_aimPos{aimPos},
    m_aimAtEntity{aimAtEntity},
    m_moveState{moveState},
    m_movePos{movePos},
    m_moveTargetRadius{targetRadius},
    m_slowDownDistance{slowDownDist}
{
    CEntity::SafeRegisterRef(m_aimAtEntity);
}

CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(const CTaskComplexGoToPointAiming& o) :
    CTaskComplexGoToPointAiming{
        o.m_moveState,
        o.m_movePos,
        o.m_aimAtEntity,
        o.m_aimPos,
        o.m_moveTargetRadius,
        o.m_slowDownDistance
    }
{
}

// 0x668870
CTaskComplexGoToPointAiming::~CTaskComplexGoToPointAiming() {
    CEntity::SafeCleanUpRef(m_aimAtEntity);
}

// 0x6688D0
CTask* CTaskComplexGoToPointAiming::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return new CTaskComplexGoToPointAndStandStill{
            m_moveState,
            m_movePos,
            m_moveTargetRadius,
            m_slowDownDistance,
            false,
            false
        };
    case TASK_SIMPLE_GUN_CTRL:
        return new CTaskSimpleGunControl{
            m_aimAtEntity,
            m_aimPos,
            CVector{}, // The original passes `nullptr` here
            GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING
                ? eGunCommand::FIREBURST
                : eGunCommand::NONE,
            1,
            600'000
        };
    default:
        return nullptr;
    }
}

// 0x66DD70
CTask* CTaskComplexGoToPointAiming::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_SIMPLE_GUN_CTRL:
        return CreateSubTask(TASK_FINISHED);
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x66DDB0
CTask* CTaskComplexGoToPointAiming::CreateFirstSubTask(CPed* ped) {
    m_newTargetSet = false;

    const auto& winfo = ped->GetActiveWeapon().GetWeaponInfo(ped);
    if (winfo.flags.bAimWithArm) {
        auto* secTask = ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK);
        if (!secTask) {
            CTask* useGun = nullptr;
            if (GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING) {
                useGun = new CTaskSimpleUseGun(m_aimAtEntity, m_aimPos, eGunCommand::FIREBURST, 3, false);
            } else {
                useGun = new CTaskSimpleUseGun(m_aimAtEntity, m_aimPos, eGunCommand::AIM, 1, false);
            }
            ped->GetTaskManager().SetTaskSecondary(useGun, TASK_SECONDARY_ATTACK);
        } else {
            secTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
        }
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
    }

    if (winfo.flags.bCanAim) {
        if (winfo.m_nWeaponFire == WEAPON_FIRE_INSTANT_HIT || winfo.m_nWeaponFire == WEAPON_FIRE_AREA_EFFECT) {
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL);
        }
    }

    return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
}

// 0x6689E0
CTask* CTaskComplexGoToPointAiming::ControlSubTask(CPed* ped) {
    const auto& winfo = ped->GetActiveWeapon().GetWeaponInfo(ped);

    if (m_newTargetSet) {
        return CreateFirstSubTask(ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_GUN_CTRL: {
        if (ped->GetIntelligence()->GetTaskUseGun()) {
            const CVector delta = m_movePos - ped->GetPosition();
            const CMatrix& mat = ped->GetMatrix();
            const float localX = DotProduct(delta, mat.GetRight());
            const float localY = DotProduct(delta, mat.GetForward()) * -1.0f;
            const float distSq = localX * localX + localY * localY;
            CVector2D moveDir;
            if (distSq < m_moveTargetRadius * m_moveTargetRadius) {
                moveDir = { 0.0f, 0.0f };
            } else {
                const float invDist = 1.0f / sqrtf(distSq);
                moveDir = { localX * invDist, localY * invDist };
            }
            if (auto* useGun = static_cast<CTaskSimpleUseGun*>(ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK))) {
                useGun->ControlGunMove(moveDir);
            }
        }
        break;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask)->GoToPoint(m_movePos, 0.5f, 2.0f, false);
        if (winfo.flags.bAimWithArm) {
            auto* secTask = ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK);
            if (!secTask) {
                ped->GetTaskManager().SetTaskSecondary(new CTaskSimpleUseGun(m_aimAtEntity, m_aimPos, eGunCommand::AIM, 1, false), TASK_SECONDARY_ATTACK);
            } else {
                if (auto* useGun = ped->GetIntelligence()->GetTaskUseGun()) {
                    if (GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING) {
                        if ((CGeneral::GetRandomNumber() & 0x3F) == 0) {
                            useGun->ControlGun(ped, m_aimAtEntity, eGunCommand::FIRE);
                        } else {
                            useGun->ControlGun(ped, m_aimAtEntity, eGunCommand::AIM);
                        }
                    } else {
                        useGun->ControlGun(ped, m_aimAtEntity, eGunCommand::AIM);
                    }
                } else {
                    secTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
                }
            }
        }
        break;
    }
    default:
        break;
    }

    return m_pSubTask;
}
