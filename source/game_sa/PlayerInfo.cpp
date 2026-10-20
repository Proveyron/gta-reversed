#include "StdInc.h"

#include "PlayerInfo.h"
#include "FireManager.h"
#include "MenuSystem.h"
#include "Hud.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexGoPickUpEntity.h"
#include "TaskSimpleJetPack.h"
#include "TaskSimpleSwim.h"

void CPlayerInfo::InjectHooks() {
    RH_ScopedClass(CPlayerInfo);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x571920, { .State = HS::RedirectToGTA, .Locked = true }); // hooking ctor will produce bugs with weapons, you will never give weapon through cheat or something
    RH_ScopedInstall(CancelPlayerEnteringCars, 0x56E860);
    RH_ScopedInstall(FindObjectToSteal, 0x56DBD0);
    RH_ScopedInstall(EvaluateCarPosition, 0x56DAD0);
    RH_ScopedInstall(Process, 0x56F8D0);
    RH_ScopedInstall(FindClosestCarSectorList, 0x56F4E0);
    RH_ScopedInstall(Clear, 0x56F330);
    RH_ScopedInstall(StreamParachuteWeapon, 0x56EB30);
    RH_ScopedInstall(AddHealth, 0x56EAB0);
    RH_ScopedInstall(ArrestPlayer, 0x56E5D0);
    RH_ScopedInstall(SetPlayerSkin, 0x5717F0);
    RH_ScopedInstall(LoadPlayerSkin, 0x56F7D0);
    RH_ScopedInstall(DeletePlayerSkin, 0x56EA80);
    RH_ScopedInstall(BlowUpRCBuggy, 0x56EA30);
    RH_ScopedInstall(MakePlayerSafe, 0x56E870);
    RH_ScopedInstall(PlayerFailedCriticalMission, 0x56E830);
    RH_ScopedInstall(WorkOutEnergyFromHunger, 0x56E610);
    RH_ScopedInstall(KillPlayer, 0x56E580);
    RH_ScopedInstall(IsRestartingAfterMissionFailed, 0x56E570);
    RH_ScopedInstall(IsRestartingAfterArrest, 0x56E560);
    RH_ScopedInstall(IsRestartingAfterDeath, 0x56E550);
    RH_ScopedInstall(IsPlayerInRemoteMode, 0x56DAB0);
    RH_ScopedInstall(GetPos_Hook, 0x56DFB0);
    RH_ScopedInstall(GetSpeed_Hook, 0x56DF50);
    RH_ScopedInstall(GivePlayerParachute, 0x56EC40);
    RH_ScopedInstall(SetLastTargetVehicle, 0x56DA80);
    RH_ScopedInstall(Load, 0x5D3B00);
    RH_ScopedInstall(Save, 0x5D3AC0);
}

// 0x571920
CPlayerInfo::CPlayerInfo() {
    plugin::CallMethod<0x571920, CPlayerInfo*>(this); // see hook
    return;

    m_PlayerData = CPlayerPedData();

    m_pSkinTexture = nullptr;
    m_bParachuteReferenced = false;
    m_nRequireParachuteTimer = 0;
}

// 0x571920
CPlayerInfo* CPlayerInfo::Constructor() {
    this->CPlayerInfo::CPlayerInfo();
    return this;
}

CVector* CPlayerInfo::GetSpeed_Hook(CVector* out) {
    *out = GetSpeed();
    return out;
}

CVector* CPlayerInfo::GetPos_Hook(CVector* outPos) {
    *outPos = GetPos();
    return outPos;
}

// 0x56E860
void CPlayerInfo::CancelPlayerEnteringCars(CVehicle* vehicle) {
    // NOP
}

// 0x56DBD0
CEntity* CPlayerInfo::FindObjectToSteal(CPed* ped) {
    const CVector searchCenter = ped->GetPosition() + ped->GetForward() * 1.5f;

    const int32 minX = std::max(0, CWorld::GetSectorX(searchCenter.x - 3.0f));
    const int32 maxX = std::min(MAX_SECTORS_X - 1, CWorld::GetSectorX(searchCenter.x + 3.0f));
    const int32 minY = std::max(0, CWorld::GetSectorY(searchCenter.y - 3.0f));
    const int32 maxY = std::min(MAX_SECTORS_Y - 1, CWorld::GetSectorY(searchCenter.y + 3.0f));

    CEntity* bestObj = nullptr;
    CWorld::AdvanceCurrentScanCode();

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            for (auto* obj : CWorld::GetRepeatSector(x, y).Objects) {
                if (!obj->objectFlags.bIsLiftable) {
                    continue;
                }
                if (obj->m_nScanCode == CWorld::ms_nCurrentScanCode) {
                    continue;
                }
                const CVector diff = obj->GetPosition() - searchCenter;
                if (diff.SquaredMagnitude() >= 4.5f) {
                    continue;
                }
                const float rightDot = DotProduct(diff, ped->GetRight());
                const float forwardDot = DotProduct(diff, ped->GetForward());
                const float forwardScore = (forwardDot < 0.0f) ? (10.0f - forwardDot) : forwardDot;
                const float score = std::abs(rightDot) * 3.0f + forwardScore;
                if (score < 1000.0f) {
                    bestObj = obj;
                }
            }
        }
    }

    return bestObj;
}

// 0x56DAD0
void CPlayerInfo::EvaluateCarPosition(CEntity* car, CPed* ped, float pedToVehDist, float* outDistance, CVehicle** outVehicle) {
    const auto& carPosn = car->GetPosition();
    const auto& pedPosn = ped->GetPosition();
    const auto& forward = ped->GetForward();

    // Find our rotation (so that is, at which angle the forward vector is)
    const auto angleFront = CGeneral::GetATanOfXY(forward.x, forward.y);

    // Find car angle, not relative to our rotation
    const auto carAngle = CGeneral::GetATanOfXY(carPosn.x - pedPosn.x, carPosn.y - pedPosn.y);

    // Make car's angle relative to our rotation (notice: it's an abs value)
    // Basically, we calculate how much the car is in our FOV.
    const auto carAngleFromFront = std::abs(CGeneral::LimitRadianAngle(angleFront - carAngle));

    // Calculate imaginary distance based on the car's angle: The higher the angle the greater the distance
    const auto distance = (1.0f - carAngleFromFront / TWO_PI) * (10.0f - pedToVehDist);
    if (distance >= *outDistance) {
        *outDistance = distance;
        *outVehicle = car->AsVehicle();
    }
}

// 0x56F7D0
void CPlayerInfo::LoadPlayerSkin() {
    DeletePlayerSkin();
    m_pSkinTexture = CPlayerSkin::GetSkinTexture(m_szSkinName);
}

