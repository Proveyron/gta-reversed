#include "StdInc.h"

#include "TaskGoToVehicleAndLean.h"
#include "TaskLeanOnVehicle.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexGoToPointAndStandStill.h"

void CTaskGoToVehicleAndLean::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGoToVehicleAndLean, 0x86FAC8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660E60);
    RH_ScopedInstall(Destructor, 0x660EE0);
    RH_ScopedInstall(Clone, 0x6621B0);
    RH_ScopedInstall(MakeAbortable, 0x664500);
    RH_ScopedInstall(CreateNextSubTask, 0x664590);
    RH_ScopedInstall(CreateFirstSubTask, 0x664D40);
    RH_ScopedInstall(ControlSubTask, 0x664E60);
}

// 0x660E60
CTaskGoToVehicleAndLean::CTaskGoToVehicleAndLean(CVehicle* vehicle, int32 leanAnimDurationInMs) : CTaskComplex() {
    m_Vehicle = vehicle;
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_LeanOnVehicle = false;
    field_15 = false;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x660EE0
CTaskGoToVehicleAndLean::~CTaskGoToVehicleAndLean() {
    if (m_LeanOnVehicle) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_LeanOnVehicle = false;
    }
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x664770
CVector CTaskGoToVehicleAndLean::CalcTargetPos(CPed* ped) {
    auto modelInfo = m_Vehicle->GetModelInfo();
    const auto& colBox = modelInfo->GetColModel()->GetBoundingBox();

    CVector pedToCar = ped->GetPosition() - m_Vehicle->GetPosition();
    float dotRight = DotProduct(pedToCar, m_Vehicle->GetRight());

    CVector targetPos{0.0f, 0.0f, 0.0f};
    if (dotRight >= 0.0f) {
        field_15 = true;
        targetPos.x = colBox.m_vecMax.x + 0.6f;
    } else {
        field_15 = false;
        targetPos.x = colBox.m_vecMin.x - 0.6f;
    }
    return *m_Vehicle->GetMatrix() * targetPos;
}

// 0x664500
bool CTaskGoToVehicleAndLean::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, priority, event);
        }
        DoTidyUp(ped);
        return true;
    }

    if (m_pSubTask && m_pSubTask->MakeAbortable(ped, priority, event)) {
        DoTidyUp(ped);
        return true;
    }

    return false;
}

// 0x664590
CTask* CTaskGoToVehicleAndLean::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        DoTidyUp(ped);
        return nullptr;
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        CVector dir = m_Vehicle->GetForward();
        if (!field_15) {
            dir = -dir;
        }
        float heading = CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.0f, 0.0f);
        return new CTaskSimpleAchieveHeading{heading};
    }
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        if (m_LeanOnVehicle) {
            return new CTaskLeanOnVehicle{m_Vehicle, m_LeanAnimDurationInMs, 0};
        }
        return new CTaskSimpleStandStill{500, false, false, 8.0f};
    }
    default:
        return nullptr;
    }
}

// 0x664D40
CTask* CTaskGoToVehicleAndLean::CreateFirstSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    if (m_Vehicle->vehicleFlags.bHasGangLeaningOn) {
        DoTidyUp(ped);
        return nullptr;
    }

    m_Vehicle->vehicleFlags.bHasGangLeaningOn = true;
    CVector targetPos = CalcTargetPos(ped);

    if ((targetPos - ped->GetPosition()).SquaredMagnitude() < 1.0f) {
        return nullptr;
    }

    return new CTaskComplexGoToPointAndStandStill{PEDMOVE_WALK, targetPos, 0.05f, 2.0f, false, false};
}

// 0x664E60
CTask* CTaskGoToVehicleAndLean::ControlSubTask(CPed* ped) {
    if (!m_Vehicle) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
        return nullptr;
    }

    if (ped->GetIntelligence()->m_AnotherStaticCounter > 30) {
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
        return nullptr;
    }

    CVector localPedPos = Multiply3x3(ped->GetPosition() - m_Vehicle->GetPosition(), *m_Vehicle->GetMatrix());
    if (m_Vehicle->m_pDriver || localPedPos.Magnitude() >= 0.05f) {
        if (m_pSubTask && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return nullptr;
        }
    }

    CVector targetPos = CalcTargetPos(ped);
    if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
        auto goToTask = static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask);
        goToTask->GoToPoint(targetPos);
    }

    return m_pSubTask;
}

// 0x660F60
void CTaskGoToVehicleAndLean::DoTidyUp(CPed* ped) {
    if (m_Vehicle)
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;

    if (m_pSubTask)
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
}
