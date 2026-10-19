#include "StdInc.h"

#include "TaskComplexFollowPointRoute.h"
#include "TaskComplexWalkRoundCar.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexEnterCar.h"
#include "CarEnterExit.h"
#include "General.h"
#include "Timer.h"

void CTaskComplexWalkRoundCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWalkRoundCar, 0x86f308, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6541B0);
    RH_ScopedInstall(Destructor, 0x656B00);

    RH_ScopedInstall(SetNewVehicle, 0x654290);
    RH_ScopedInstall(CreateRouteTask, 0x6542E0);
    RH_ScopedInstall(ComputeRouteRoundSmallCar, 0x6544F0, { .Reversed = false });
    RH_ScopedInstall(GoingForDoor, 0x654720);
    RH_ScopedInstall(ComputeRouteRoundBigCar, 0x656BB0, { .Reversed = false });
    RH_ScopedInstall(ComputeRoute, 0x657B80);

    RH_ScopedVMTInstall(Clone, 0x655B00);
    RH_ScopedVMTInstall(GetTaskType, 0x654280);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x656B70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x658200);
    RH_ScopedVMTInstall(ControlSubTask, 0x654370);
}

// 0x6541B0
CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(eMoveState moveState, CVector const& targetPt, CVehicle* vehicle, bool isPedGoingForCarDoor, uint8 forceThisDirectionRoundCar) :
    m_MoveState{(uint8)moveState},
    m_bIsPedGoingForCarDoor{isPedGoingForCarDoor},
    m_ForceThisDirectionRoundCar{forceThisDirectionRoundCar},
    m_TargetPt{targetPt},
    m_Veh{vehicle},
    m_Route{new CPointRoute{}}
{
    CEntity::SafeRegisterRef(m_Veh);
}

CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(const CTaskComplexWalkRoundCar& o) :
    CTaskComplexWalkRoundCar{
        (eMoveState)o.m_MoveState,
        o.m_TargetPt,
        o.m_Veh,
        o.m_bIsPedGoingForCarDoor,
        o.m_ForceThisDirectionRoundCar
    }
{
}

// 0x656B00
CTaskComplexWalkRoundCar::~CTaskComplexWalkRoundCar() {
    CEntity::SafeCleanUpRef(m_Veh);
    delete m_Route;
}


// 0x654290
void CTaskComplexWalkRoundCar::SetNewVehicle(CVehicle * vehicle, uint8 forceThisDirectionRoundCar) {
    if (notsa::IsFixBugs()) {
        if (vehicle == m_Veh) {
            return;
        }
    }
    CEntity::ChangeEntityReference(m_Veh, vehicle);
    m_ForceThisDirectionRoundCar    = forceThisDirectionRoundCar;
    m_bFirstSubTaskNeedsToBeCreated = true;
    m_Route->Clear();
}

// 0x6542E0
CTask* CTaskComplexWalkRoundCar::CreateRouteTask(CPed*) const {
    if (m_Route->IsEmpty()) {
        return nullptr;
    }
    return new CTaskComplexFollowPointRoute{ (eMoveState)m_MoveState, *m_Route };
}

// 0x6544F0
float CTaskComplexWalkRoundCar::ComputeRouteRoundSmallCar(CPed* ped) {
    return plugin::CallMethodAndReturn<float, 0x6544F0, CTaskComplexWalkRoundCar*, CPed*>(this, ped);
}

// 0x656BB0
float CTaskComplexWalkRoundCar::ComputeRouteRoundBigCar(CPed* ped) {
    return plugin::CallMethodAndReturn<float, 0x656BB0, CTaskComplexWalkRoundCar*, CPed*>(this, ped);
}

// 0x657B80
float CTaskComplexWalkRoundCar::ComputeRoute(CPed* ped) {
    return m_Veh->vehicleFlags.bIsBig
        ? ComputeRouteRoundBigCar(ped)
        : ComputeRouteRoundSmallCar(ped);
}

// 0x656B70
CTask* CTaskComplexWalkRoundCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return CreateRouteTask(ped);
    }
    return nullptr;
}