// 0x56EA80
void CPlayerInfo::DeletePlayerSkin() {
    if (m_pSkinTexture) {
        RwTextureDestroy(m_pSkinTexture);
        m_pSkinTexture = nullptr;
    }
}

// 0x5717F0
void CPlayerInfo::SetPlayerSkin(const char* name) {
    strcpy_s(m_szSkinName, name); // NOTSA: They used `strcpy`, we use `_s` for safety
    LoadPlayerSkin();
}

// 0x56DA80
void CPlayerInfo::SetLastTargetVehicle(CVehicle* vehicle) {
    CEntity::SafeCleanUpRef(m_pLastTargetVehicle);
    m_pLastTargetVehicle = vehicle;
    CEntity::SafeRegisterRef(m_pLastTargetVehicle);
}

// 0x56F4E0
void CPlayerInfo::FindClosestCarSectorList(CPtrListDoubleLink<CVehicle*>& ptrList, CPed* ped, float minX, float minY, float maxX, float maxY, float* outVehDist, CVehicle** outVehicle) {
    for (auto* vehicle : ptrList) {
        if (vehicle->m_nScanCode == CWorld::ms_nCurrentScanCode) {
            continue;
        }
        if (!vehicle->m_bUsesCollision) {
            continue;
        }
        if (vehicle->m_nType != ENTITY_TYPE_VEHICLE) {
            continue;
        }
        vehicle->m_nScanCode = CWorld::ms_nCurrentScanCode;
        if (vehicle->m_nStatus == STATUS_WRECKED || vehicle->m_nStatus == STATUS_TRAIN_MOVING) {
            continue;
        }
        if (!vehicle->m_matrix) {
            vehicle->AllocateMatrix();
            vehicle->m_placement.UpdateMatrix(vehicle->m_matrix);
        }
        if (vehicle->GetUp().z < 0.3f && vehicle->m_nVehicleSubType != VEHICLE_TYPE_BIKE) {
            continue;
        }
        (void)vehicle->GetBoundCentre();
        const float roadZ = vehicle->GetPosition().z - vehicle->GetHeightAboveRoad() + 1.0f;
        const float deltaZ = ped->GetPosition().z - roadZ;

        bool bDoorAccessible = false;
        if (vehicle->m_nModelIndex == MODEL_FORKLIFT) {
            CVector doorPos;
            int32 door;
            if (CCarEnterExit::GetNearestCarDoor(ped, vehicle, &doorPos, &door) &&
                std::abs(ped->GetPosition().z - doorPos.z) < 1.0f) {
                bDoorAccessible = true;
            }
        }
        if (!bDoorAccessible) {
            if (std::abs(deltaZ) < 2.0f) {
                bDoorAccessible = true;
            } else if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
                if (roadZ < ped->GetPosition().z && (ped->GetPosition().z - roadZ) < 4.0f) {
                    bDoorAccessible = true;
                } else if (ped->m_pContactEntity == vehicle) {
                    bDoorAccessible = true;
                }
            }
        }
        if (!bDoorAccessible) {
            continue;
        }
        if (!vehicle->vehicleFlags.bConsideredByPlayer) {
            continue;
        }
        CVector doorPos;
        int32 door;
        if (!CCarEnterExit::GetNearestCarDoor(ped, vehicle, &doorPos, &door)) {
            continue;
        }
        float dist = DistanceBetweenPoints2D(ped->GetPosition(), vehicle->GetPosition());
        if (dist >= 10.0f) {
            continue;
        }
        if (CPedGeometryAnalyser::LiesInsideBoundingBox(*ped, ped->GetPosition(), *vehicle)) {
            dist = 10.0f;
        } else if (dist > 10.0f) {
            continue;
        }
        if (CCranes::IsThisCarBeingCarriedByAnyCrane(vehicle)) {
            continue;
        }
        EvaluateCarPosition(vehicle, ped, dist, outVehDist, outVehicle);
    }
}

