#include "StdInc.h"

#include "RoadBlocks.h"
#include "PedPlacement.h"
#include "TaskComplexWanderCop.h"
#include "TaskSimpleStandStill.h"
#include "Automobile.h"
#include "FireManager.h"
#include "Population.h"
#include <extensions/File.hpp>

void CRoadBlocks::InjectHooks() {
    RH_ScopedClass(CRoadBlocks);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x461100);
    RH_ScopedInstall(ClearScriptRoadBlocks, 0x460EC0);
    RH_ScopedInstall(ClearSpaceForRoadBlockObject, 0x461020);
    RH_ScopedInstall(CreateRoadBlockBetween2Points, 0x4619C0);
    RH_ScopedInstall(GenerateRoadBlockPedsForCar, 0x461170);
    RH_ScopedInstall(GenerateRoadBlocks, 0x4629E0);
    RH_ScopedInstall(GetRoadBlockNodeInfo, 0x460EE0);
    RH_ScopedInstall(RegisterScriptRoadBlock, 0x460DF0);
}

// 0x461100
void CRoadBlocks::Init() {
    rng::fill(InOrOut, true);
    GenerateDynamicRoadBlocks = false;

    if (notsa::File rbx("data\\paths\\roadblox.dat", "rb"); rbx) {
        rbx.Read(&NumRoadBlocks, sizeof(int32));
        assert(NumRoadBlocks <= MAX_ROADBLOCKS);
        rbx.Read(RoadBlockNodes.data(), RoadBlockNodes.size() * sizeof(CNodeAddress));
    } else {
        NOTSA_UNREACHABLE("roadblox.dat couldn't be opened!");
    }
    ClearScriptRoadBlocks();
}

// 0x460EC0
void CRoadBlocks::ClearScriptRoadBlocks() {
    for (auto& srb : aScriptRoadBlocks) {
        srb.IsActive = false;
    }
}

// 0x461020
// Returns true if cleared successfully.
bool CRoadBlocks::ClearSpaceForRoadBlockObject(CVector cornerA, CVector cornerB){
    int16 numEntities{};
    CEntity* entities[2]{};
    CWorld::FindObjectsIntersectingCube(
        cornerA,
        cornerB,
        &numEntities,
        std::size(entities),
        entities,
        false,
        true,
        true,
        true,
        false
    );

    if (numEntities > std::size(entities) || numEntities <= 0) {
        return numEntities <= 0;
    }

    const auto Remove = [](CEntity* e) {
        CWorld::Remove(e);
        delete e;
    };

    for (auto* entity : entities | rngv::take(numEntities)) {
        switch (entity->GetType()) {
        case ENTITY_TYPE_VEHICLE:
            if (auto* v = entity->AsVehicle(); !v->CanBeDeleted()) {
                return false;
            } else if (!v->vehicleFlags.bCreateRoadBlockPeds) {
                Remove(v);
            }
            break;
        case ENTITY_TYPE_PED:
            if (auto* p = entity->AsPed(); p->CanBeDeleted()) {
                Remove(p);
            } else {
                return false;
            }
            break;
        case ENTITY_TYPE_OBJECT:
            if (auto* o = entity->AsObject(); o->CanBeDeleted() && o->m_nObjectType != OBJECT_GAME) {
                Remove(o);
            } else {
                return false;
            }
            break;
        default:
            NOTSA_UNREACHABLE();
        }
    }

    return true;
}

