#include "StdInc.h"
#include "TaskComplexUseClosestFreeScriptedAttractor.h"
#include "TaskComplexUseEffect.h"
#include "TaskComplexUseEffectRunning.h"
#include "TaskComplexUseEffectSprinting.h"
#include "Scripted2dEffects.h"
#include "PedAttractorManager.h"
#include "InterestingEvents.h"

void CTaskComplexUseClosestFreeScriptedAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexUseClosestFreeScriptedAttractor, 0x86e428, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6346F0);
    RH_ScopedInstall(Destructor, 0x634720);

    RH_ScopedGlobalInstall(ComputeClosestFreeScriptedEffect, 0x634740);

    RH_ScopedVMTInstall(Clone, 0x636F70);
    RH_ScopedVMTInstall(GetTaskType, 0x634710);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x634730);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x639530);
    RH_ScopedVMTInstall(ControlSubTask, 0x634890);
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(eMoveState ms) :
    m_MoveState{ms}
{
}

CTaskComplexUseClosestFreeScriptedAttractor::CTaskComplexUseClosestFreeScriptedAttractor(const CTaskComplexUseClosestFreeScriptedAttractor&) :
    CTaskComplexUseClosestFreeScriptedAttractor{}
{
}

// 0x634740
C2dEffect* CTaskComplexUseClosestFreeScriptedAttractor::ComputeClosestFreeScriptedEffect(CPed const& ped) {
    float closestDistSq = std::numeric_limits<float>::max();
    C2dEffect* closestEffect = nullptr;

    for (size_t i = 0; i < NUM_SCRIPTED_2D_EFFECTS; ++i) {
        if (!CScripted2dEffects::ms_activated[i]) {
            continue;
        }

        const auto& userList = CScripted2dEffects::ms_userLists[i];
        if (userList.m_bUseList) {
            bool matches = false;
            for (int32 j = 0; j < 4; ++j) {
                if (userList.m_UserTypes[j] == ped.m_nModelIndex) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                for (int32 j = 0; j < 4; ++j) {
                    if (userList.m_UserTypes[j] == -2 && userList.m_UserTypesByPedType[j] == ped.m_nPedType) {
                        matches = true;
                        break;
                    }
                }
            }
            if (!matches) {
                continue;
            }
        }

        auto& effect = CScripted2dEffects::ms_effects[i];
        float distSq = (ped.GetPosition() - effect.m_pos).SquaredMagnitude();
        if (distSq < closestDistSq) {
            if (GetPedAttractorManager()->HasEmptySlot(&effect, nullptr)) {
                closestDistSq = distSq;
                closestEffect = &effect;
            }
        }
    }
    return closestEffect;
}

// 0x639530
CTask* CTaskComplexUseClosestFreeScriptedAttractor::CreateFirstSubTask(CPed* ped) {
    C2dEffect* effect = ComputeClosestFreeScriptedEffect(*ped);
    if (!effect) {
        return nullptr;
    }

    g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_3, ped);

    auto* attractor = reinterpret_cast<C2dEffectPedAttractor*>(effect);
    switch (m_MoveState) {
    case PEDMOVE_RUN:
        return new CTaskComplexUseEffectRunning(attractor, nullptr);
    case PEDMOVE_SPRINT:
        return new CTaskComplexUseEffectSprinting(attractor, nullptr);
    case PEDMOVE_WALK:
    default:
        return new CTaskComplexUseEffect(attractor, nullptr);
    }
}