// 0x56F8D0
void CPlayerInfo::Process(uint32 playerIndex) {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    CPad* pad = CPad::GetPad(playerIndex);

    // Taxi fare timer
    if (m_bTaxiTimerScore && m_pPed->bInVehicle &&
        (m_pPed->m_pVehicle->m_nModelIndex == MODEL_TAXI || m_pPed->m_pVehicle->m_nModelIndex == MODEL_CABBIE) &&
        m_pPed->m_pVehicle->m_pDriver == m_pPed &&
        m_pPed->m_pVehicle->m_nNumPassengers > 0)
    {
        const uint32 delta = CTimer::GetTimeInMS() - m_nTaxiTimer;
        if (delta >= 1000) {
            const uint32 seconds = delta / 1000;
            m_nMoney += seconds;
            m_nTaxiTimer += seconds * 1000;
        }
    } else {
        m_nTaxiTimer = CTimer::GetTimeInMS();
    }

    // Stunt tracking
    if (m_pPed->bInVehicle && m_pPed->m_pVehicle) {
        if (m_pPed->m_pVehicle->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
            auto* autoVeh = m_pPed->m_pVehicle->AsAutomobile();
            if (autoVeh->m_nNumContactWheels < 3) {
                m_nCarLess3WheelCounter += CTimer::GetTimeStepInMS();
            } else {
                m_nCarLess3WheelCounter = 0;
            }

            const bool bLeftSuspension = autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_FRONT_RIGHT] < 1.0f &&
                                         autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_REAR_RIGHT] < 1.0f &&
                                         autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_FRONT_LEFT] >= 1.0f &&
                                         autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_REAR_LEFT] >= 1.0f &&
                                         autoVeh->m_fDamageIntensity == 0.0f;

            const bool bRightSuspension = autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_FRONT_LEFT] < 1.0f &&
                                          autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_REAR_LEFT] < 1.0f &&
                                          autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_FRONT_RIGHT] >= 1.0f &&
                                          autoVeh->m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_REAR_RIGHT] >= 1.0f &&
                                          autoVeh->m_fDamageIntensity == 0.0f;

            if (bLeftSuspension || bRightSuspension) {
                m_nCarTwoWheelCounter += CTimer::GetTimeStepInMS();
                m_fCarTwoWheelDist += autoVeh->m_fMovingSpeed;
                m_nTempBufferCounter = std::max(0, (int32)(m_nTempBufferCounter - CTimer::GetTimeStepInMS() * 0.5f));
            } else {
                if (m_nCarTwoWheelCounter > 0 && m_nTempBufferCounter < 500) {
                    m_nTempBufferCounter -= static_cast<uint32>(CTimer::GetTimeStepInMS() * 1.5f);
                } else {
                    if (m_nCarTwoWheelCounter >= 2000) {
                        m_fBestCarTwoWheelsDistM = m_fCarTwoWheelDist;
                        m_nBestCarTwoWheelsTimeMs = m_nCarTwoWheelCounter;
                        CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_TIME, static_cast<float>(m_nCarTwoWheelCounter / 1000));
                        CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_DISTANCE, m_fCarTwoWheelDist);
                    }
                    m_nCarTwoWheelCounter = 0;
                    m_fCarTwoWheelDist = 0.0f;
                }
            }
        } else if (m_pPed->m_pVehicle->m_nVehicleSubType == VEHICLE_TYPE_BIKE) {
            auto* bike = m_pPed->m_pVehicle->AsBike();
            const bool bWheelie = bike->m_aWheelRatios[0] < 1.0f &&
                                  bike->m_aWheelRatios[1] < 1.0f &&
                                  m_nBikeFrontWheelCounter == 0 &&
                                  (bike->m_aWheelRatios[2] >= 1.0f || (bike->m_aWheelRatios[3] >= 1.0f && bike->m_fDamageIntensity == 0.0f));

            if (bWheelie) {
                m_nBikeRearWheelCounter += CTimer::GetTimeStepInMS();
                m_fBikeRearWheelDist += bike->m_fMovingSpeed;
                m_nTempBufferCounter = std::max(0, (int32)(m_nTempBufferCounter - CTimer::GetTimeStepInMS() * 0.1f));
                m_nCarTwoWheelCounter = 0;
                m_nCarLess3WheelCounter = 0;
            } else {
                if (m_nBikeRearWheelCounter > 0 && m_nTempBufferCounter < 500) {
                    m_nTempBufferCounter -= static_cast<uint32>(CTimer::GetTimeStepInMS() * 1.5f);
                } else {
                    if (m_nBikeRearWheelCounter >= 5000) {
                        m_nBestBikeWheelieTimeMs = m_nBikeRearWheelCounter;
                        m_fBestBikeWheelieDistM = m_fBikeRearWheelDist;
                        CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_TIME, static_cast<float>(m_nBikeRearWheelCounter / 1000));
                        CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_DISTANCE, m_fBikeRearWheelDist);
                    }
                    m_nBikeRearWheelCounter = 0;
                    m_fBikeRearWheelDist = 0.0f;
                }

                const bool bStoppie = bike->m_aWheelRatios[2] < 1.0f &&
                                      bike->m_aWheelRatios[3] < 1.0f &&
                                      (bike->m_aWheelRatios[0] >= 1.0f || (bike->m_aWheelRatios[1] >= 1.0f && bike->m_fDamageIntensity == 0.0f));

                if (bStoppie) {
                    m_nBikeFrontWheelCounter += CTimer::GetTimeStepInMS();
                    m_fBikeFrontWheelDist += bike->m_fMovingSpeed;
                    m_nTempBufferCounter = std::max(0, (int32)(m_nTempBufferCounter - CTimer::GetTimeStepInMS() * 0.1f));
                    m_nCarTwoWheelCounter = 0;
                    m_nCarLess3WheelCounter = 0;
                } else {
                    if (m_nBikeFrontWheelCounter > 0 && m_nTempBufferCounter < 500) {
                        m_nTempBufferCounter -= static_cast<uint32>(CTimer::GetTimeStepInMS() * 1.5f);
                    } else {
                        if (m_nBikeFrontWheelCounter >= 2000) {
                            m_nBestBikeStoppieTimeMs = m_nBikeFrontWheelCounter;
                            m_fBestBikeStoppieDistM = m_fBikeFrontWheelDist;
                            CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_TIME, static_cast<float>(m_nBikeFrontWheelCounter / 1000));
                            CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_DISTANCE, m_fBikeFrontWheelDist);
                        }
                        m_nBikeFrontWheelCounter = 0;
                        m_fBikeFrontWheelDist = 0.0f;
                    }
                }
            }
        }
    }

    WorkOutEnergyFromHunger();

    // Display money smoothing
    const int32 moneyDiff = m_nMoney - m_nDisplayMoney;
    if (moneyDiff != 0) {
        const int32 absDiff = std::abs(moneyDiff);
        int32 step;
        if (absDiff > 100000) {
            step = 12345;
        } else if (absDiff > 10000) {
            step = 1234;
        } else if (absDiff > 1000) {
            step = 123;
        } else if (absDiff > 50) {
            step = 42;
        } else {
            step = 1;
        }
        if (moneyDiff < 0) {
            m_nDisplayMoney -= step;
        } else {
            m_nDisplayMoney += step;
        }
    }

    // Health calculation
    m_pPed->m_fMaxHealth = CStats::GetFatAndMuscleModifier(STAT_MOD_MAX_HEALTH);
    m_nMaxHealth = static_cast<uint8>(m_pPed->m_fMaxHealth);

    // Parachute streaming
    if (m_pPed->bInVehicle && CStats::GetStatValue(STAT_FLYING_SKILL) >= 1000.0f) {
        StreamParachuteWeapon(true);
    } else if (m_bParachuteReferenced) {
        if (!CGameLogic::IsCoopGameGoingOn()) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
    }

    // Road density calculation
    if ((CTimer::m_FrameCounter & 0xF) == 0) {
        const CVector& densityPos = m_pPed->bInVehicle ? m_pPed->m_pVehicle->GetPosition() : m_pPed->GetPosition();
        m_fRoadDensityAroundPlayer = ThePaths.CalcRoadDensity(densityPos.x, densityPos.y);
        m_fRoadDensityAroundPlayer = std::clamp((m_fRoadDensityAroundPlayer - 1.0f) * 0.95f + 1.0f, 0.5f, 3.0f);
    }

    // Vehicle enter / exit handling
    if (!pad->GetTarget() && m_pPed->bCanExitCar && !m_pPed->bUsingMobilePhone) {
        if (pad->ExitVehicleJustDown() || (m_bTryingToExitCar && pad->GetExitVehicle() && m_pPed->bInVehicle)) {
            m_bTryingToExitCar = false;
            if (m_pPed->bInVehicle) {
                if (!m_pRemoteVehicle) {
                    CVehicle* veh = m_pPed->m_pVehicle;
                    if (!veh->m_pTrailer || !CBridge::ThisIsABridgeObjectMovingUp(veh->m_pTrailer->m_nModelIndex)) {
                        if (veh->m_nStatus != STATUS_WRECKED && veh->m_nStatus != STATUS_TRAIN_MOVING && veh->m_nStatus != STATUS_ABANDONED) {
                            CTask* activeTask = m_pPed->GetTaskManager().GetActiveTask();
                            if (activeTask && !activeTask->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nullptr)) {
                                m_bTryingToExitCar = true;
                            } else {
                                if (veh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
                                    m_pPed->GetTaskManager().SetTask(new CTaskComplexLeaveCar(veh, 0, 0, true, false), TASK_PRIMARY_PRIMARY);
                                    m_pPed->bTryingToReachDryLand = true;
                                } else if (veh->CanPedStepOutCar(false) || veh->CanPedJumpOutCar(m_pPed)) {
                                    m_pPed->GetTaskManager().SetTask(new CTaskComplexLeaveCar(veh, 0, 0, true, false), TASK_PRIMARY_PRIMARY);
                                    m_bTryingToExitCar = true;
                                    GivePlayerParachute();
                                } else if (veh->m_nStatus == STATUS_PLAYER) {
                                    m_bTryingToExitCar = true;
                                }
                            }
                        }
                    }
                }
            } else {
                // Ped on foot
                if (!m_pPed->m_pAttachedTo) {
                    CEntity* objToSteal = FindObjectToSteal(m_pPed);
                    if (objToSteal) {
                        CTask* simplestTask = m_pPed->GetTaskManager().GetSimplestActiveTask();
                        auto* taskHold = m_pPed->GetIntelligence()->GetTaskHold(false);
                        auto* taskFighting = m_pPed->GetIntelligence()->GetTaskFighting();
                        if ((!simplestTask || simplestTask->GetTaskType() != TASK_SIMPLE_CAR_DRIVE) &&
                            !taskFighting &&
                            (!taskHold || !taskHold->GetHeldEntity()))
                        {
                            CTask* activeTask = m_pPed->GetTaskManager().GetActiveTask();
                            if (!activeTask || activeTask->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nullptr)) {
                                auto* task = (CTaskComplexGoPickUpEntity*)CTask::operator new(sizeof(CTaskComplexGoPickUpEntity));
                                plugin::CallMethod<0x6919C0, CTaskComplexGoPickUpEntity*, CEntity*, int32>(task, objToSteal, 0x51);
                                CEventScriptCommand event(TASK_PRIMARY_PRIMARY, task, false);
                                m_pPed->GetEventGroup().Add(&event);
                            }
                        }
                    } else {
                        const CVector& pos = m_pPed->GetPosition();
                        const int32 minX = std::max(0, CWorld::GetSectorX(pos.x - 10.0f));
                        const int32 maxX = std::min(MAX_SECTORS_X - 1, CWorld::GetSectorX(pos.x + 10.0f));
                        const int32 minY = std::max(0, CWorld::GetSectorY(pos.y - 10.0f));
                        const int32 maxY = std::min(MAX_SECTORS_Y - 1, CWorld::GetSectorY(pos.y + 10.0f));

                        float bestVehDist = 0.0f;
                        CVehicle* bestVeh = nullptr;
                        CWorld::AdvanceCurrentScanCode();

                        for (int32 y = minY; y <= maxY; ++y) {
                            for (int32 x = minX; x <= maxX; ++x) {
                                FindClosestCarSectorList(CWorld::GetRepeatSector(x, y).Vehicles, m_pPed, (float)minX, (float)minY, (float)maxX, (float)maxY, &bestVehDist, &bestVeh);
                            }
                        }

                        if (bestVeh) {
                            if (bestVeh->m_nVehicleSubType == VEHICLE_TYPE_TRAILER && bestVeh->m_pTowingVehicle) {
                                bestVeh = bestVeh->m_pTowingVehicle;
                            }
                            if (bestVeh && bestVeh->CanBeDriven()) {
                                if (auto* taskJetPack = m_pPed->GetIntelligence()->GetTaskJetPack()) {
                                    taskJetPack->DropJetPack(m_pPed);
                                }
                                if (bestVeh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) {
                                    if (!bestVeh->m_pDriver) {
                                        CTask* activeTask = m_pPed->GetTaskManager().GetActiveTask();
                                        if (!activeTask || activeTask->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nullptr)) {
                                            m_pPed->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver(bestVeh), TASK_PRIMARY_PRIMARY);
                                        }
                                    }
                                } else {
                                    auto* swimTask = m_pPed->GetIntelligence()->GetTaskSwim();
                                    const auto model = bestVeh->m_nModelIndex;
                                    if (swimTask && (model == MODEL_SKIMMER || model == MODEL_VORTEX || model == MODEL_SEASPAR || model == MODEL_LEVIATHN)) {
                                        CVector doorPos;
                                        int32 door;
                                        if (CCarEnterExit::GetNearestCarDoor(m_pPed, bestVeh, &doorPos, &door)) {
                                            swimTask->m_vecPos = doorPos;
                                            swimTask->m_nTimeStep = 5000;
                                        }
                                    } else {
                                        CTask* activeTask = m_pPed->GetTaskManager().GetActiveTask();
                                        if (!activeTask || activeTask->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nullptr)) {
                                            if (CWorld::Players[1].m_pPed && !CGameLogic::IsCoopGameGoingOn()) {
                                                const uint32 otherPlayerIdx = (playerIndex + 1) % 2;
                                                CPed* otherPed = CWorld::Players[otherPlayerIdx].m_pPed;
                                                CTask* defaultTask = otherPed ? otherPed->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_DEFAULT) : nullptr;
                                                CTask* primaryTask = otherPed ? otherPed->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY) : nullptr;
                                                const auto defaultTaskType = defaultTask ? defaultTask->GetTaskType() : TASK_INVALID;
                                                const auto primaryTaskType = primaryTask ? primaryTask->GetTaskType() : TASK_INVALID;

                                                CVehicle* otherVeh = nullptr;
                                                if (primaryTaskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER ||
                                                    primaryTaskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED ||
                                                    primaryTaskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_WAIT ||
                                                    primaryTaskType == TASK_COMPLEX_ENTER_CAR_AS_DRIVER_TIMED) {
                                                    otherVeh = static_cast<CTaskComplexEnterCar*>(primaryTask)->GetTargetCar();
                                                }
                                                if (defaultTaskType == TASK_SIMPLE_PLAYER_IN_CAR ||
                                                    primaryTaskType == TASK_SIMPLE_PLAYER_IN_CAR ||
                                                    defaultTaskType == TASK_SIMPLE_CAR_DRIVE ||
                                                    primaryTaskType == TASK_SIMPLE_CAR_DRIVE) {
                                                    if (otherPed && otherPed->m_pVehicle && otherPed->m_pVehicle->m_pDriver == otherPed) {
                                                        otherVeh = otherPed->m_pVehicle;
                                                    }
                                                }

                                                CVehicle* otherEnteringVeh = nullptr;
                                                if (primaryTaskType == TASK_COMPLEX_ENTER_CAR_AS_DRIVER) {
                                                    otherEnteringVeh = static_cast<CTaskComplexEnterCar*>(primaryTask)->GetTargetCar();
                                                }

                                                if (otherVeh == bestVeh) {
                                                    m_pPed->GetTaskManager().SetTask(new CTaskComplexEnterCarAsPassenger(bestVeh, 0, false), TASK_PRIMARY_PRIMARY);
                                                } else if (otherEnteringVeh == bestVeh || (!otherVeh && !otherEnteringVeh)) {
                                                    m_pPed->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver(bestVeh), TASK_PRIMARY_PRIMARY);
                                                }
                                            } else {
                                                m_pPed->GetTaskManager().SetTask(new CTaskComplexEnterCarAsDriver(bestVeh), TASK_PRIMARY_PRIMARY);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        m_bTryingToExitCar = false;
    }

    // Remote vehicle explosion handling
    if (m_bAfterRemoteVehicleExplosion) {
        if (CTimer::m_snPreviousTimeInMilliseconds - m_nTimeOfRemoteVehicleExplosion < 1000 &&
            CTimer::m_snTimeInMilliseconds - m_nTimeOfRemoteVehicleExplosion >= 1000) {
            if (!m_bCreateRemoteVehicleExplosion && m_bFadeAfterRemoteVehicleExplosion) {
                TheCamera.SetFadeColour(0, 0, 0);
                TheCamera.Fade(1.0f, eFadeFlag::FADE_OUT);
            }
        }
        if (CTimer::m_snTimeInMilliseconds - m_nTimeOfRemoteVehicleExplosion > 2000) {
            if (!m_bCreateRemoteVehicleExplosion && m_bFadeAfterRemoteVehicleExplosion) {
                TheCamera.RestoreWithJumpCut();
                TheCamera.SetFadeColour(0, 0, 0);
                TheCamera.Fade(1.0f, eFadeFlag::FADE_IN);
                TheCamera.Process();
                CTimer::Stop();
                CRenderer::RequestObjectsInFrustum(nullptr, 0);
                CStreaming::LoadAllRequestedModels(false);
                CTimer::Update();
            }
            m_bAfterRemoteVehicleExplosion = false;
            if (m_pRemoteVehicle) {
                m_pRemoteVehicle->m_bRemoveFromWorld = true;
                m_pRemoteVehicle = nullptr;
            }
        }
    }

    // Upside-down vehicle check (every 32 frames)
    if ((CTimer::m_FrameCounter & 31) == 0) {
        CPed* focusPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        if (focusPed && focusPed->bInVehicle && focusPed->m_pVehicle) {
            CVehicle* focusVeh = focusPed->m_pVehicle;
            if (focusVeh->m_matrix->GetUp().z < 0.0f && focusVeh->m_vecMoveSpeed.Magnitude() < 0.05f &&
                (focusVeh->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE || focusVeh->m_nVehicleSubType == VEHICLE_TYPE_BOAT) &&
                !focusVeh->physicalFlags.bSubmergedInWater)
            {
                if (focusVeh->m_matrix->GetUp().z < -0.5f) {
                    m_nTimesUpsideDownInARow += 2;
                } else {
                    m_nTimesUpsideDownInARow += 1;
                }
            } else {
                m_nTimesUpsideDownInARow = 0;
            }

            if (m_nTimesUpsideDownInARow > 6) {
                if (focusVeh->vehicleFlags.bCanBeDamaged) {
                    focusVeh->m_fHealth = std::min(focusVeh->m_fHealth, 249.0f);
                    if (focusVeh->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
                        auto* autoVeh = focusVeh->AsAutomobile();
                        autoVeh->m_damageManager.SetEngineStatus(225);
                        *(uint32*)&autoVeh->field_928 = 0;
                    }
                }
            }
        }
    }

    // Distance statistics and driving stats
    CPed* focusPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
    if (focusPed && focusPed->bInVehicle && focusPed->m_pVehicle) {
        CVehicle* focusVeh = focusPed->m_pVehicle;
        if (focusVeh->m_nModelIndex == MODEL_CADDY) {
            CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_GOLF_CART, focusVeh->m_fDistanceTravelled);
        } else if (focusVeh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BICYCLE, focusVeh->m_fDistanceTravelled);
        } else {
            switch (focusVeh->GetVehicleAppearance()) {
            case VEHICLE_APPEARANCE_HELI:
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_HELICOPTER, focusVeh->m_fDistanceTravelled);
                break;
            case VEHICLE_APPEARANCE_PLANE:
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_PLANE, focusVeh->m_fDistanceTravelled);
                break;
            case VEHICLE_APPEARANCE_AUTOMOBILE:
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_CAR, focusVeh->m_fDistanceTravelled);
                break;
            case VEHICLE_APPEARANCE_BIKE:
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_MOTORBIKE, focusVeh->m_fDistanceTravelled);
                break;
            case VEHICLE_APPEARANCE_BOAT:
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BOAT, focusVeh->m_fDistanceTravelled);
                break;
            default:
                break;
            }
        }

        if ((focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE || focusVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI) &&
            focusVeh->m_vecMoveSpeed.Magnitude() >= 0.1f)
        {
            CStats::IncrementStat(STAT_FLIGHT_TIME, CTimer::ms_fTimeStep * (1.0f / 3600.0f));
        }

        if (!CWorld::FindPlayerTrain(-1)) {
            if (focusVeh->m_vecMoveSpeed.SquaredMagnitude() > 0.0f) {
                switch (focusVeh->GetVehicleAppearance()) {
                case VEHICLE_APPEARANCE_HELI:
                case VEHICLE_APPEARANCE_PLANE:
                    CStats::UpdateStatsWhenFlying(focusVeh);
                    break;
                case VEHICLE_APPEARANCE_BIKE:
                    if (focusVeh->m_nVehicleSubType != VEHICLE_TYPE_BMX) {
                        CStats::UpdateStatsWhenOnMotorBike(focusVeh->AsBike());
                    }
                    break;
                default:
                    CStats::UpdateStatsWhenDriving(focusVeh);
                    break;
                }
            }
        }
    } else if (focusPed) {
        // Ped on foot
        if (focusPed->m_fDistanceTravelled > 0.0f) {
            if (focusPed->GetIntelligence()->GetTaskSwim()) {
                if (pad->GetPedWalkLeftRight() != 0 || pad->GetAccelerate() != 0) {
                    CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_SWIMMING, focusPed->m_fDistanceTravelled);
                }
            } else if (focusPed->GetIntelligence()->GetTaskJetPack()) {
                CStats::IncrementStat(STAT_TIME_ON_JETPACK, CTimer::ms_fTimeStep * (1.0f / 3600.0f));
            } else {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_ON_FOOT, focusPed->m_fDistanceTravelled);
            }
        }
    }

    // Police chase value calculation
    if (m_pPed->GetWanted()->m_nWantedLevel > 0 && !CTheScripts::IsPlayerOnAMission()) {
        static CVector s_LastPlayerPos{};
        static bool s_bMovedEnough = false;
        static bool s_bNearRoadNode = false;

        // Every 20 seconds:
        if (CTimer::m_snTimeInMillisecondsNonClipped / 20000 != CTimer::m_snPreviousTimeInMillisecondsNonClipped / 20000) {
            const CVector playerPos = FindPlayerCoors(-1);
            s_bMovedEnough = (DistanceBetweenPoints(s_LastPlayerPos, playerPos) >= 10.0f);
            s_LastPlayerPos = playerPos;

            CNodeAddress node = ThePaths.FindNodeClosestToCoors(playerPos, PATH_TYPE_PED, 60.0f, 1, 0, 0, 0, 0);
            s_bNearRoadNode = node.IsAreaValid();
        }

        float targetChaseValue = 0.0f;
        switch (m_pPed->GetWanted()->m_nWantedLevel) {
        case 1: targetChaseValue = 31.0f; break;
        case 2: targetChaseValue = 62.0f; break;
        case 3: targetChaseValue = 125.0f; break;
        case 4: targetChaseValue = 250.0f; break;
        case 5: targetChaseValue = 500.0f; break;
        case 6: targetChaseValue = 1000.0f; break;
        default: break;
        }

        const float diff = (targetChaseValue - m_fCurrentChaseValue) * CTimer::ms_fTimeStep * 0.0001f;
        if (diff < 0.0f) {
            m_fCurrentChaseValue += diff;
        } else {
            if (s_bMovedEnough && s_bNearRoadNode && !CCullZones::NoPolice() && !CCullZones::PoliceAbandonCars() && CGame::currArea == AREA_CODE_NORMAL) {
                m_fCurrentChaseValue += diff;
            }
        }
    } else {
        m_fCurrentChaseValue = 0.0f;
    }

    // Crosshair processing
    plugin::CallMethod<0x56EC80, void*, uint32, CPad*>(&m_nCrosshairActivated, playerIndex, pad);

    // Money cap
    m_nMoney = std::min(m_nMoney, 999'999'999);
    m_nDisplayMoney = std::min(m_nDisplayMoney, 999'999'999);
}