// 0x4619C0
void CRoadBlocks::CreateRoadBlockBetween2Points(CVector a, CVector b, bool isGangRoadBlock) {
    CVector diff = b - a;
    float len = diff.Magnitude();
    CVector mid = (a + b) * 0.5f;

    CVector dir = diff;
    dir.Normalise();

    CVector perp(-dir.y, dir.x, 0.0f);
    perp.Normalise();

    CVector toPlayer = FindPlayerCoors() - mid;
    if (DotProduct(toPlayer, perp) < 0.0f) {
        perp = -perp;
    }

    int32 modelIndex = MODEL_INVALID;
    if (isGangRoadBlock) {
        modelIndex = CPopulation::PickRiotRoadBlockCar();
        if (modelIndex == MODEL_INVALID) {
            return;
        }
    } else {
        CWanted* wanted = FindPlayerWanted();
        if (wanted->AreArmyRequired()) {
            modelIndex = MODEL_BARRACKS;
        } else if (wanted->AreFbiRequired()) {
            modelIndex = MODEL_FBIRANCH;
        } else if (wanted->AreSwatRequired()) {
            modelIndex = MODEL_ENFORCER;
        } else {
            modelIndex = CStreaming::GetDefaultCopCarModel(false);
        }

        if (CStreaming::ms_aInfoForModel[modelIndex].m_nLoadState != LOADSTATE_LOADED) {
            modelIndex = CStreaming::GetDefaultCopCarModel(false);
        }

        if (modelIndex == MODEL_POLMAV) {
            return;
        }
    }

    CColModel* colModel = CModelInfo::GetModelInfo(modelIndex)->GetColModel();
    const CBoundingBox& bbox = colModel->GetBoundingBox();

    float carWidth = (bbox.m_vecMax.x - bbox.m_vecMin.x) + 2.0f;
    float carLength = (bbox.m_vecMax.y - bbox.m_vecMin.y) + 0.2f;
    if (isGangRoadBlock) {
        carLength += 0.5f;
    }

    float remainingLen = len;
    int32 numCars = 0;
    bool carOrientations[5]{};

    for (int32 i = 0; i < 5; i++) {
        if (modelIndex == MODEL_BARRACKS || isGangRoadBlock) {
            carOrientations[i] = false;
        } else {
            carOrientations[i] = (CGeneral::GetRandomNumber() & 1);
        }

        float widthNeeded = carOrientations[i] ? carWidth : carLength;
        if (remainingLen < widthNeeded) {
            break;
        }
        remainingLen -= widthNeeded;
        numCars++;
        if (numCars == 5) {
            break;
        }
    }

    float spacing = remainingLen / (float)(numCars + 1);

    if (numCars > 0) {
        float curOffset = spacing;
        for (int32 i = 0; i < numCars; i++) {
            float halfDim = carOrientations[i] ? (carWidth * 0.5f) : (carLength * 0.5f);
            curOffset += halfDim;

            CVector carPos = a + dir * curOffset;
            CMatrix carMatrix;
            carMatrix.SetUnity();

            bool isPerp = false;
            if (carOrientations[i]) {
                carMatrix.GetForward() = -perp;
                carMatrix.GetRight() = -dir;
                carMatrix.GetUp() = CVector(0.0f, 0.0f, 1.0f);

                if (CGeneral::GetRandomNumber() & 1) {
                    carMatrix.RotateZ(PI);
                }
                float angle = ((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.0f) * (isGangRoadBlock ? 0.004f : 0.002f);
                carMatrix.RotateZ(angle);
            } else {
                if (isGangRoadBlock) {
                    isPerp = (CGeneral::GetRandomNumber() & 0xFF) < 0x40;
                }
                if (isPerp) {
                    carMatrix.GetForward() = CVector(0.0f, 0.0f, 1.0f);
                    carMatrix.GetRight() = CVector(-dir.y, 0.0f, dir.x);
                    carMatrix.GetUp() = CVector(0.0f, 0.0f, 1.0f);
                } else {
                    carMatrix.GetForward() = -dir;
                    carMatrix.GetRight() = perp;
                    carMatrix.GetUp() = CVector(0.0f, 0.0f, 1.0f);
                }

                if (CGeneral::GetRandomNumber() & 1) {
                    if (isPerp) {
                        carMatrix.RotateX(PI);
                    } else {
                        carMatrix.RotateZ(PI);
                    }
                }
                float angle = ((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.0f) * (isGangRoadBlock ? 0.006f : 0.003f);
                carMatrix.RotateZ(angle);
            }

            carPos.z = CWorld::FindGroundZFor3DCoord(carPos.x, carPos.y, carPos.z + 2.0f, nullptr, nullptr) + 0.3f;
            if (isPerp) {
                carPos.z -= bbox.m_vecMin.x;
            } else {
                carPos.z -= bbox.m_vecMin.z;
            }

            carMatrix.GetPosition() = carPos;

            CVector minBox = carMatrix.TransformPoint(bbox.m_vecMin);
            CVector maxBox = carMatrix.TransformPoint(bbox.m_vecMax);
            CVector boxA(std::min(minBox.x, maxBox.x), std::min(minBox.y, maxBox.y), std::min(minBox.z, maxBox.z));
            CVector boxB(std::max(minBox.x, maxBox.x), std::max(minBox.y, maxBox.y), std::max(minBox.z, maxBox.z));

            if (ClearSpaceForRoadBlockObject(boxA, boxB)) {
                if (isGangRoadBlock && (isPerp || (CGeneral::GetRandomNumber() & 0xFF) < 0x40)) {
                    auto* obj = new CObject(ModelIndices::MI_ROADBLOCKFUCKEDCAR1, true);
                    obj->SetMatrix(carMatrix);
                    obj->SetPosition(carPos);
                    obj->m_nStatus = STATUS_PHYSICS;
                    obj->m_nObjectType = OBJECT_TEMPORARY;
                    obj->m_nRemovalTime = CTimer::m_snTimeInMilliseconds + 600000;
                    CWorld::Add(obj);
                    CObject::nNoTempObjects++;
                    gFireManager.StartFire(obj, nullptr, 0.8f, 1, 60000, 2);
                    if (obj->m_pFire) {
                        obj->m_pFire->m_nFlags = 0x5C;
                    }
                } else {
                    auto* veh = new CAutomobile(modelIndex, MISSION_VEHICLE);
                    veh->vehicleFlags.bCreateRoadBlockPeds = true;
                    veh->SetStatus(STATUS_PHYSICS);
                    veh->m_eDoorLock = CARLOCK_LOCKED;
                    veh->SetMatrix(carMatrix);
                    veh->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
                    veh->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
                    veh->m_fSteerAngle = 0.0f;
                    veh->m_fGasPedal = 0.0f;
                    veh->m_fBrakePedal = 0.0f;
                    veh->vehicleFlags.bIsLocked = true;

                    if (isGangRoadBlock) {
                        if (CGeneral::GetRandomNumber() & 1) {
                            veh->BurstTyre(0, true);
                            veh->BurstTyre(1, true);
                            veh->BurstTyre(2, true);
                            veh->BurstTyre(3, true);
                            veh->m_fHealth = 100.0f;
                        }
                    } else {
                        if (veh->m_pHandling->m_fSuspensionForceLevel >= 0.94f) {
                            if (CGeneral::GetRandomNumber() & 1) {
                                veh->m_nNumPedsForRoadBlock = 1;
                            } else {
                                veh->m_nNumPedsForRoadBlock = 2;
                            }
                            GenerateRoadBlockPedsForCar(veh, i, PED_TYPE_COP);
                        }
                    }

                    CWorld::Add(veh);
                }
            }

            curOffset += halfDim;
        }
    }

    if (!isGangRoadBlock && ModelIndices::MI_ROADWORKBARRIER1 != MODEL_INVALID) {
        CColModel* barrierCol = CModelInfo::GetModelInfo(ModelIndices::MI_ROADWORKBARRIER1)->GetColModel();
        const CBoundingBox& bBox = barrierCol->GetBoundingBox();
        float barrierWidth = (bBox.m_vecMax.x - bBox.m_vecMin.x) + 0.5f;

        int32 numBarriers = std::min((int32)(len / barrierWidth), 8);
        if (numBarriers > 0) {
            float bSpacing = (len - ((float)numBarriers * barrierWidth)) / (float)(numBarriers + 1);
            for (int32 i = 0; i < numBarriers; i++) {
                float bOffset = (float)(i + 1) * bSpacing + ((float)i + 0.5f) * barrierWidth;
                CVector bPos = a + dir * bOffset + perp * 5.0f;

                CMatrix bMatrix;
                bMatrix.SetUnity();
                bMatrix.GetForward() = perp;
                bMatrix.GetRight() = -dir;
                bMatrix.GetUp() = CVector(0.0f, 0.0f, 1.0f);

                float rotAngle = ((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.0f) * 0.003f;
                bMatrix.RotateZ(rotAngle);

                bPos.x += (float)(CGeneral::GetRandomNumber() & 0xF) * 0.05f;
                bPos.y += (float)(CGeneral::GetRandomNumber() & 0xF) * 0.05f;
                bPos.z = CWorld::FindGroundZFor3DCoord(bPos.x, bPos.y, bPos.z + 2.0f, nullptr, nullptr) + 0.3f;
                bPos.z -= bBox.m_vecMin.z;
                bMatrix.GetPosition() = bPos;

                CVector minB = bMatrix.TransformPoint(bBox.m_vecMin);
                CVector maxB = bMatrix.TransformPoint(bBox.m_vecMax);
                CVector boxA(std::min(minB.x, maxB.x), std::min(minB.y, maxB.y), std::min(minB.z, maxB.z));
                CVector boxB(std::max(minB.x, maxB.x), std::max(minB.y, maxB.y), std::max(minB.z, maxB.z));

                if (ClearSpaceForRoadBlockObject(boxA, boxB)) {
                    auto* obj = new CObject(ModelIndices::MI_ROADWORKBARRIER1, true);
                    obj->SetMatrix(bMatrix);
                    obj->SetPosition(bPos);
                    obj->m_nObjectType = OBJECT_TEMPORARY;
                    obj->m_nRemovalTime = CTimer::m_snTimeInMilliseconds + 600000;
                    CWorld::Add(obj);
                    CObject::nNoTempObjects++;
                }
            }
        }
    }
}

// 0x461170
void CRoadBlocks::GenerateRoadBlockPedsForCar(CVehicle* vehicle, int32 pedsPositionsType, ePedType pedType) {
    const auto Generate = [&](eModelID pedModel = MODEL_INVALID, eCopType copType = COP_TYPE_CITYCOP, bool isSpecialCop = false) {
        static constexpr auto PLACEMENTS = std::to_array<CVector>({
            { -1.5f, +1.9f, 0.0f },
            { -1.5f, -2.6f, 0.0f },
            { +1.5f, +1.9f, 0.0f },
            { +1.5f, -2.6f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        static constexpr auto SPECIAL_COP_PLACEMENTS = std::to_array<CVector>({
            {  0.0f, +3.2f, 0.0f },
            { +1.5f, -1.8f, 0.0f },
            {  0.0f, +3.2f, 0.0f },
            { -1.5f, -1.8f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        const auto placementIdx = 2 * pedsPositionsType;
        const auto radiusRatio  = vehicle->GetColModel()->GetBoundingSphere().m_fRadius
            / CModelInfo::GetModelInfo(CStreaming::GetDefaultCopCarModel(false))->GetColModel()->GetBoundingSphere().m_fRadius;

        for (auto i = 0u; i < vehicle->m_nNumPedsForRoadBlock; i++) {
            const auto offset = (isSpecialCop ? SPECIAL_COP_PLACEMENTS : PLACEMENTS)[placementIdx + i] * radiusRatio;
            const auto pos = vehicle->GetMatrix().TransformPoint(offset);

            auto* ped = [&]() -> CPed* {
                if (pedType != PED_TYPE_COP) { // 0x461560
                    return new CCivilianPed(pedType, pedModel);
                } else {
                    auto* p = new CCopPed(CStreaming::IsModelLoaded(pedModel) ? copType : COP_TYPE_CITYCOP);
                    if (copType == COP_TYPE_CITYCOP) {
                        p->SetCurrentWeapon(WEAPON_PISTOL);
                    }
                    return p;
                }
            }();

            ped->SetPosn(std::get<CVector>(CPedPlacement::FindZCoorForPed(pos)));
            ped->GetMatrix().SetRotateKeepPos({ 0.0f, 0.0f, -HALF_PI });

            if (pedType == PED_TYPE_COP) {
                auto* t = new CTaskComplexWanderCop(PEDMOVE_STILL, CGeneral::GetRandomNumberInRange(8ui8));
                t->m_nSubTaskCreatedTimer = {};
                t->m_nScanForStuffTimer   = {};
                ped->GetTaskManager().SetTask(t, TASK_PRIMARY_PRIMARY);
            }
            ped->GetTaskManager().SetTask(new CTaskSimpleStandStill(0, true), TASK_PRIMARY_DEFAULT);

            ped->bStayInSamePlace         = true;
            ped->bNotAllowedToDuck        = true;
            ped->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 10'000;
            ped->bCrouchWhenShooting      = !isSpecialCop || pedsPositionsType != 2;
            ped->bCullExtraFarAway        = true;
            CEntity::RegisterReference(ped->m_pVehicle = vehicle);
            CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);

            if (pedType != PED_TYPE_COP) {
                const auto weapon = CGangs::Gang[pedType - PED_TYPE_GANG1].GetRandomWeapon(false);
                if (weapon != WEAPON_UNARMED) {
                    ped->GiveDelayedWeapon(weapon, 25'001);
                    ped->SetCurrentWeapon(weapon);
                }
            }
            CWorld::Add(ped);
            ped->GetEventGroup().Add<CEventScriptCommand>({ TASK_PRIMARY_PRIMARY, new CTaskComplexKillPedOnFoot(FindPlayerPed()) });
        }
    };

    if (pedType == PED_TYPE_COP) {
        switch (vehicle->GetModelId()) {
        case MODEL_ENFORCER: Generate(MODEL_SWAT,    COP_TYPE_SWAT1,   true); break;
        case MODEL_BARRACKS: Generate(MODEL_ARMY,    COP_TYPE_ARMY,    true); break;
        case MODEL_FBIRANCH: Generate(MODEL_FBI,     COP_TYPE_FBI,     true); break;
        case MODEL_COPCARRU: Generate(MODEL_INVALID, COP_TYPE_CITYCOP, true); break;
        default:             Generate(MODEL_INVALID, COP_TYPE_CITYCOP, false); break;
        }
    } else if (IsPedTypeGang(pedType)) {
        for (auto i = 0; i < TOTAL_GANGS; i++) {
            if (!CPopCycle::m_pCurrZoneInfo->GangStrength[i]) {
                continue;
            }
            const auto pedModel = CGangs::ChooseGangPedModel((eGangID)i);
            if (pedModel == MODEL_INVALID) {
                continue;
            }
            Generate(pedModel);
            return;
        }
    } else {
        Generate();
    }
}

// 0x4629E0
void CRoadBlocks::GenerateRoadBlocks() {
    ZoneScoped;

    if (FindPlayerWanted()->m_ChanceOnRoadBlock && FindPlayerVehicle()) {
        if (!GenerateDynamicRoadBlocks) {
            rng::fill(InOrOut, true);
            GenerateDynamicRoadBlocks = true;
        }

        const auto counter1      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16 + 1);
        const auto rbsToGenerate = std::min((uint32)NumRoadBlocks, ((counter1 % 16) + counter1) / 16);
        auto       counter2      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16) / 16;

        for (; counter2 < rbsToGenerate; counter2++) {
            const auto& mrbNode = RoadBlockNodes[counter2];
            if (!ThePaths.IsAreaLoaded(mrbNode)) {
                continue;
            }
            const auto& mainNode = ThePaths.GetPathNode(mrbNode);
            const auto  playerPos = FindPlayerCoors();
            if (std::abs(playerPos.x - mainNode->GetPosition().x) >= 90.0f ||
                std::abs(playerPos.y - mainNode->GetPosition().y) >= 90.0f ||
                DistanceBetweenPoints2D(playerPos, mainNode->GetPosition()) >= 90.0f)
            {
                InOrOut[counter2] = false;
                continue;
            }

            if (InOrOut[counter2]) {
                continue;
            }
            InOrOut[counter2] = true;

            if (CGeneral::GetRandomNumberInRange(128u) >= FindPlayerWanted()->m_ChanceOnRoadBlock) {
                continue;
            }

            float mrbWidth{};
            CVector mrbDir{};
            if (!GetRoadBlockNodeInfo(mrbNode, mrbWidth, mrbDir)) {
                continue;
            }

            if (mainNode->m_nPathWidth) {
                const auto width = mainNode->m_nPathWidth / 16.0f;
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() + mrbDir * (mrbWidth / 2.f + width),
                    mainNode->GetPosition() + mrbDir * width,
                    false
                );
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() - mrbDir * width,
                    mainNode->GetPosition() - mrbDir * (mrbWidth / 2.f + width),
                    false
                );
                continue;
            }

            for (auto&& [i, nodeAddr] : rngv::enumerate(RoadBlockNodes)) {
                if (counter2 == i || InOrOut[i] || !ThePaths.IsAreaLoaded(nodeAddr.m_wAreaId)) {
                    continue;
                }
                const auto& node = ThePaths.GetPathNode(nodeAddr);

                if (std::abs(mainNode->GetPosition().x - node->GetPosition().x) >= 30.0f ||
                    std::abs(mainNode->GetPosition().y - node->GetPosition().y) >= 30.0f)
                {
                    continue;
                }

                float   width{};
                CVector dir{}; 
                if (!GetRoadBlockNodeInfo(nodeAddr, width, dir)) {
                    continue;
                }

                if (mrbWidth != width || dir.Dot(mrbDir) <= 0.7f) {
                    continue;
                }

                [[maybe_unused]] CColPoint col{};
                [[maybe_unused]] CEntity*  colEntity{};
                if (CWorld::ProcessLineOfSight(
                    mainNode->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    node->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    col,
                    colEntity,
                    true,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false
                )) {
                    continue;
                }

                const auto dirFromMain = (node->GetPosition() - mainNode->GetPosition()).Normalized();
                CreateRoadBlockBetween2Points(
                    node->GetPosition()     + dirFromMain * (mrbWidth / 2.0f),
                    mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                    false
                );
                InOrOut[i] = true;

                if (i == NumRoadBlocks) {
                    CreateRoadBlockBetween2Points(
                        mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                        mainNode->GetPosition() + dirFromMain * (mrbWidth / 2.0f),
                        false
                    );
                    break;
                }
            }
        }
    } else {
        GenerateDynamicRoadBlocks = false;
    }

    if (auto& srb = aScriptRoadBlocks[CTimer::GetFrameCounter() % MAX_SCRIPT_ROADBLOCKS]; srb.IsActive) {
        const auto c = CVector::Centroid({ srb.CornerA, srb.CornerB });

        if (DistanceBetweenPoints(FindPlayerCoors(), c) >= 90.0f) {
            srb.IsSafeToCreate = true;
        } else if (srb.IsSafeToCreate) {
            CreateRoadBlockBetween2Points(srb.CornerA, srb.CornerB, srb.IsGangRoadBlock);
            srb.IsSafeToCreate = false;
        }
    }
}

// 0x460EE0
bool CRoadBlocks::GetRoadBlockNodeInfo(CNodeAddress nodeAddress, float& outWidth, CVector& outDir) {
    auto* const node = ThePaths.GetPathNode(nodeAddress);
    assert(node);

    assert(node->m_nNumLinks >= 2);
    const auto naviLinkAddrA = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 0),
               naviLinkAddrB = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 1);
    if (!ThePaths.IsAreaLoaded(naviLinkAddrA.m_wAreaId) || !ThePaths.IsAreaLoaded(naviLinkAddrB.m_wAreaId)) {
        return false;
    }

    const auto &naviLinkA = ThePaths.GetCarPathLink(naviLinkAddrA),
               &naviLinkB = ThePaths.GetCarPathLink(naviLinkAddrB);

    const auto maxNumLanes = std::max(
        naviLinkA.m_numOppositeDirLanes + naviLinkA.m_numSameDirLanes,
        naviLinkB.m_numOppositeDirLanes + naviLinkB.m_numSameDirLanes
    );

    outWidth = ((float)maxNumLanes + 1.f) * 5.f;
    outDir   = CVector{ (naviLinkB.GetNodeCoors() - naviLinkA.GetNodeCoors()).GetPerpRight(), 0.f }.Normalized();

    return true;
}

// 0x460DF0
void CRoadBlocks::RegisterScriptRoadBlock(CVector cornerA, CVector cornerB, bool isGangRoadBlock) {
    auto free = rng::find_if(aScriptRoadBlocks, [](const auto& srb) { return !srb.IsActive; });
    if (free == aScriptRoadBlocks.end()) {
        // No free script roadblock found
        return;
    }

    free->CornerA         = cornerA;
    free->CornerB         = cornerB;
    free->IsActive        = true;
    free->IsSafeToCreate  = true;
    free->IsGangRoadBlock = isGangRoadBlock;
}