// 0x658200
CTask* CTaskComplexWalkRoundCar::CreateFirstSubTask(CPed* ped) {
    m_Route->Clear();
    float routeDist = ComputeRoute(ped);
    if (m_Route->IsEmpty()) {
        return nullptr;
    }

    uint32 timerInterval = (m_MoveState == PEDMOVE_SPRINT) ? 20000 : 15000;
    if (m_Veh->m_pDriver || m_Veh->m_apPassengers[0]) {
        timerInterval = static_cast<uint32>(static_cast<float>(timerInterval) * 4.0f);
    }
    m_Timer.Start(timerInterval);

    m_VehPos      = m_Veh->GetPosition();
    m_VehMatFwd   = m_Veh->GetMatrix()->GetForward();
    m_VehMatRight = m_Veh->GetMatrix()->GetRight();

    CVector diff = (*m_Route)[0] - ped->GetPosition();
    float diffDist = diff.Magnitude();
    float dot = DotProduct(diff, ped->GetMatrix()->GetForward());

    float maxDist = 6.0f;
    if (m_MoveState == PEDMOVE_SPRINT) {
        maxDist = 2.0f;
    } else if (m_MoveState == PEDMOVE_RUN) {
        maxDist = 4.0f;
    }

    if (ped->IsPlayer()) {
        if (auto enterCarTask = ped->GetTaskManager().FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_ENTER_CAR_AS_DRIVER)) {
            m_EnterCarStartTime = enterCarTask->As<CTaskComplexEnterCar>()->m_nStartTime;
        }
    }

    if (!m_Veh->vehicleFlags.bIsBig && diffDist <= maxDist && routeDist <= maxDist && dot <= 0.0f) {
        return CreateRouteTask(ped);
    }

    CTaskSimpleStandStill standStillTask(0, false, false, 8.0f);
    standStillTask.ProcessPed(ped);

    const auto& firstPoint = (*m_Route)[0];
    float angle = CGeneral::GetRadianAngleBetweenPoints(firstPoint.x - ped->GetPosition().x, firstPoint.y - ped->GetPosition().y, 0.0f, 0.0f);
    float heading = CGeneral::LimitRadianAngle(angle);
    return new CTaskSimpleAchieveHeading(heading, 1.0f, 0.1f);
}

// 0x654370
CTask* CTaskComplexWalkRoundCar::ControlSubTask(CPed* ped) {
    if (m_bFirstSubTaskNeedsToBeCreated) {
        m_bFirstSubTaskNeedsToBeCreated = false;
        return CreateFirstSubTask(ped);
    }

    bool bAbortParent = false;
    bool bTimerOrAbort = false;

    if (ped->IsPlayer() && m_Veh && m_EnterCarStartTime != -1) {
        if (CCarEnterExit::IsPlayerToQuitCarEnter(ped, m_Veh, m_EnterCarStartTime, m_pSubTask)) {
            if (m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE) {
                bAbortParent = true;
                bTimerOrAbort = true;
            }
        }
    }

    if (!bTimerOrAbort && m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE) {
        if (m_Timer.IsOutOfTime()) {
            bTimerOrAbort = true;
        }
    }

    bool bVehicleMoved = false;
    if (m_Veh) {
        CVector posDiff = m_VehPos - m_Veh->GetPosition();
        if (posDiff.SquaredMagnitude() > 0.0625f) { // > 0.25m
            bVehicleMoved = true;
        } else {
            float dotFwd = DotProduct(m_VehMatFwd, m_Veh->GetMatrix()->GetForward());
            if (dotFwd < 0.9f) {
                bVehicleMoved = true;
            } else {
                float dotRight = DotProduct(m_VehMatRight, m_Veh->GetMatrix()->GetRight());
                if (dotRight < 0.9f) {
                    bVehicleMoved = true;
                }
            }
        }
    }

    if (!bTimerOrAbort && !bVehicleMoved) {
        return m_pSubTask;
    }

    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        if (bAbortParent) {
            if (auto primary = ped->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY)) {
                if (primary->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_DRIVER) {
                    primary->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
                }
            }
        }
        return nullptr;
    }

    return m_pSubTask;
}
