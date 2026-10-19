#include "StdInc.h"

#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleDuck.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimplePause.h"
#include "TaskSimpleThrowControl.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "Cover.h"
#include "PedGroups.h"
#include "World.h"
#include <extensions/utility.hpp>

void CTaskComplexKillPedOnFootArmed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedOnFootArmed, 0x86d918, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x621190);
    RH_ScopedInstall(Destructor, 0x621250);

    RH_ScopedInstall(LineOfSightClearForAttack, 0x621500);
    RH_ScopedInstall(IsPedInLeaderFiringLine, 0x621300);
    RH_ScopedInstall(CreateSubTask, 0x626FC0);

    RH_ScopedVMTInstall(Clone, 0x6234C0);
    RH_ScopedVMTInstall(GetTaskType, 0x621240);
    RH_ScopedVMTInstall(MakeAbortable, 0x6212B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x62C190);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x62BF00);
    RH_ScopedVMTInstall(ControlSubTask, 0x62CCE0);
}

// 0x621190
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(
    CPed*  target,
    uint32 duckingConditions,
    uint32 duckTime,
    uint32 duckChancePerc,
    int8   competence
) :
    m_target{ target },
    m_duckingConditions{ duckingConditions },
    m_lengthOfDuck{ duckTime },
    m_duckChancePerc{ duckChancePerc },
    m_competence{ competence }
{
    CEntity::SafeRegisterRef(m_target);
}

// notsa
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(const CTaskComplexKillPedOnFootArmed& o) :
    CTaskComplexKillPedOnFootArmed{
        o.m_target,
        o.m_duckingConditions,
        o.m_lengthOfDuck,
        o.m_duckChancePerc,
        o.m_competence
    }
{
    m_aimImmediate = o.m_aimImmediate;
}

// 0x621250
CTaskComplexKillPedOnFootArmed::~CTaskComplexKillPedOnFootArmed() {
    CEntity::SafeCleanUpRef(m_target);
}

// 0x621500
bool CTaskComplexKillPedOnFootArmed::LineOfSightClearForAttack(CPed* ped) { // ped is the task owner ped
    /*
    * TODO:
    * here's some code to calculate the values to use (based on some variables)
    * nothing special, but it's messy, and it's late, so I won't bother
    */
    const auto reqDistDeltaSq = sq(2.f);
    const auto reqTimeDelta   = 5000;

    //> If the LOS was recently clear, let's consider it still is
    if (CTimer::GetTimeInMS() - m_losClearTime < reqTimeDelta) {
        return true;
    }

    //> Perhaps check if the entities are still kinda around the same position as the last time the LOS was blocked...
    if (CTimer::GetTimeInMS() - m_losBlockedTime < reqTimeDelta) {
        if (reqDistDeltaSq >= (m_target->GetPosition() - m_losBlockedTargetPos).SquaredMagnitude()) {
            if (reqDistDeltaSq >= (ped->GetPosition() - m_losBlockedOurPos).SquaredMagnitude()) {
                return false;
            }
        }
    }

    //> 0x6216DD
    // Temporarily disable the target ped vehicle's collision (To ignore it)
    // Perhaps, `CWorld::pIgnoreEntity` could be used?
    const auto targetVeh = m_target->GetVehicleIfInOne();
    const notsa::ScopeGuard restore{[targetVeh, had = targetVeh && targetVeh->GetUsesCollision()] {
        if (targetVeh) {
            targetVeh->SetUsesCollision(had);
        }
    }};

    if (targetVeh) {
        targetVeh->SetUsesCollision(false);
    }

    const auto GetPedHeadPos = [](CPed* headOf) {
        CVector inout{ 0.1f, 0.f, 0.f };
        headOf->GetTransformedBonePosition(inout, BONE_HEAD);
        return inout;
    };

    if (CWorld::GetIsLineOfSightClear(
        ped->bIsDucking
            ? GetPedHeadPos(ped)
            : ped->GetPosition() + CVector{0.f, 0.f, 0.25f},
        GetPedHeadPos(m_target), // Always use head position
        true,
        true,
        false,
        true,
        false,
        true,
        false
    )) {
        m_losClearTime   = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return true;
    } else {
        m_losBlockedTime      = CTimer::GetTimeInMS();
        m_losClearTime        = 0;
        m_losBlockedOurPos    = ped->GetPosition();
        m_losBlockedTargetPos = m_target->GetPosition();
        return false;
    }
}

