#include "StdInc.h"

#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimplePause.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskComplexDragPedFromCar.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskComplexKillPedOnFootMelee.h"
#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskComplexDestroyCar.h"
#include "TaskComplexSignalAtPed.h"
#include "PedGroups.h"

void CTaskComplexKillPedOnFoot::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedOnFoot, 0x86D894, 11);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x620E30);
    RH_ScopedInstall(CreateSubTask, 0x625E70);
}

CTaskComplexKillPedOnFoot::CTaskComplexKillPedOnFoot(
    CPed* target,
    int32 time,
    int32 pedFlags,
    int32 delay,
    int32 chance,
    uint8 nCompetence,
    bool bWaitForPlayerToBeSafe,
    bool bWaitingForPlayerToBeSafe
) :
    m_bWaitForPlayerToBeSafe{ bWaitForPlayerToBeSafe },
    m_bWaitingForPlayerToBeSafe{ bWaitingForPlayerToBeSafe },
    m_target{ target },
    m_pedFlags{ pedFlags },
    m_actionDelay{ delay },
    m_actionChance{ chance },
    m_nCompetence{ nCompetence },
    m_time{ time },
    m_startTime{ CTimer::GetTimeInMS() }
{
    CEntity::SafeRegisterRef(m_target);
}

CTaskComplexKillPedOnFoot::~CTaskComplexKillPedOnFoot() {
    CEntity::SafeCleanUpRef(m_target);
}

CTaskComplexKillPedOnFoot* CTaskComplexKillPedOnFoot::Constructor(CPed* target, int32 time, int32 pedFlags, int32 delay, int32 chance, int8 a7) {
    this->CTaskComplexKillPedOnFoot::CTaskComplexKillPedOnFoot(target, time, pedFlags, delay, chance, a7);
    return this;
}

bool CTaskComplexKillPedOnFoot::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return plugin::CallMethodAndReturn<bool, 0x625E40, CTaskComplexKillPedOnFoot*, CPed*, eAbortPriority, const CEvent*>(this, ped, priority, event);
}

CTask* CTaskComplexKillPedOnFoot::CreateNextSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x62B150, CTaskComplexKillPedOnFoot*, CPed*>(this, ped);
}

CTask* CTaskComplexKillPedOnFoot::CreateFirstSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x62B490, CTaskComplexKillPedOnFoot*, CPed*>(this, ped);
}

CTask* CTaskComplexKillPedOnFoot::ControlSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x626260, CTaskComplexKillPedOnFoot*, CPed*>(this, ped);
}

// 0x625E70
CTask* CTaskComplexKillPedOnFoot::CreateSubTask(int32 taskId, CPed* ped) {
    switch (taskId) {
    case TASK_NONE:
        return new CTaskSimpleLeaveGroup{};

    case TASK_SIMPLE_PAUSE: {
        CTaskSimpleStandStill standStill{ 0, false, false, 8.0f };
        standStill.ProcessPed(ped);
        const auto pauseTime = (m_bWaitForPlayerToBeSafe && m_bWaitingForPlayerToBeSafe) ? 10000 : 2000;
        return new CTaskSimplePause{ pauseTime };
    }

    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 0, false, false, 8.0f };

    case TASK_COMPLEX_DRAG_PED_FROM_CAR:
        return new CTaskComplexDragPedFromCar{ m_target, 0 };

    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ ped->m_pVehicle, 0, 0, true, true };

    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return new CTaskSimpleCarDriveTimed{ ped->m_pVehicle, 2000 };

    case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE:
        m_timer.Start(2000);
        return new CTaskComplexKillPedOnFootMelee{ m_target };

    case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED: {
        auto* task = new CTaskComplexKillPedOnFootArmed{
            m_target,
            static_cast<uint32>(m_pedFlags),
            static_cast<uint32>(m_actionDelay),
            static_cast<uint32>(m_actionChance),
            static_cast<int8>(m_nCompetence)
        };
        task->m_bAimImmediate = m_bAimImmediate;
        m_bAimImmediate = false;
        m_timer.Start(2000);
        return task;
    }

    case TASK_COMPLEX_DESTROY_CAR:
        return new CTaskComplexDestroyCar{ m_target->m_pVehicle, 0, 0, 0 };

    case TASK_COMPLEX_SIGNAL_AT_PED: {
        const auto player = FindPlayerPed(0);
        if (m_target == player) {
            ped->Say(CTX_GLOBAL_GENERIC_INSULT);
        } else {
            const auto group = CPedGroups::GetPedsGroup(m_target);
            if (!group || group->GetMembership().GetLeader() != player) {
                ped->Say(CTX_GLOBAL_GANG_INSULT);
            }
        }
        return new CTaskComplexSignalAtPed{ m_target, static_cast<uint16>(CGeneral::GetRandomNumberInRange(0, 1500)), 1 };
    }

    case TASK_SIMPLE_TOGGLE_PED_THREAT_SCANNER:
        ped->bKindaStayInSamePlace = false;
        return nullptr;

    default:
        return nullptr;
    }
}
