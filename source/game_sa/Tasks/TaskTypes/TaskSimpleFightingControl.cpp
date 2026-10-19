#include "StdInc.h"

#include "TaskSimpleFightingControl.h"
#include "PedStats.h"
#include "TaskSimpleFight.h"

#include "EventObjectCollision.h"
#include "EventBuildingCollision.h"

constexpr auto FIGHT_CTRL_MAX_ATTACK_ANGLE_RAD = DegreesToRadians(15);
constexpr auto FIGHT_CTRL_FIGHT_IDLE_TIME = 60000.f;

void CTaskSimpleFightingControl::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleFightingControl, 0x86d700, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61DC10);
    RH_ScopedInstall(Destructor, 0x61DCA0);

    RH_ScopedInstall(CalcMoveCommand, 0x624B50);

    RH_ScopedVMTInstall(Clone, 0x622EB0);
    RH_ScopedVMTInstall(GetTaskType, 0x61DC90);
    RH_ScopedVMTInstall(MakeAbortable, 0x61DD00);
    RH_ScopedVMTInstall(ProcessPed, 0x62A0A0);
}

// 0x61DC10
CTaskSimpleFightingControl::CTaskSimpleFightingControl(CEntity* entityToFight, float angleRad, float attackRange) :
    m_target{entityToFight},
    m_angleRad{angleRad},
    m_maxAttackRange{ attackRange }
{
    CEntity::SafeRegisterRef(m_target);
}

// NOTSA
CTaskSimpleFightingControl::CTaskSimpleFightingControl(const CTaskSimpleFightingControl& o) :
    CTaskSimpleFightingControl{o.m_target, o.m_angleRad, o.m_maxAttackRange }
{
}

// 0x61DCA0
CTaskSimpleFightingControl::~CTaskSimpleFightingControl() {
    CEntity::SafeCleanUpRef(m_target);
}

// 0x624B50
int16 CTaskSimpleFightingControl::CalcMoveCommand(CPed* ped) {
    if (!m_target) {
        return -1;
    }

    const auto& targetPos = m_target->GetPosition();
    const auto& pedPos    = ped->GetPosition();

    const auto segmentPedToTarget = targetPos - pedPos;

    // Check if target is outside of the attacking angle
    if (std::abs(CGeneral::LimitRadianAngle(segmentPedToTarget.Heading()) - ped->m_fCurrentRotation) >= FIGHT_CTRL_MAX_ATTACK_ANGLE_RAD) {
        m_nextAttackTime += (uint32)CTimer::GetTimeStepInMS();
        return 0;
    }

    switch (m_target->GetType()) {
    case ENTITY_TYPE_PED: {
        const auto range = segmentPedToTarget.Magnitude() - m_maxAttackRange;
        if (range > 0.1f) {
            return 3;
        }

        const auto rnd = rand();

        if (range > -0.1f) { // Inverted
            return (rnd % 16 == 0) ? 7 : -1;
        }

        if (range >= 0.8f) {
            if (rand() % 64 == 0) {
                return 8;
            }
            if (rand() % 64 == 0) {
                return 10;
            }
        } else if (rnd % 16 == 0) {
            return 9;
        }
     
        break;
    }
    case ENTITY_TYPE_VEHICLE: {
        CVector planes[4]{};
        float planesDot[4]{};
        CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll(targetPos.z, *m_target, &planes, planesDot);

        const auto hitSide = CPedGeometryAnalyser::ComputeEntityHitSide(pedPos, *m_target);
        if ((DotProduct(planes[hitSide], pedPos) + planesDot[hitSide]) >= 0.7f) {
            return 3;
        }

        break;
    }
    default: {
        if (segmentPedToTarget.Magnitude() - m_maxAttackRange > 0.3f) {
            return 3;
        }
        break;
    }
    }

    return -1;
}