// 0x621300
bool CTaskComplexKillPedOnFootArmed::IsPedInLeaderFiringLine(CPed* ped) {
    const auto pedGrp = ped->GetGroup();
    if (!pedGrp) {
        return false;
    }

    const auto grpLeaderPlyr = pedGrp->GetMembership().GetLeader();
    if (!grpLeaderPlyr || !grpLeaderPlyr->IsPlayer()) {
        return false;
    }

    if (!grpLeaderPlyr->m_pTargetedObject || grpLeaderPlyr->GetActiveWeapon().IsTypeMelee()) {
        return false;
    }

    const auto &leaderPos2D      = grpLeaderPlyr->GetPosition2D();
    const auto &leaderPos        = grpLeaderPlyr->GetPosition();
    const auto leaderToPed       = ped->GetPosition() - leaderPos, // 0x6213BD
               leaderToTargetDir = (grpLeaderPlyr->m_pTargetedObject->GetPosition() - leaderPos).Normalized(); // 0x621394

    /* clang-format off
     * --[projPointOnLeaderToTargetRay2D]-->[leaderPos]---[leaderToTargetDir]-->[Target]
     *                                          /
     *                                         /
     *                                        /
     *                                       /
     *                                 [leaderToPed]
     *                                     /
     *                                    /
     *                                  \|/
     *                                [Ped]
     * clang-format on */

    //> 0x6213AE
    const auto projPointOnLeaderToTargetRay2D = leaderPos + CVector2D{ leaderToPed }.ProjectOnToNormal(leaderToTargetDir);
    if ((projPointOnLeaderToTargetRay2D - ped->GetPosition2D()).SquaredMagnitude() >= sq(2.f)) {
        return false;
    }

    //> 0x0621482
    if (leaderToTargetDir.Dot(leaderToPed) <= 0.f) { // Ped is "behind" leader (Like on the ASCII art above)
        return false;
    }

    //> 0x6214CD
    if (leaderToPed.SquaredMagnitude() >= sq(10.f)) {
        return false;
    }

    return true;
}

