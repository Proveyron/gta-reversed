#include "StdInc.h"

#include "TaskComplexFollowLeaderInFormation.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimplePause.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDrive.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "TaskSimpleAchieveHeading.h"
#include "PedGroups.h"

void CTaskComplexFollowLeaderInFormation::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowLeaderInFormation, 0x870c3c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6949A0);
    RH_ScopedInstall(Destructor, 0x694A40);

    RH_ScopedVMTInstall(Clone, 0x695740);
    RH_ScopedVMTInstall(GetTaskType, 0x694A30);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x696820);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6968E0);
    RH_ScopedVMTInstall(ControlSubTask, 0x696940);
    RH_ScopedInstall(CreateSubTask, 0x6962A0);
}

// 0x6949A0
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(CPedGroup* pedGroup, CPed* ped, const CVector& posn, float a5) : CTaskComplex() {
    m_Group  = pedGroup;
    m_Leader = ped;
    m_Pos    = posn;
    m_Int    = 4;
    m_Dist   = a5;
    CEntity::SafeRegisterRef(m_Leader);
}

// For 0x695740
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(const CTaskComplexFollowLeaderInFormation& o) :
    CTaskComplexFollowLeaderInFormation{
        o.m_Group,
        o.m_Leader,
        o.m_Pos,
        o.m_Dist
    }
{
}

// 0x694A40
CTaskComplexFollowLeaderInFormation::~CTaskComplexFollowLeaderInFormation() {
    CEntity::SafeCleanUpRef(m_Leader);
}

// 0x6962A0
CTask* CTaskComplexFollowLeaderInFormation::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill(500, false, false, 8.0f);
    case TASK_SIMPLE_PAUSE:
        return new CTaskSimplePause(2000);
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return new CTaskComplexEnterCarAsPassenger(m_Leader->m_pVehicle);
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar(ped->m_pVehicle, TARGET_DOOR_FRONT_LEFT, 0, true, false);
    case TASK_SIMPLE_CAR_DRIVE:
        return new CTaskSimpleCarDrive(ped->m_pVehicle);
    case TASK_COMPLEX_SEEK_ENTITY: {
        return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>{
            m_Leader,
            50'000,
            1'000,
            2.0f,
            2.0f,
            0.0f,
            true,
            true,
            CEntitySeekPosCalculatorXYOffset{ m_Pos }
        };
    }
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        int32 numMembers = m_Group ? m_Group->GetMembership().CountMembers() : 0;
        CPed* member = nullptr;
        if (numMembers > 0) {
            member = m_Group->GetMembership().GetMember(CGeneral::GetRandomNumberInRange(0, numMembers));
        }
        if (member == ped) {
            member = m_Group->GetMembership().GetLeader();
        }
        if (member && CGeneral::GetRandomNumberInRange(0, 50) == 20) {
            CVector diff = member->GetPosition() - ped->GetPosition();
            diff.Normalise();
            float heading = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(diff.x, diff.y, 0.0f, 0.0f));
            return new CTaskSimpleAchieveHeading(heading, 0.5f, 0.2f);
        }
        return new CTaskSimpleAchieveHeading(ped->m_fCurrentRotation, 0.5f, 0.2f);
    }
    default:
        return nullptr;
    }
}

// 0x6968E0
CTask* CTaskComplexFollowLeaderInFormation::CreateFirstSubTask(CPed* ped) {
    if (ped->bInVehicle && ped->m_pVehicle) {
        if (m_Leader->bInVehicle && m_Leader->m_pVehicle == ped->m_pVehicle) {
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE, ped);
        }
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    }
    return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
}

// 0x696820
CTask* CTaskComplexFollowLeaderInFormation::CreateNextSubTask(CPed* ped) {
    auto subTaskType = m_pSubTask->GetTaskType();
    int32 leaderMoveState = m_Leader->m_nMoveState;
    switch (subTaskType) {
    case TASK_SIMPLE_PAUSE:
        return CreateSubTask(TASK_SIMPLE_PAUSE, ped);
    case TASK_SIMPLE_STAND_STILL:
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return CreateFirstSubTask(ped);
    case TASK_COMPLEX_LEAVE_CAR:
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    case TASK_SIMPLE_CAR_DRIVE:
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    case TASK_COMPLEX_SEEK_ENTITY:
        if (leaderMoveState == PEDMOVE_STILL || leaderMoveState == PEDMOVE_WALK) {
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    default:
        return nullptr;
    }
}

// 0x696940
CTask* CTaskComplexFollowLeaderInFormation::ControlSubTask(CPed* ped) {
    if (!m_Leader) {
        return nullptr;
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE) {
        return m_pSubTask;
    }

    if (m_Leader->bInVehicle && m_Leader->m_pVehicle) {
        CVehicle* veh = m_Leader->m_pVehicle;
        float radius = veh->GetColModel()->GetBoundRadius();
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY) {
            if (m_Dist >= 0.0f) {
                CVector diff = ped->GetPosition() - m_Leader->GetPosition();
                if (diff.SquaredMagnitude() > m_Dist * m_Dist) {
                    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                        CTaskSimpleStandStill standStill{ 0, false, false, 8.0f };
                        standStill.ProcessPed(ped);
                        return new CTaskSimplePause(2000);
                    }
                }
            }
        } else if (m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL) {
            if (!CPedGroups::GetPedsGroup(ped)) {
                CVector diff = ped->GetPosition() - m_Leader->GetPosition();
                if (diff.SquaredMagnitude() <= radius * radius) {
                    if (veh->m_nNumPassengers < veh->m_nMaxPassengers) {
                        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                            return new CTaskComplexEnterCarAsPassenger(veh);
                        }
                    }
                } else if (m_Dist >= 0.0f && diff.SquaredMagnitude() > m_Dist * m_Dist) {
                    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
                    }
                }
            }
        }
    } else {
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) {
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                return CreateFirstSubTask(ped);
            }
        } else if (m_pSubTask->GetTaskType() == TASK_SIMPLE_PAUSE || m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL) {
            CVector diff = ped->GetPosition() - m_Leader->GetPosition();
            if (diff.SquaredMagnitude() > m_Dist * m_Dist) {
                if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                    return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
                }
            }
        }
    }

    return m_pSubTask;
}