// 0x56F330
void CPlayerInfo::Clear() {
    // TODO: This should just use the constructor and `swap`,
    // like to just swap ourselves to a default constructed object
    // and do all the cleanup in the destructor

    m_pPed = nullptr;
    m_pRemoteVehicle = nullptr;
    if (m_pSpecCar) {
        m_pSpecCar->physicalFlags.bAddMovingCollisionSpeed = false;
        m_pSpecCar = nullptr;
    }
    m_nDisplayMoney = 0;
    m_nMoney = 0;
    m_nPlayerState = PLAYERSTATE_PLAYING;
    m_nCarDensityForCurrentZone = 0;
    m_fRoadDensityAroundPlayer = 1.0f;
    m_bAfterRemoteVehicleExplosion = false;
    m_bCreateRemoteVehicleExplosion = false;
    m_bFadeAfterRemoteVehicleExplosion = false;
    m_bTryingToExitCar = false;
    m_bTaxiTimerScore = false;
    m_nTaxiTimer = 0;
    m_nVehicleTimeCounter = CTimer::GetTimeInMS();
    m_nMaxArmour = 100;
    m_nMaxHealth = 100;
    m_bCanDoDriveBy = true;
    m_nCollectablesPickedUp = 0;
    m_nTotalNumCollectables = 3;
    m_nLastTimeEnergyLost = 0;
    m_nLastTimeArmourLost = 0;
    m_nLastTimeBigGunFired = 0;
    m_nTimesStuckInARow = 0;
    m_nTimesUpsideDownInARow = 0;
    m_nCarTwoWheelCounter = 0;
    m_fCarTwoWheelDist = 0.0f;
    m_nCarLess3WheelCounter = 0;
    m_nBikeRearWheelCounter = 0;
    m_fBikeRearWheelDist = 0.0f;
    m_nBikeFrontWheelCounter = 0;
    m_fBikeFrontWheelDist = 0.0f;
    m_nTempBufferCounter = 0;
    m_nBestCarTwoWheelsTimeMs = 0;
    m_fBestCarTwoWheelsDistM = 0.0f;
    m_nBestBikeWheelieTimeMs = 0;
    m_fBestBikeWheelieDistM = 0.0f;
    m_nBestBikeStoppieTimeMs = 0;
    m_fBestBikeStoppieDistM = 0.0f;
    m_bDoesNotGetTired = false;
    m_bFastReload = false;
    m_bFireProof = false;
    m_bGetOutOfJailFree = false;
    m_bFreeHealthCare = false;
    m_nTimeOfLastCarExplosionCaused = 0;
    m_nExplosionMultiplier = 0;
    m_nHavocCaused = 0;
    FindPlayerInfo().m_nNumHoursDidntEat = 0;
    m_nLastBustMessageNumber = 1;
    m_fCurrentChaseValue = 0.0f;
    m_nBustedAudioStatus = 0;
    m_nCrosshairActivated = 0;

    m_nRequireParachuteTimer = 0;

    if (m_bParachuteReferenced) {
        CGameLogic::IsCoopGameGoingOn();
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
    }
}