// 0x626FC0
CTask* CTaskComplexKillPedOnFootArmed::CreateSubTask(eTaskType taskType, CPed* ped) {
    const auto& wepInfo = ped->GetActiveWeapon().GetWeaponInfo(ped);

    switch (taskType) {
    case TASK_SIMPLE_DUCK: {
        return new CTaskSimpleDuck(DUCK_TASK_CONTROLLED, static_cast<uint16>(m_lengthOfDuck), -1);
    }
    case TASK_SIMPLE_STAND_STILL: {
        return new CTaskSimpleStandStill(20000, false, false, 8.0f);
    }
    case TASK_SIMPLE_PAUSE: {
        CTaskSimpleStandStill standStill(0, false, false, 8.0f);
        standStill.ProcessPed(ped);
        return new CTaskSimplePause(100);
    }
    case TASK_COMPLEX_SEEK_ENTITY: {
        CTask* subTask = nullptr;
        if (m_needToMoveInCloserTime != 0) {
            float timeDiff = static_cast<float>(CTimer::GetTimeInMS() - m_needToMoveInCloserTime);
            if (timeDiff <= 3000.0f) {
                subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, 6.0f, 2.0f, 2.0f, true, true);
            } else if (timeDiff > 8000.0f) {
                CVector pedPos = ped->GetPosition();
                CVector targetPos = m_target->GetPosition();
                CVector dir = targetPos - pedPos;
                dir.Normalise();
                CVector perp{-dir.y * 1.5f, dir.x * 1.5f, 0.0f};

                CVector perp1 = targetPos + perp;
                CVector perp2 = targetPos - perp;

                if (CWorld::GetIsLineOfSightClear(targetPos, perp1, true, true, false, false, true, false, false)) {
                    subTask = new CTaskComplexGoToPointAndStandStill(PEDMOVE_RUN, perp1, 0.5f, 2.0f, false, false);
                } else if (CWorld::GetIsLineOfSightClear(targetPos, perp2, true, true, false, false, true, false, false)) {
                    subTask = new CTaskComplexGoToPointAndStandStill(PEDMOVE_RUN, perp2, 0.5f, 2.0f, false, false);
                } else {
                    subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, 1.0f, 2.0f, 2.0f, true, true);
                }
            } else {
                float targetRange = 6.0f - (timeDiff - 3000.0f) * 0.001f;
                subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, targetRange, 2.0f, 2.0f, true, true);
            }
        } else {
            subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, 6.0f, 2.0f, 2.0f, true, true);
        }

        if (m_target && m_target->IsPlayer()) {
            if (ped->m_nPedType == PED_TYPE_COP) {
                if (auto wanted = FindPlayerWanted(-1)) {
                    if (wanted->m_nWantedLevel > 1) {
                        if (ped->Say(CTX_GLOBAL_CHASE_FOOT) >= 0) {
                            m_target->Say(CTX_GLOBAL_CHASED, 3500);
                        }
                    } else {
                        if (ped->Say(CTX_GLOBAL_SOLO) >= 0) {
                            m_target->Say(CTX_GLOBAL_CHASED, 3500);
                        }
                    }
                }
            } else if (CPedGroups::GetPedsGroup(ped)) {
                ped->Say(CTX_GLOBAL_CHASE_FOOT);
            }
        }
        return subTask;
    }
    case TASK_SIMPLE_GUN_CTRL: {
        if (wepInfo.flags.bThrow) {
            return new CTaskSimpleThrowControl(m_target);
        }

        bool bLosClear = LineOfSightClearForAttack(ped);
        auto* gunTask = new CTaskSimpleGunControl(m_target, CVector{}, CVector{}, bLosClear ? eGunCommand::FIREBURST : static_cast<eGunCommand>(0), 5, -1);
        gunTask->m_aimImmidiately = m_aimImmediate;
        m_aimImmediate = false;
        m_shootTimer = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(4000, 8000);
        if (CPedGroups::GetPedsGroup(ped)) {
            ped->Say(CTX_GLOBAL_SURROUNDED);
        }
        m_lastAttackTime = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return gunTask;
    }
    default:
        return nullptr;
    }
}

// 0x6212B0
bool CTaskComplexKillPedOnFootArmed::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_URGENT: {
        if (const auto aimedAtEvent = notsa::dyn_cast_if_present<const CEventGunAimedAt>(event)) {
            if (aimedAtEvent->m_AimedBy == m_target) {
                return false;
            }
        }
        break;
    }
    case ABORT_PRIORITY_IMMEDIATE:
        break;
    case ABORT_PRIORITY_LEISURE:
    default:
        return false;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x62C190
