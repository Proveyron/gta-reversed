#include "StdInc.h"
#include "PedGroupPlacer.h"
#include "PedGroups.h"
#include "PedPlacement.h"
#include "Population.h"
#include "Tasks/Allocators/PedGroup/PedGroupDefaultTaskAllocators.h"
#include "Tasks/TaskTypes/TaskComplexBeInGroup.h"
#include "Tasks/TaskTypes/TaskComplexFollowLeaderInFormation.h"
#include "Tasks/TaskTypes/TaskComplexWanderGang.h"

void CPedGroupPlacer::InjectHooks() {
    RH_ScopedClass(CPedGroupPlacer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(PlaceFormationGroup, 0x5FC9B0);
    RH_ScopedInstall(PlaceChatGroup, 0x5FCE80);
    RH_ScopedInstall(PlaceRandomGroup, 0x5FD330);
    RH_ScopedInstall(PlaceGroup, 0x5FD810);
}

// 0x5FC9B0
bool CPedGroupPlacer::PlaceFormationGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    const int32 groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    if (TheCamera.IsSphereVisible({ origin, 3.0f })) {
        if ((origin - FindPlayerCentreOfWorld(CWorld::PlayerInFocus)).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * 42.5f) {
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, 3.0f, -1, nullptr, true, true, true)) {
        return false;
    }

    bool bGroundHit = false;
    const float groundZ = CWorld::FindGroundZFor3DCoord(origin.x, origin.y, origin.z + 1.0f, &bGroundHit, nullptr);
    if (!bGroundHit) {
        return false;
    }

    CVector leaderPos = origin;
    leaderPos.z = std::max(groundZ + 1.0f, origin.z);

    const auto gangId = (eGangID)(type - PED_TYPE_GANG1);
    const eModelID leaderModel = CPopulation::ChooseGangOccupation(gangId);
    if (leaderModel == MODEL_INVALID || !CModelInfo::GetPedModelInfo(leaderModel)->GetRwObject()) {
        return false;
    }

    CPed* leader = CPopulation::AddPed(type, leaderModel, leaderPos, false);
    if (!leader) {
        return false;
    }

    auto& group = CPedGroups::ms_groups[groupId];
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(allocType));
    group.GetMembership().SetLeader(leader);
    group.GetMembership().AddMember(leader);
    group.GetIntelligence().Process();
    leader->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, true), TASK_PRIMARY_PRIMARY);
    CVisibilityPlugins::SetClumpAlpha(leader->GetRpClump(), 0);

    for (uint32 i = 1; i < numOfPeds; i++) {
        const eModelID followerModel = CPopulation::ChooseGangOccupation(gangId);
        if (followerModel == MODEL_INVALID || !CModelInfo::GetPedModelInfo(followerModel)->GetRwObject()) {
            continue;
        }

        CVector followerPos = leader->GetPosition() + CVector{ CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets[i], 0.0f };
        bool bFollowerGroundHit = false;
        const float followerGroundZ = CWorld::FindGroundZFor3DCoord(followerPos.x, followerPos.y, followerPos.z + 1.0f, &bFollowerGroundHit, nullptr);
        if (bFollowerGroundHit) {
            followerPos.z = std::max(followerGroundZ + 1.0f, leader->GetPosition().z);
        } else {
            continue;
        }

        if (!CPedPlacement::IsPositionClearForPed(followerPos, -1.0f, -1, nullptr, true, true, true)) {
            continue;
        }

        CPed* follower = CPopulation::AddPed(type, followerModel, followerPos, false);
        if (follower) {
            group.GetMembership().AddMember(follower);
            group.GetIntelligence().Process();
            follower->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, false), TASK_PRIMARY_PRIMARY);
            CVisibilityPlugins::SetClumpAlpha(follower->GetRpClump(), 0);
        }
    }

    return true;
}