// 0x56EC40
void CPlayerInfo::GivePlayerParachute() const {
    if (m_nRequireParachuteTimer) {
        if (CStreaming::IsModelLoaded(MODEL_GUN_PARA)) {
            m_pPed->GiveWeapon(WEAPON_PARACHUTE, 1, true);
            m_pPed->SetSavedWeapon(WEAPON_PARACHUTE);
        }
    }
}

// 0x56EB30
void CPlayerInfo::StreamParachuteWeapon(bool unk) {
    if (CGameLogic::IsCoopGameGoingOn()) {
        if (unk) {
            return;
        }
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
        return;
    }

    if (m_pPed && m_pPed->IsInVehicle()) {
        if (m_pPed->m_pVehicle->IsSubPlane() || m_pPed->m_pVehicle->IsSubHeli()) {
            if (m_nRequireParachuteTimer <= (uint32)CTimer::GetTimeStepInMS()) {
                const auto groundHeight = TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
                const auto vehToGroundZDist = m_pPed->m_pVehicle->GetPosition().z - groundHeight;
                m_nRequireParachuteTimer = (vehToGroundZDist <= 50.f) ? 0 : 5000;
            } else {
                m_nRequireParachuteTimer -= (uint32)CTimer::GetTimeStepInMS();
            }
        }
    }

    if (m_nRequireParachuteTimer) {
        CStreaming::RequestModel(MODEL_GUN_PARA, STREAMING_MISSION_REQUIRED);
        m_bParachuteReferenced = true;
        return;
    }

    if (m_bParachuteReferenced) {
        CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
        m_bParachuteReferenced = false;
        m_nRequireParachuteTimer = 0;
    }
}