CTask* CTaskComplexKillPedOnFootArmed::CreateNextSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    bool targetArmedWithGun = !m_target->GetActiveWeapon().IsTypeMelee();
    const auto& ourActiveWep = ped->GetActiveWeapon().GetWeaponInfo(ped);
    float targetToOurPedDist = (m_target->GetPosition() - ped->GetPosition()).Magnitude();

    CTask* subTask = nullptr;

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_DUCK: {
        ped->bIsDucking = false;
        m_needToMoveInCloserTime = 0;
        return CreateSubTask(LineOfSightClearForAttack(ped) ? TASK_SIMPLE_GUN_CTRL : TASK_SIMPLE_PAUSE, ped);
    }
    case TASK_SIMPLE_PAUSE:
    case TASK_SIMPLE_STAND_STILL: {
        if (!ped->bStayInSamePlace && !ped->bKindaStayInSamePlace) {
            ped->Say(CTX_GLOBAL_MOVE_IN);
            subTask = CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
            if (!m_needToMoveInCloserTime) {
                m_needToMoveInCloserTime = CTimer::GetTimeInMS();
            }
            return subTask;
        }

        if (targetToOurPedDist <= (ourActiveWep.m_fTargetRange / 4.0f) && LineOfSightClearForAttack(ped)) {
            subTask = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        } else {
            subTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        }
        m_needToMoveInCloserTime = 0;
        return subTask;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_COMPLEX_USE_COVER_POINT:
    case TASK_COMPLEX_SEEK_ENTITY: {
        if (LineOfSightClearForAttack(ped)) {
            subTask = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        } else {
            subTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        }
        m_needToMoveInCloserTime = 0;
        return subTask;
    }
    case TASK_SIMPLE_GUN_CTRL: {
        if (!ped->bStayInSamePlace) {
            if (targetToOurPedDist < 3.0f) {
                subTask = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
            } else {
                float maxRange = std::max(ourActiveWep.m_fTargetRange * 0.8f, 23.0f);
                if (targetToOurPedDist <= maxRange) {
                    if (ped->bKindaStayInSamePlace) {
                        subTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                        m_lastAttackTime = CTimer::GetTimeInMS();
                    } else {
                        float seekRange = std::max(ourActiveWep.m_fTargetRange * 0.6f, 20.0f);
                        subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, seekRange, 2.0f, 2.0f, true, true);
                    }
                } else {
                    auto* gunTask = static_cast<CTaskSimpleGunControl*>(m_pSubTask);
                    if (gunTask->m_aimImmidiately && !ped->bStayInSamePlace && !ped->bKindaStayInSamePlace) {
                        ped->Say(CTX_GLOBAL_MOVE_IN);
                        subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, 2.0f, 2.0f, 2.0f, true, true);
                    } else if (targetArmedWithGun && ped->m_pCoverPoint) {
                        if (CCover::DoesCoverPointStillProvideCover(ped->m_pCoverPoint, m_target->GetPosition())) {
                            CVector coverPos{};
                            if (CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, m_target->GetPosition(), coverPos)) {
                                if ((coverPos - ped->GetPosition()).Magnitude2D() <= 0.75f) {
                                    if (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::LOWCOVER) {
                                        if (targetToOurPedDist > 12.0f && !ped->bIsDucking && CGeneral::GetRandomNumberInRange(0, 4) == 0 && !ped->GetIntelligence()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK)) {
                                            ped->Say(CTX_GLOBAL_DUCK);
                                            subTask = new CTaskSimpleDuck(DUCK_TASK_CONTROLLED, 2500, -1);
                                        }
                                    } else {
                                        if (CGeneral::GetRandomNumberInRange(0, 2) == 0 && !ourActiveWep.flags.bThrow) {
                                            auto* newGunTask = new CTaskSimpleGunControl(m_target, CVector{}, CVector{}, eGunCommand::FIREBURST, 5, -1);
                                            newGunTask->m_aimImmidiately = m_aimImmediate;
                                            m_aimImmediate = false;
                                            m_shootTimer = CTimer::GetTimeInMS() + 5000;
                                            m_lastAttackTime = CTimer::GetTimeInMS();
                                            m_lastStrafeTime = CTimer::GetTimeInMS() + 2500;
                                            m_strafeDir = (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::WALLTOLEFT) ? eStrafeDir::LEFT : eStrafeDir::RIGHT;
                                            m_bStrafeBack = true;
                                            ped->GetIntelligence()->ClearTaskDuckSecondary();
                                            subTask = newGunTask;
                                        }
                                    }
                                } else {
                                    ped->ReleaseCoverPoint();
                                }
                            } else {
                                ped->ReleaseCoverPoint();
                            }
                        } else {
                            ped->ReleaseCoverPoint();
                        }
                    }

                    if (!subTask && m_competence > 0 && targetToOurPedDist >= 6.0f && targetArmedWithGun && (CGeneral::GetRandomNumberInRange(0, 2) != 0)) {
                        ped->ReleaseCoverPoint();
                        ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, m_target->GetPosition(), m_competence == 2);
                        if (ped->m_pCoverPoint) {
                            CVector coverPos{};
                            CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, m_target->GetPosition(), coverPos);
                            coverPos.z += 1.0f;
                            if ((coverPos - m_target->GetPosition()).Magnitude2D() <= ourActiveWep.m_fTargetRange * 0.75f) {
                                if (CWorld::GetIsLineOfSightClear(coverPos, ped->GetPosition(), true, true, false, false, false, false, false)) {
                                    if (CPedGroups::GetPedsGroup(ped)) {
                                        ped->Say(CTX_GLOBAL_COVER_ME);
                                    }
                                    ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                                    subTask = new CTaskSimpleGoToPoint(PEDMOVE_RUN, coverPos, 0.5f, true, false);
                                }
                            }
                            if (!subTask) {
                                ped->ReleaseCoverPoint();
                            }
                        }
                    }

                    if (!subTask) {
                        if (targetToOurPedDist <= 10.0f) {
                            int32 rnd = CGeneral::GetRandomNumberInRange(0, 4);
                            if (rnd == 0 && !ped->bKindaStayInSamePlace) {
                                ped->Say(CTX_GLOBAL_MOVE_IN);
                                subTask = new CTaskComplexSeekEntityStandard(m_target, 50000, 1000, targetToOurPedDist - 4.0f, 2.0f, 2.0f, true, true);
                            } else if (rnd == 1 && !ped->bKindaStayInSamePlace) {
                                m_strafeDir = eStrafeDir::FORWARD;
                                m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                                m_bStrafeBack = false;
                            }
                        }

                        if (targetToOurPedDist <= 5.0f && CGeneral::GetRandomNumberInRange(0, 4) == 0) {
                            m_strafeDir = (CGeneral::GetRandomNumberInRange(0, 2) != 0) ? eStrafeDir::RIGHT : eStrafeDir::LEFT;
                            CVector right = ped->GetRight();
                            CVector strafeTarget = (m_strafeDir == eStrafeDir::RIGHT) ? (ped->GetPosition() + right * 2.5f) : (ped->GetPosition() - right * 2.5f);
                            if (!CWorld::GetIsLineOfSightClear(ped->GetPosition(), strafeTarget, true, true, false, true, false, false, false)) {
                                m_strafeDir = (m_strafeDir == eStrafeDir::RIGHT) ? eStrafeDir::LEFT : eStrafeDir::RIGHT;
                            }
                            m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                            m_bStrafeBack = false;
                        }

                        if (!subTask) {
                            if (LineOfSightClearForAttack(ped)) {
                                subTask = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
                            } else {
                                if (CGeneral::GetRandomNumberInRange(0, 4) == 0) {
                                    m_strafeDir = (CGeneral::GetRandomNumberInRange(0, 32768) < 0x3fff) ? eStrafeDir::RIGHT : eStrafeDir::LEFT;
                                }
                                subTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
                                m_lastAttackTime = CTimer::GetTimeInMS();
                            }
                        }
                    }
                }
            }
        } else {
            subTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
            m_lastAttackTime = CTimer::GetTimeInMS();
        }

        if (m_competence > 0 && !ped->bIsDucking && !ped->m_pCoverPoint) {
            ped->GetIntelligence()->SetTaskDuckSecondary(6000);
        }

        m_needToMoveInCloserTime = 0;
        return subTask;
    }
    case TASK_SIMPLE_THROW_CTRL: {
        m_needToMoveInCloserTime = 0;
        return nullptr;
    }
    default:
        m_needToMoveInCloserTime = 0;
        return nullptr;
    }
}

