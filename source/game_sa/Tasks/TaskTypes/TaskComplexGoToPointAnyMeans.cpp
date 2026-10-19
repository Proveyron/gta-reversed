#include "StdInc.h"

#include "TaskComplexGoToPointAnyMeans.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskSimpleCreateCarAndGetIn.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexFollowNodeRoute.h"
#include "CarEnterExit.h"
#include "Timer.h"

void CTaskComplexGoToPointAnyMeans::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointAnyMeans, 0x86FF68, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "1", 0x66B720, CTaskComplexGoToPointAnyMeans*(CTaskComplexGoToPointAnyMeans::*)(int32, CVector const&, float, int32));
    RH_ScopedOverloadedInstall(Constructor, "2", 0x66B790, CTaskComplexGoToPointAnyMeans*(CTaskComplexGoToPointAnyMeans::*)(int32, CVector const&, CVehicle*, float, int32));
    RH_ScopedInstall(Destructor, 0x66B830);
    RH_ScopedInstall(CreateSubTask, 0x6705D0);
    RH_ScopedInstall(CreateNextSubTask, 0x6728A0);
    RH_ScopedInstall(CreateFirstSubTask, 0x6729C0);
    RH_ScopedInstall(ControlSubTask, 0x672A50);
}

// 0x66B720
CTaskComplexGoToPointAnyMeans::CTaskComplexGoToPointAnyMeans(int32 moveState, const CVector& posn, float radius, int32 modelId) : CTaskComplex() {
    m_Pos = posn;
    m_fRadius = radius;
    m_MoveState = static_cast<eMoveState>(moveState);
    m_nModelId = modelId;
    m_nStartTimeInMs = 0;
    m_nTimeOffsetInMs = 0;
    m_bRefreshTime = false;
    m_bResetStartTime = false;
    m_Vehicle = nullptr;
}

// 0x66B790
CTaskComplexGoToPointAnyMeans::CTaskComplexGoToPointAnyMeans(int32 moveState, const CVector& posn, CVehicle* vehicle, float radius, int32 modelId)
    : CTaskComplexGoToPointAnyMeans(moveState, posn, radius, modelId)
{
    m_Vehicle = vehicle;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x66B830
CTaskComplexGoToPointAnyMeans::~CTaskComplexGoToPointAnyMeans() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x6705D0
CTask* CTaskComplexGoToPointAnyMeans::CreateSubTask(int32 taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        return new CTaskComplexEnterCarAsDriver(m_Vehicle);

    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar(ped->m_pVehicle, 0, 0, true, false);

    case TASK_COMPLEX_CAR_DRIVE_TO_POINT:
        return new CTaskComplexDriveToPoint(m_Vehicle, m_Pos, -1.0f, 0, MODEL_INVALID, -1.0f, DRIVING_STYLE_STOP_FOR_CARS);

    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN:
        return new CTaskSimpleCreateCarAndGetIn(ped->GetPosition(), m_nModelId);

    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return new CTaskComplexGoToPointAndStandStill(m_MoveState, m_Pos, m_fRadius, 2.0f, false, false);

    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
        return new CTaskComplexFollowNodeRoute(m_MoveState, m_Pos, m_fRadius, 3.0f, 2.0f, false, -1, true);

    default:
        return nullptr;
    }
}

// 0x6728A0
CTask* CTaskComplexGoToPointAnyMeans::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER: {
        if (ped->bInVehicle && ped->m_pVehicle) {
            return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);
        }
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
    }
    case TASK_COMPLEX_LEAVE_CAR:
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);

    case TASK_COMPLEX_CAR_DRIVE_TO_POINT:
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);

    case TASK_SIMPLE_CREATE_CAR_AND_GET_IN: {
        if (ped->bInVehicle) {
            return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);
        }
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
    default:
        return nullptr;
    }
}

// 0x6729C0
CTask* CTaskComplexGoToPointAnyMeans::CreateFirstSubTask(CPed* ped) {
    if (m_Vehicle) {
        if (ped->bInVehicle && ped->m_pVehicle) {
            return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);
        } else {
            return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
        }
    }

    if (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle->IsDriver(ped)) {
        return CreateSubTask(TASK_COMPLEX_CAR_DRIVE_TO_POINT, ped);
    }

    return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
}

// 0x672A50
CTask* CTaskComplexGoToPointAnyMeans::ControlSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_NODE_ROUTE) {
        return m_pSubTask;
    }

    if (m_nModelId != -1) {
        if (!m_bRefreshTime) {
            m_nStartTimeInMs = CTimer::GetTimeInMS();
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime = true;
        }
    }

    if (m_bRefreshTime) {
        if (m_bResetStartTime) {
            m_nStartTimeInMs = CTimer::GetTimeInMS();
            m_bResetStartTime = false;
        }

        if (CTimer::GetTimeInMS() >= m_nStartTimeInMs + m_nTimeOffsetInMs) {
            CTask* task = CreateSubTask(TASK_SIMPLE_CREATE_CAR_AND_GET_IN, ped);
            m_nTimeOffsetInMs = 3000;
            m_bRefreshTime = true;
            m_nStartTimeInMs = CTimer::GetTimeInMS();
            return task;
        }
    }

    if (DistanceBetweenPoints(ped->GetPosition(), m_Pos) > 50.0f) {
        CVehicle* vehicle = ped->GetIntelligence()->m_vehicleScanner.GetClosestVehicleInRange();
        if (vehicle && vehicle != m_Vehicle) {
            if (CCarEnterExit::IsVehicleStealable(vehicle, ped)) {
                CEntity::SafeCleanUpRef(m_Vehicle);
                m_Vehicle = vehicle;
                CEntity::SafeRegisterRef(m_Vehicle);
                return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
            }
        }
    }

    return m_pSubTask;
}