// 0x56EAB0
void CPlayerInfo::AddHealth(int32 amount) const {
    const auto newValue = std::min((float)m_nMaxHealth, m_pPed->m_fHealth + (float)amount); // Clamp to m_nMaxHealth
    m_pPed->m_fHealth = std::max(newValue, m_pPed->m_fHealth); // Don't change health to a lower value
}

// 0x56EA30
void CPlayerInfo::BlowUpRCBuggy(bool bExplode) const {
    if (m_pRemoteVehicle && !m_pRemoteVehicle->m_bRemoveFromWorld) {
        CRemote::TakeRemoteControlledCarFromPlayer(bExplode);
        if (bExplode)
            m_pRemoteVehicle->BlowUpCar(FindPlayerPed(), false);
    }
}

// 0x56E870
void CPlayerInfo::MakePlayerSafe(bool enable, float radius) {
    // Not quite SA, but this is the way to do it (instead of copy pasting it twice)
    auto& flags = m_pPed->physicalFlags;
    flags.bInvulnerable = enable;
    flags.bBulletProof = enable;
    flags.bFireProof = enable;
    flags.bExplosionProof = enable;
    flags.bCollisionProof = enable;
    flags.bMeleeProof = enable;
    m_PlayerData.m_bCanBeDamaged = !enable;
    m_PlayerData.m_pWanted->m_bEverybodyBackOff = enable;
    m_pPed->GetPadFromPlayer()->bPlayerSafe = enable;
    CWorld::SetAllCarsCanBeDamaged(!enable);

    if (enable) {
        CWorld::StopAllLawEnforcersInTheirTracks();
        CPad::StopPadsShaking();

        m_pPed->ClearAdrenaline();
        m_PlayerData.m_fTimeCanRun = std::max(m_PlayerData.m_fTimeCanRun, 0.f);
        m_pPed->GetIntelligence()->ClearTasks(true, false);

        gFireManager.ExtinguishPoint(GetPos(), radius);
        CExplosion::RemoveAllExplosionsInArea(GetPos(), 4000.f);
        CProjectileInfo::RemoveAllProjectiles();
        CWorld::ExtinguishAllCarFiresInArea(GetPos(), radius);
        CReplay::DisableReplays();
        m_pPed->ClearWeaponTarget();
    } else {
        CReplay::EnableReplays();
    }
}

