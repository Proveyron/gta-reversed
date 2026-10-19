#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>

#include "./PedGroupDefaultTaskAllocator.h"
#include "PedGroup.h"
#include "PedGroupMembership.h"
#include "PedGroupIntelligence.h"
#include "TaskComplexFollowLeaderInFormation.h"
#include "TaskComplexGangFollower.h"
#include "TaskComplexGangLeader.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorRandom final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6E90
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        const auto leader = pedGroup->GetMembership().GetLeader();
        for (auto&& [i, tp] : rngv::enumerate(pedGroup->GetIntelligence().GetDefaultPedTaskPairs())) {
            if (!tp.Ped) {
                continue;
            }
            if (ped && tp.Ped != ped) {
                continue;
            }
            if (i < CPedGroupMembership::LEADER_MEM_ID) {
                auto* task = new CTaskComplexGangFollower{
                    pedGroup,
                    leader,
                    static_cast<uint8>(i),
                    CVector{ CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets[i], 0.0f },
                    10.0f
                };
                task->SetFollowerFollowsInCar(pedGroup->m_bMembersEnterLeadersVehicle);
                tp.Task = task;
            } else {
                tp.Task = new CTaskComplexGangLeader{ pedGroup };
            }
        }
    }

    // 0x5F6530
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::RANDOM; };

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorRandom, 0x86C77C, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F6E90);
        RH_ScopedVMTInstall(GetType, 0x5F6530);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorRandom, sizeof(void*)); /* vtable only */