// 0x62BF00
CTask* CTaskComplexKillPedOnFootArmed::CreateFirstSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    if (   !ped->bStayInSamePlace
        && CGeneral::RandomBool(50)
        && m_competence > 0
        && (targetToOurPedDistSq >= sq(30.f) || targetToOurPedDistSq >= sq(6.f) && !m_target->GetActiveWeapon().IsTypeMelee()) 
    ) {
        ped->ReleaseCoverPoint();
        if (ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, targetPos, false)) {
            CVector coverPos{};
            if (!notsa::IsFixBugs() || CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, targetPos, coverPos)) {
                if (CWorld::GetIsLineOfSightClear(coverPos, ourPos, true, true, false, false, false, false, false)) {
                    ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                    if (const auto task = new CTaskSimpleGoToPoint{
                        PEDMOVE_RUN,
                        coverPos,
                        0.5f,
                        true
                    }) { // I can't believe my eyes.... error handling?
                        return task;
                    }
                } else {
                    ped->ReleaseCoverPoint();
                }
            } else {
                NOTSA_LOG_WARN("Can't find cover point's coordinates!"); // Originally the game has left `coverPos` uninitialized in this case
            }
        }
    }

    return CreateSubTask([&, this] {
        const auto& ourActiveWep = ped->GetActiveWeapon().GetWeaponInfo(ped);

        if (targetToOurPedDistSq <= sq(ourActiveWep.m_fTargetRange / 4.f) && LineOfSightClearForAttack(ped)) {
            return TASK_SIMPLE_GUN_CTRL;
        }

        if (!ped->bStayInSamePlace && !ped->bKindaStayInSamePlace) {
            return TASK_COMPLEX_SEEK_ENTITY;
        }

        if (LineOfSightClearForAttack(ped) || ourActiveWep.flags.bThrow) {
            return TASK_SIMPLE_PAUSE;
        }

        return TASK_SIMPLE_GUN_CTRL;
    }(), ped);
}