// 0x61DD00
bool CTaskSimpleFightingControl::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_LEISURE:
        return false;
    case ABORT_PRIORITY_IMMEDIATE:
        return true;
    case ABORT_PRIORITY_URGENT: {
        if (event) {
            if (event->GetEventPriority() < 22) {
                return false;
            }

            switch (event->GetEventType()) {
            case EVENT_PED_COLLISION_WITH_PED:
            case EVENT_PED_COLLISION_WITH_PLAYER:
            case EVENT_PLAYER_COLLISION_WITH_PED: {
                return false;
            }
            case EVENT_VEHICLE_COLLISION:
            case EVENT_OBJECT_COLLISION:
            case EVENT_BUILDING_COLLISION: {
                if (!m_target) {
                    return true;
                }

                // This is not how they did it, but I'd like to avoid using blackmagic.
                // Once we reverse all stuff related to these events we should make a common base class
                // like `CEventEntityCollision` or something similar.
                const auto impactNormal = [event] {
                    switch (event->GetEventType()) {
                    case EVENT_VEHICLE_COLLISION:
                        return static_cast<const CEventVehicleCollision*>(event)->m_impactNormal;
                    case EVENT_OBJECT_COLLISION:
                        return static_cast<const CEventObjectCollision*>(event)->m_impactNormal;
                    case EVENT_BUILDING_COLLISION:
                        return static_cast<const CEventBuildingCollision*>(event)->m_impactNormal;
                    default:
                        NOTSA_UNREACHABLE();
                    }
                }();

                // Check if vector are opposing each other with 45deg tolerance
                if (-DotProduct(Normalized(m_target->GetPosition() - ped->GetPosition()), impactNormal) < SQRT_2 / 2) {
                    return false; // They aren't
                }
            }
            }
        }

        if (ped->GetIntelligence()->GetTaskFighting()) {
            return ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)->MakeAbortable(ped, priority, event);
        }

        return true;
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x62A0A0
bool CTaskSimpleFightingControl::ProcessPed(CPed* ped) {
    if (!m_target || m_bool) {
        return true;
    }

    const auto pedShootingRate = [&] {
        const auto rate = static_cast<float>(ped->m_nWeaponShootingRate);
        if (ped->m_nCreatedBy != PED_MISSION && ped->m_nPedType != PED_TYPE_COP && ped->m_nWeaponShootingRate == 40) {
            return static_cast<float>(ped->m_pStats->m_wShootingRate);
        }
        return rate;
    }();

    ped->GiveWeaponAtStartOfFight();

    // Create fight task for ped if not already
    auto* pedFightTask = static_cast<CTaskSimpleFight*>(ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK));
    if (!pedFightTask) {
        pedFightTask = new CTaskSimpleFight{ m_target, false, static_cast<uint32>(FIGHT_CTRL_FIGHT_IDLE_TIME) };
        ped->GetTaskManager().SetTaskSecondary(pedFightTask, TASK_SECONDARY_ATTACK);
        m_nextAttackTime = 0;
    } else if (pedFightTask->GetTaskType() != TASK_SIMPLE_FIGHT) {
        pedFightTask->MakeAbortable(ped);
        return false;
    }

    uint8 moveCmd = 0;
    if (m_nextAttackTime <= CTimer::GetTimeInMS()) {
        m_nextAttackTime = 0;
        if (ped->m_nFightState == 4 || ped->GetActiveWeapon().m_nType == WEAPON_UNARMED) {
            moveCmd = 11;
        } else {
            moveCmd = 12;
        }
    } else {
        if (m_target->m_nStatus == STATUS_PLAYER && m_someTime == 0) {
            const auto randVal = CGeneral::GetRandomNumberInRange(0.0f, 100.0f);
            if (randVal <= 0.01f * pedShootingRate * 2.0f) {
                m_someTime = CGeneral::GetRandomNumberInRange<uint32>(500, 2000);
                moveCmd = 2;
            }
        } else if (m_someTime > 0) {
            const auto stepMs = static_cast<uint32>(CTimer::GetTimeStepInMS());
            if (m_someTime > stepMs) {
                m_someTime -= stepMs;
            } else {
                m_someTime = 0;
            }
            moveCmd = 2;
        }
    }

    const auto comboIdx = std::max(0, static_cast<int32>(pedFightTask->m_nComboSet) - 4);
    m_maxAttackRange = CTaskSimpleFight::m_aComboData[comboIdx].m_fRanges;

    if (m_nextAttackTime == 0) {
        const auto randMult = CGeneral::GetRandomNumberInRange(0.5f, 1.5f);
        const auto divisor = 0.1f + pedShootingRate * m_unk3 * 0.0001f;
        m_nextAttackTime = CTimer::GetTimeInMS() + static_cast<uint32>((randMult / divisor) * 2000.0f);
    }

    if (pedFightTask->m_nComboSet <= 1) {
        const auto calcCmd = CalcMoveCommand(ped);
        if (calcCmd > 0) {
            moveCmd = static_cast<uint8>(calcCmd);
        }
    }

    pedFightTask->ControlFight(m_target, moveCmd);
    return false;
}