// 0x56E830
void CPlayerInfo::PlayerFailedCriticalMission() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_FAILED_MISSION;
        CGameLogic::SetMissionFailed();
        CDarkel::ResetOnPlayerDeath();
    }
}

// 0x56E610
void CPlayerInfo::WorkOutEnergyFromHunger() {

    static auto& s_lastTimeHungryStateProcessedInitialized = StaticRef<bool>(0xB9B8F4); // false
    static auto& s_lastTimeHungryStateProcessed = StaticRef<uint8>(0xB9B8F2);
    static auto& s_LastHungryState = StaticRef<int8>(0xB9B8F1);
    static auto& s_bHungryMessageShown = StaticRef<bool>(0xB9B8F0);

    if (CCheat::IsActive(CHEAT_NEVER_GET_HUNGRY)) {
        return;
    }

    if (!s_lastTimeHungryStateProcessedInitialized) {
        s_lastTimeHungryStateProcessedInitialized = true;
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }

    auto pad = CPad::GetPad();
    if (   pad->ArePlayerControlsDisabled()
        || CMenuSystem::num_menus_in_use
        || TheCamera.m_bWideScreenOn
        || CCutsceneMgr::ms_running
        || CGameLogic::IsCoopGameGoingOn()
        || m_pRemoteVehicle
    ) {
        return;
    }

    if (!m_pPed)
        return;

    if (m_pPed->m_pAttachedTo)
        return;

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        if (!m_nNumHoursDidntEat)
            s_LastHungryState = 0;
        m_nNumHoursDidntEat += 1;
    }

    if (m_nNumHoursDidntEat <= 48) {
        s_bHungryMessageShown = false;
    } else {
        if (CClock::GetGameClockHours() == s_lastTimeHungryStateProcessed)
            return;

        m_pPed->Say(CTX_GLOBAL_STOMACH_RUMBLE);
        pad->StartShake(400, 110u, 0);

        if (s_bHungryMessageShown) {
            bool bDecreaseHealth{};
            if (CStats::GetStatValue(STAT_FAT) > 0.0f) {
                CStats::DecrementStat(STAT_FAT, 25.0f);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_FAT, 25.0f);
                bDecreaseHealth = true;
                if (!s_LastHungryState) {
                    s_LastHungryState = m_nNumHoursDidntEat + 24;
                }
            }

            if (CStats::GetStatValue(STAT_MUSCLE) <= 0.0f || m_nNumHoursDidntEat <= s_LastHungryState && s_LastHungryState) {
                if (!bDecreaseHealth) {
                    m_pPed->m_fHealth -= 2.0f;
                }
            } else {
                CStats::DecrementStat(STAT_MUSCLE, 25.0);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MUSCLE, 25.0f);
            }
        } else {
            CHud::SetHelpMessage(TheText.Get("NOTEAT"), true, false, true);
            s_bHungryMessageShown = true;
        }
    }

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }
}

// 0x56E5D0
void CPlayerInfo::ArrestPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_BEEN_ARRESTED;
        m_nBustedAudioStatus = 0;
        CDarkel::ResetOnPlayerDeath();
        CStats::IncrementStat(STAT_TIMES_BUSTED, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E580
void CPlayerInfo::KillPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_DIED;
        CDarkel::ResetOnPlayerDeath();
        CMessages::AddBigMessage(TheText.Get("DEAD"), 4000, STYLE_WHITE_MIDDLE);
        CStats::IncrementStat(STAT_NUMBER_OF_HOSPITAL_VISITS, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E570
bool CPlayerInfo::IsRestartingAfterMissionFailed() const {
    return m_nPlayerState == PLAYERSTATE_FAILED_MISSION;
}

// 0x56E560
bool CPlayerInfo::IsRestartingAfterArrest() const {
    return m_nPlayerState == PLAYERSTATE_HAS_BEEN_ARRESTED;
}

// 0x56E550
bool CPlayerInfo::IsRestartingAfterDeath() const {
    return m_nPlayerState == PLAYERSTATE_HAS_DIED;
}

// 0x56DAB0
bool CPlayerInfo::IsPlayerInRemoteMode() const {
    return m_pRemoteVehicle || m_bAfterRemoteVehicleExplosion;
}

// 0x56DFB0
// Return occupied vehicle's (if in any) or player's ped position
CVector CPlayerInfo::GetPos() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetPosition() : m_pPed->GetPosition();
}

// 0x56DF50
// Return occupied vehicle's (if in any) or player's ped move speed
CVector CPlayerInfo::GetSpeed() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetMoveSpeed() : m_pPed->GetMoveSpeed();
}