// 0x62CCE0
CTask* CTaskComplexKillPedOnFootArmed::ControlSubTask(CPed* ped) {
    if (m_newTarget) {
        return CreateFirstSubTask(ped);
    }

    if (!m_target || m_target->m_fHealth <= 0.f) {
        return nullptr;
    }
    const auto ogSubTask = m_pSubTask;

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    //> 0x62CD55
    if (   (m_duckingConditions & 4) == 0
        || CTimer::GetTimeInMS() <= m_lastDuckTime + m_lengthOfDuck + 2000
        || ped->bIsDucking
    ) {
        if (   m_duckingConditions & 1
            && m_bShotFiredByPlayer
            && CGeneral::RandomBool((float)m_duckChancePerc)
        ) {
            if (const auto quack = ped->GetIntelligence()->GetTaskDuck()) {
                quack->SetDuckTimer((uint16)m_lengthOfDuck);
            } else {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
#ifdef FIX_BUGS
                m_lastDuckTime = CTimer::GetTimeInMS(); // Not actually sure if this is necessary tbh
#endif
            }
        }
    } else {
        if (CGeneral::RandomBool((float)m_duckChancePerc)) {
            if (targetToOurPedDistSq <= sq(20.f)) {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
            }
        }
        m_lastDuckTime = CTimer::GetTimeInMS();
    }

    //> 0x62CE56
    const auto quack = ped->GetIntelligence()->GetTaskDuck();
    if (quack) {
        quack->m_bIsInControl = true;
    }

    //> 0x62CE6E
    if (m_pSubTask != ogSubTask) { // ?????
        return ogSubTask;
    }

    const auto& ourwi = ped->GetActiveWeapon().GetWeaponInfo(ped);
    switch (m_pSubTask->GetTaskType()) { //> 0x62CE7E
    case TASK_COMPLEX_SEEK_ENTITY: {
        if ([&, this]{
            if (IsPedInLeaderFiringLine(ped)) {
                return true;
            }
            if (m_target->physicalFlags.bSubmergedInWater && LineOfSightClearForAttack(ped)) {
                return true;
            }
            if (ped->bStayInSamePlace) {
                return LineOfSightClearForAttack(ped);
            } else if (sq(ourwi.m_fTargetRange / 2.f) >= targetToOurPedDistSq) {
                if (CTimer::GetTimeInMS() - m_lastAttackTime >= 2000 || m_target->GetMoveSpeed().Dot(ped->GetForward()) < 0.f) {
                    return LineOfSightClearForAttack(ped);
                }
            }
            return false;
        }()) {
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        } else {
            if (ped->GetGroup()) {
                ped->Say(CTX_GLOBAL_COVER_ME);
            }
        }
        break;
    }
    case TASK_SIMPLE_GUN_CTRL: { //> 0x62CE87
        if (   targetToOurPedDistSq >= sq(ourwi.m_fTargetRange)
            || targetToOurPedDistSq >= sq(ourwi.m_fTargetRange / 2.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() - m_lastAttackTime >= 2000
            || targetToOurPedDistSq >= sq(4.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() >= m_shootTimer
            || !LineOfSightClearForAttack(ped)
        ) {
            const auto gctrl = notsa::cast<CTaskSimpleGunControl>(m_pSubTask);
            if (gctrl->m_firingTask != eGunCommand::END_LEISURE) {
                gctrl->m_nextAtkTimeMs = 0;
                gctrl->m_firingTask    = eGunCommand::END_LEISURE;
            }
        }

        const auto ugun = ped->GetIntelligence()->GetTaskUseGun();
        if (!ugun) {
            break;
        }

        if (ped->GetGroup()) {
            ped->Say(CTX_GLOBAL_SURROUNDED);
        }

        //> 0x62CF88
        const auto actionDir = [&, this]() -> CVector2D {
            if (m_target->physicalFlags.bSubmergedInWater) {
                return { 0.f, 0.f };
            }
            if (CTimer::GetTimeInMS() >= m_lastStrafeTime || !ped->bStayInSamePlace) {
                if (targetToOurPedDistSq <= sq(4.f) && ped->bStayInSamePlace) {
                    return { 0.f, 1.f }; // Forwards
                }
                if (m_bStrafeBack) {
                    m_bStrafeBack    = false;
                    m_lastStrafeTime = CTimer::GetTimeInMS() + 2500;
                    m_strafeDir      = [&, this] { // Strafe back to where we came from the last time
                        switch (m_strafeDir)
                        {
                        case eStrafeDir::LEFT:    return eStrafeDir::RIGHT;
                        case eStrafeDir::RIGHT:   return eStrafeDir::LEFT;
                        case eStrafeDir::FORWARD: return eStrafeDir::BACK;
                        case eStrafeDir::BACK:    return eStrafeDir::FORWARD;
                        default:                  NOTSA_UNREACHABLE();
                        }
                    }();
                }
                return { 0.f, 0.f };
            } else {
                switch (m_strafeDir)
                {
                case eStrafeDir::LEFT:    return { -1.f,  0.f };
                case eStrafeDir::RIGHT:   return {  1.f,  0.f };
                case eStrafeDir::FORWARD: return {  0.f, -1.f };
                case eStrafeDir::BACK:    return {  0.f,  1.f };
                default:                  NOTSA_UNREACHABLE();
                }
            }
        }();

        //> 0x62D080
        if (quack) {
            quack->ControlDuckMove([&, this] {
                if (notsa::contains(std::to_array({ eStrafeDir::LEFT, eStrafeDir::RIGHT }), m_strafeDir)) {
                    if (m_lastRollTime + 3000 >= CTimer::GetTimeInMS()) {
                        return CVector2D{ 0.f, 0.f };
                    } else {
                        m_lastRollTime = CTimer::GetTimeInMS();
                    }
                }
                return actionDir;
            }());
        } else {
            ugun->ControlGunMove(actionDir);
        }

        break;
    }
    }

    return m_pSubTask;
}