// 0x5FCE80
bool CPedGroupPlacer::PlaceChatGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    const int32 groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    const float radius = std::sqrt(0.5f / (1.0f - std::cos(TWO_PI / (float)numOfPeds)));

    if (TheCamera.IsSphereVisible({ origin, radius })) {
        if ((origin - FindPlayerCentreOfWorld(CWorld::PlayerInFocus)).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * 42.5f) {
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, radius, -1, nullptr, true, true, true)) {
        return false;
    }

    const auto gangId = (eGangID)(type - PED_TYPE_GANG1);
    const float angleStep = TWO_PI / (float)numOfPeds;
    CPed* peds[TOTAL_PED_GROUP_MEMBERS]{};
    int32 createdCount = 0;

    for (uint32 i = 0; i < numOfPeds; i++) {
        const float angle = angleStep * (float)i + CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        const float r = radius + CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        CVector pedPos = origin + CVector(std::sin(angle) * r, std::cos(angle) * r, 0.0f);

        bool bGroundHit = false;
        const float groundZ = CWorld::FindGroundZFor3DCoord(pedPos.x, pedPos.y, pedPos.z + 1.0f, &bGroundHit, nullptr);
        if (!bGroundHit) {
            continue;
        }
        pedPos.z = std::max(groundZ + 1.0f, origin.z);

        if (std::abs(groundZ - origin.z) >= 1.0f) {
            continue;
        }

        const eModelID model = CPopulation::ChooseGangOccupation(gangId);
        if (model == MODEL_INVALID || !CModelInfo::GetPedModelInfo(model)->GetRwObject()) {
            continue;
        }

        CPed* ped = CPopulation::AddPed(type, model, pedPos, false);
        if (ped) {
            ped->m_fCurrentRotation = ped->m_fAimingRotation = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pedPos.x, pedPos.y);
            ped->SetHeading(ped->m_fCurrentRotation);
            CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);
            peds[createdCount++] = ped;
        }
    }

    if (createdCount < 1) {
        for (int32 i = 0; i < createdCount; i++) {
            CPopulation::RemovePed(peds[i]);
        }
        return false;
    }

    auto& group = CPedGroups::ms_groups[groupId];
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(allocType));
    group.GetMembership().SetLeader(peds[0]);
    group.GetMembership().AddMember(peds[0]);
    peds[0]->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, true), TASK_PRIMARY_PRIMARY);

    for (int32 i = 1; i < createdCount; i++) {
        group.GetMembership().AddMember(peds[i]);
        peds[i]->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, false), TASK_PRIMARY_PRIMARY);
    }
    group.GetIntelligence().Process();

    return true;
}

// 0x5FD330
bool CPedGroupPlacer::PlaceRandomGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    const int32 groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    const float radius = std::sqrt(0.5f / (1.0f - std::cos(TWO_PI / (float)numOfPeds)));

    if (TheCamera.IsSphereVisible({ origin, radius })) {
        if ((origin - FindPlayerCentreOfWorld(CWorld::PlayerInFocus)).Magnitude2D() < CPopulation::PedCreationDistMultiplier() * 42.5f) {
            return false;
        }
    }

    if (!CPedPlacement::IsPositionClearForPed(origin, radius, -1, nullptr, true, true, true)) {
        return false;
    }

    const auto gangId = (eGangID)(type - PED_TYPE_GANG1);
    const float angleStep = TWO_PI / (float)numOfPeds;
    CPed* peds[TOTAL_PED_GROUP_MEMBERS]{};
    int32 createdCount = 0;

    for (uint32 i = 0; i < numOfPeds; i++) {
        const float angle = angleStep * (float)i + CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        const float r = radius + CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        CVector pedPos = origin + CVector(std::sin(angle) * r, std::cos(angle) * r, 0.0f);

        bool bGroundHit = false;
        const float groundZ = CWorld::FindGroundZFor3DCoord(pedPos.x, pedPos.y, pedPos.z + 1.0f, &bGroundHit, nullptr);
        if (!bGroundHit) {
            continue;
        }
        pedPos.z = std::max(groundZ + 1.0f, origin.z);

        if (std::abs(groundZ - origin.z) >= 1.0f) {
            continue;
        }

        const eModelID model = CPopulation::ChooseGangOccupation(gangId);
        if (model == MODEL_INVALID || !CModelInfo::GetPedModelInfo(model)->GetRwObject()) {
            continue;
        }

        CPed* ped = CPopulation::AddPed(type, model, pedPos, false);
        if (ped) {
            ped->m_fCurrentRotation = ped->m_fAimingRotation = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pedPos.x, pedPos.y);
            ped->SetHeading(ped->m_fCurrentRotation);
            CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);
            peds[createdCount++] = ped;
        }
    }

    if (createdCount < 1) {
        for (int32 i = 0; i < createdCount; i++) {
            CPopulation::RemovePed(peds[i]);
        }
        return false;
    }

    auto& group = CPedGroups::ms_groups[groupId];
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::RANDOM));
    group.GetMembership().SetLeader(peds[0]);
    group.GetMembership().AddMember(peds[0]);
    peds[0]->GetTaskManager().SetTask(new CTaskComplexWanderGang(PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), CGeneral::GetRandomNumberInRange(0, 5000), true, 0.5f), TASK_PRIMARY_PRIMARY);

    for (int32 i = 1; i < createdCount; i++) {
        group.GetMembership().AddMember(peds[i]);
        peds[i]->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, false), TASK_PRIMARY_PRIMARY);
    }
    group.GetIntelligence().Process();

    return true;
}

// 0x5FD810
bool CPedGroupPlacer::PlaceGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    using enum ePedGroupDefaultTaskAllocatorType;
    switch (allocType) {
    case FOLLOW_ANY_MEANS:
    case FOLLOW_LIMITED:   return PlaceFormationGroup(type, numOfPeds, origin, allocType);
    case STAND_STILL:
    case CHAT:             return PlaceChatGroup(type, numOfPeds, origin, allocType);
    case RANDOM:           return PlaceRandomGroup(type, numOfPeds, origin, allocType);
    default:               NOTSA_UNREACHABLE();
    }
}