// 0x5D3B00
bool CPlayerInfo::Load() {
    CGenericGameStorage::LoadDataFromWorkBuffer<int32>(); // Discarded
    auto data = CGenericGameStorage::LoadDataFromWorkBuffer<CPlayerInfoSaveStructure>();
    data.Extract(this);
    return true;
}

// 0x5D3AC0
bool CPlayerInfo::Save() {
    CPlayerInfoSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(CPlayerInfoSaveStructure));
    CGenericGameStorage::SaveDataToWorkBuffer(data);
    return true;
}

// 0x45DEF0
CPlayerInfo& CPlayerInfo::operator=(const CPlayerInfo& rhs) {
    m_pPed                             = rhs.m_pPed;
    m_PlayerData                       = rhs.m_PlayerData;
    m_pRemoteVehicle                   = rhs.m_pRemoteVehicle;
    m_pSpecCar                         = rhs.m_pSpecCar;
    m_nMoney                           = rhs.m_nMoney;
    m_nDisplayMoney                    = rhs.m_nDisplayMoney;
    m_nCollectablesPickedUp            = rhs.m_nCollectablesPickedUp;
    m_nTotalNumCollectables            = rhs.m_nTotalNumCollectables;
    m_nLastBumpPlayerCarTimer          = rhs.m_nLastBumpPlayerCarTimer;
    m_nTaxiTimer                       = rhs.m_nTaxiTimer;
    m_nVehicleTimeCounter              = rhs.m_nVehicleTimeCounter;
    m_bTaxiTimerScore                  = rhs.m_bTaxiTimerScore;
    m_bTryingToExitCar                 = rhs.m_bTryingToExitCar;
    m_pLastTargetVehicle               = rhs.m_pLastTargetVehicle;
    m_nPlayerState                     = rhs.m_nPlayerState;
    m_bAfterRemoteVehicleExplosion     = rhs.m_bAfterRemoteVehicleExplosion;
    m_bCreateRemoteVehicleExplosion    = rhs.m_bCreateRemoteVehicleExplosion;
    m_bFadeAfterRemoteVehicleExplosion = rhs.m_bFadeAfterRemoteVehicleExplosion;
    m_nTimeOfRemoteVehicleExplosion    = rhs.m_nTimeOfRemoteVehicleExplosion;
    m_nLastTimeEnergyLost              = rhs.m_nLastTimeEnergyLost;
    m_nLastTimeArmourLost              = rhs.m_nLastTimeArmourLost;
    m_nLastTimeBigGunFired             = rhs.m_nLastTimeBigGunFired;
    m_nTimesUpsideDownInARow           = rhs.m_nTimesUpsideDownInARow;
    m_nTimesStuckInARow                = rhs.m_nTimesStuckInARow;
    m_nCarTwoWheelCounter              = rhs.m_nCarTwoWheelCounter;
    m_fCarTwoWheelDist                 = rhs.m_fCarTwoWheelDist;
    m_nCarLess3WheelCounter            = rhs.m_nCarLess3WheelCounter;
    m_nBikeRearWheelCounter            = rhs.m_nBikeRearWheelCounter;
    m_fBikeRearWheelDist               = rhs.m_fBikeRearWheelDist;
    m_nBikeFrontWheelCounter           = rhs.m_nBikeFrontWheelCounter;
    m_fBikeFrontWheelDist              = rhs.m_fBikeFrontWheelDist;
    m_nTempBufferCounter               = rhs.m_nTempBufferCounter;
    m_nBestCarTwoWheelsTimeMs          = rhs.m_nBestCarTwoWheelsTimeMs;
    m_fBestCarTwoWheelsDistM           = rhs.m_fBestCarTwoWheelsDistM;
    m_nBestBikeWheelieTimeMs           = rhs.m_nBestBikeWheelieTimeMs;
    m_fBestBikeWheelieDistM            = rhs.m_fBestBikeWheelieDistM;
    m_nBestBikeStoppieTimeMs           = rhs.m_nBestBikeStoppieTimeMs;
    m_fBestBikeStoppieDistM            = rhs.m_fBestBikeStoppieDistM;
    m_nCarDensityForCurrentZone        = rhs.m_nCarDensityForCurrentZone;
    m_fRoadDensityAroundPlayer         = rhs.m_fRoadDensityAroundPlayer;
    m_nTimeOfLastCarExplosionCaused    = rhs.m_nTimeOfLastCarExplosionCaused;
    m_nExplosionMultiplier             = rhs.m_nExplosionMultiplier;
    m_nHavocCaused                     = rhs.m_nHavocCaused;
    m_nNumHoursDidntEat                = rhs.m_nNumHoursDidntEat;
    m_fCurrentChaseValue               = rhs.m_fCurrentChaseValue;
    m_bDoesNotGetTired                 = rhs.m_bDoesNotGetTired;
    m_bFastReload                      = rhs.m_bFastReload;
    m_bFireProof                       = rhs.m_bFireProof;
    m_nMaxHealth                       = rhs.m_nMaxHealth;
    m_nMaxArmour                       = rhs.m_nMaxArmour;
    m_bGetOutOfJailFree                = rhs.m_bGetOutOfJailFree;
    m_bFreeHealthCare                  = rhs.m_bFreeHealthCare;
    m_bCanDoDriveBy                    = rhs.m_bCanDoDriveBy;
    m_nBustedAudioStatus               = rhs.m_nBustedAudioStatus;
    m_nLastBustMessageNumber           = rhs.m_nLastBustMessageNumber;
    m_nCrosshairActivated              = rhs.m_nCrosshairActivated;
    m_vecCrosshairTarget               = rhs.m_vecCrosshairTarget;
    m_pSkinTexture                     = rhs.m_pSkinTexture;
    m_bParachuteReferenced             = rhs.m_bParachuteReferenced;
    m_nRequireParachuteTimer           = rhs.m_nRequireParachuteTimer;
    strcpy_s(m_szSkinName, rhs.m_szSkinName);
    return *this;
}
