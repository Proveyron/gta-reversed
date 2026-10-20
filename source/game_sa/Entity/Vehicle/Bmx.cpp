#include "StdInc.h"
#include "Shadows.h"
#include "CarCtrl.h"

void CBmx::InjectHooks() {
    RH_ScopedVirtualClass(CBmx, 0x871528, 67);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6BF820);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6BF9B0);
    RH_ScopedVMTInstall(BurstTyre, 0x6BF9C0);
    RH_ScopedVMTInstall(FindWheelWidth, 0x6C0550);
    RH_ScopedVMTInstall(ProcessControl, 0x6BFA30);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6BFB50);
    RH_ScopedVMTInstall(PreRender, 0x6C0810);
    RH_ScopedVMTInstall(ProcessAI, 0x6C1470);
    RH_ScopedInstall(ProcessBunnyHop, 0x6C0590);
    RH_ScopedInstall(LaunchBunnyHopCB, 0x6C0390);
}

// 0x6BF820
CBmx::CBmx(int32 modelIndex, eVehicleCreatedBy createdBy) :
    CBike(modelIndex, createdBy) 
{
    auto mi                     = CModelInfo::GetModelInfo(modelIndex);
    m_nVehicleSubType           = VEHICLE_TYPE_BMX;
    m_RideAnimData.AnimGroup = CAnimManager::GetAnimBlocks()[mi->GetAnimFileIndex()].GroupId;
    if (m_RideAnimData.AnimGroup < ANIM_GROUP_BMX || m_RideAnimData.AnimGroup > ANIM_GROUP_CHOPPA) {
        m_RideAnimData.AnimGroup = ANIM_GROUP_BMX;
    }

    m_fControlJump     = 0.0f;
    m_fControlPedaling = 0.0f;
    m_fSprintLeanAngle = 0.0f;
    m_fCrankAngle      = 0.0f;
    m_fPedalAngleL     = 0.0f;
    m_fPedalAngleR     = 0.0f;
    m_nFixLeftHand     = false;
    m_nFixRightHand    = false;
    m_bIsFreewheeling  = false;

    const auto Calc = [&](eBmxNodes node) -> float {
        RwMatrix matrix;
        RwFrame* wheelFront = m_aBikeNodes[node];
        matrix              = *RwFrameGetMatrix(wheelFront);

        auto parent = RwFrameGetParent(wheelFront);
        if (parent) {
            do {
                RwMatrixTransform(&matrix, RwFrameGetMatrix(parent), rwCOMBINEPOSTCONCAT);
                parent = RwFrameGetParent(parent);
            } while (parent != wheelFront && parent);
        }
        return matrix.pos.y;
    };
    auto wheelFrontPosY = Calc(BMX_WHEEL_FRONT);
    auto wheelRearPosY  = Calc(BMX_WHEEL_REAR);

    m_fMidWheelDistY = wheelFrontPosY - wheelRearPosY;
    m_fMidWheelFracY = wheelFrontPosY / m_fMidWheelDistY;
}

// 0x6BF9D0
CBmx::~CBmx() {
    m_vehicleAudio.Terminate();
}

// 0x6BF9B0
bool CBmx::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6BF9C0
bool CBmx::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6BFA30
void CBmx::ProcessControl() {
    const float BMX_SPRINT_LEANSTART = FRAC_PI_2;
    const float BMX_PEDAL_LEANSTART  = 0.0f;
    const float BMX_SPRINT_LEANMULT  = 0.3f;
    const float MTB_SPRINT_LEANMULT  = 0.087f;
    const float BMX_PEDAL_LEANMULT   = 0.07f;
    const float MTB_PEDAL_LEANMULT   = 0.02f;

    CBike::ProcessControl();

    if (GetWasPostponed() || GetStatus() != STATUS_PLAYER || !m_pDriver) {
        return;
    }

    auto animBikeSprint = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_SPRINT);
    bool isMountainBike = GetModelId() == MODEL_MTBIKE;

    if (animBikeSprint && animBikeSprint->GetBlendAmount() > 0.01f) {
        float mult         = isMountainBike ? MTB_SPRINT_LEANMULT : BMX_SPRINT_LEANMULT;
        m_fSprintLeanAngle = std::sin(animBikeSprint->GetCurrentTime() / animBikeSprint->GetHier()->GetTotalTime() * TWO_PI + BMX_SPRINT_LEANSTART) * animBikeSprint->GetBlendAmount() * mult;
    } else {
        auto animBikePedal = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_PEDAL);
        if (animBikePedal && animBikePedal->GetBlendAmount() > 0.01f) {
            float mult = isMountainBike ? MTB_PEDAL_LEANMULT : BMX_PEDAL_LEANMULT;
            GetRideAnimData()->LeanAngle += std::sin(animBikePedal->GetCurrentTime() / animBikePedal->GetHier()->GetTotalTime() * TWO_PI + BMX_PEDAL_LEANSTART) * animBikePedal->GetBlendAmount() * mult;
        }
        m_fSprintLeanAngle *= 0.95f;
    }
}

// 0x6BFB50
void CBmx::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (m_bOffscreen && (!driver || !driver->IsPlayer())) {
        return;
    }

    m_nFixLeftHand = true;
    m_nFixRightHand = true;

    auto* bunnyHop = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_BUNNYHOP);
    if (bunnyHop) {
        m_fCrankAngle = (1.0f - bunnyHop->m_fBlendAmount) * m_fCrankAngle + HALF_PI * bunnyHop->m_fBlendAmount;
        m_fPedalAngleL = -m_fCrankAngle;
        m_fPedalAngleR = -m_fCrankAngle;
        return;
    }

    if (driver->m_nMoveState != PEDMOVE_NONE) {
        driver->SetMoveState(PEDMOVE_NONE);
    }

    const float forwardSpeed = DotProduct(m_vecMoveSpeed, GetForward());

    auto* animPedal   = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_PEDAL);
    auto* animSprint  = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_SPRINT);
    auto* animLeft    = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_LEFT);
    auto* animRight   = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_RIGHT);
    auto* animFwd     = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_FWD);
    auto* animDriveBy = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYLHS);
    if (!animDriveBy) {
        animDriveBy = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYRHS);
    }
    if (!animDriveBy) {
        animDriveBy = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYFT);
    }

    const float leanLimit = (m_fControlPedaling > 5.0f) ? 0.7f : 0.4f;

    if (std::abs(m_RideAnimData.LeanAngle) < leanLimit &&
        m_RideAnimData.LeanFwd < leanLimit &&
        forwardSpeed > 0.01f &&
        !animDriveBy)
    {
        float calculatedSpeed;
        float sprintSpeedThreshold;
        if (m_nModelIndex == MODEL_MTBIKE) {
            if (m_nCurrentGear < 1) {
                calculatedSpeed = 0.0f;
                sprintSpeedThreshold = 2.0f;
            } else {
                calculatedSpeed = (5.0f * forwardSpeed) / (float(m_nCurrentGear) * m_pHandling->m_transmissionData.m_MaxFlatVelocity - 0.25f);
                sprintSpeedThreshold = 2.0f;
            }
        } else {
            calculatedSpeed = 3.0f * forwardSpeed;
            sprintSpeedThreshold = 2.5f;
        }

        bool bBlendPedalOrSprint = false;
        if (m_fControlPedaling > 5.0f && calculatedSpeed >= sprintSpeedThreshold) {
            if (!animSprint || (animSprint->m_fBlendAmount < 1.0f && animSprint->m_fBlendDelta <= 0.0f)) {
                animSprint = CAnimManager::BlendAnimation(driver->GetRpClump(), m_RideAnimData.AnimGroup, ANIM_ID_BIKE_SPRINT, 4.0f);
                bBlendPedalOrSprint = true;
            }
            animSprint->SetFlag(ANIMATION_IS_PLAYING, true);
            if (animPedal) {
                animPedal->SetFlag(ANIMATION_IS_PLAYING, true);
            } else {
                animSprint->m_fSpeed = calculatedSpeed;
                animPedal = animSprint;
            }
        } else if (m_GasPedal == 0.0f && m_fControlPedaling > 0.0f && GetStatus() != STATUS_PLAYER) {
            if (!animPedal || (animPedal->m_fBlendAmount < 1.0f && animPedal->m_fBlendDelta <= 0.0f)) {
                animPedal = CAnimManager::BlendAnimation(driver->GetRpClump(), m_RideAnimData.AnimGroup, ANIM_ID_BIKE_PEDAL, 4.0f);
                bBlendPedalOrSprint = true;
            }
            animPedal->SetFlag(ANIMATION_IS_PLAYING, false);
            if (!vehicleFlags.bIsHandbrakeOn &&
                (m_aWheelRatios[0] < 1.0f || m_aWheelRatios[1] < 1.0f || m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f))
            {
                m_bIsFreewheeling = true;
            }
        } else {
            if (!animPedal || (animPedal->m_fBlendAmount < 1.0f && animPedal->m_fBlendDelta <= 0.0f)) {
                animPedal = CAnimManager::BlendAnimation(driver->GetRpClump(), m_RideAnimData.AnimGroup, ANIM_ID_BIKE_PEDAL, 4.0f);
                bBlendPedalOrSprint = true;
            }
            animPedal->SetFlag(ANIMATION_IS_PLAYING, true);
            animPedal->m_fSpeed = calculatedSpeed;
        }

        if (animPedal) {
            if (bBlendPedalOrSprint) {
                float targetAngle = -1000.0f;
                if (animLeft && animLeft->m_fBlendAmount >= 0.5f) {
                    targetAngle = PI;
                } else if (animRight && animRight->m_fBlendAmount >= 0.5f) {
                    targetAngle = 0.0f;
                } else if (animFwd && animFwd->m_fBlendAmount >= 0.5f) {
                    targetAngle = HALF_PI;
                }

                if (targetAngle != -1000.0f) {
                    float progress = -targetAngle * (1.0f / TWO_PI);
                    if (progress < 0.0f) {
                        progress += 1.0f;
                    }
                    animPedal->SetCurrentTime(progress * animPedal->m_pHierarchy->m_fTotalTime);
                    m_fCrankAngle = progress;
                } else {
                    m_fCrankAngle = -(animPedal->m_fCurrentTime / animPedal->m_pHierarchy->m_fTotalTime) * TWO_PI;
                }
            } else {
                m_fCrankAngle = -(animPedal->m_fCurrentTime / animPedal->m_pHierarchy->m_fTotalTime) * TWO_PI;
            }
        } else {
            m_fCrankAngle *= std::pow(0.97f, CTimer::GetTimeStep());
        }

        if (std::abs(m_RideAnimData.AnimLeanLeft) > 0.05f || std::abs(m_RideAnimData.AnimLeanFwd) > 0.05f) {
            m_RideAnimData.AnimLeanLeft *= 0.95f;
            m_RideAnimData.AnimLeanFwd *= 0.95f;
        }
    } else {
        if ((animPedal && animPedal->m_fBlendDelta >= 0.0f && animPedal->m_fBlendAmount > 0.0f) ||
            (animSprint && animSprint->m_fBlendDelta >= 0.0f && animSprint->m_fBlendAmount > 0.0f))
        {
            if (animPedal) {
                animPedal->SetFlag(ANIMATION_IS_PLAYING, false);
                animPedal->m_fBlendDelta = -8.0f;
            }
            if (animSprint) {
                animSprint->SetFlag(ANIMATION_IS_PLAYING, false);
                animSprint->m_fBlendDelta = -8.0f;
            }
            m_RideAnimData.AnimLeanLeft *= 0.95f;
            m_RideAnimData.AnimLeanFwd *= 0.95f;
        } else {
            CBike::ProcessRiderAnims(driver, this, &m_RideAnimData, m_BikeHandling, 0);
        }

        auto* activeAnim = animPedal ? animPedal : animSprint;
        if (activeAnim) {
            m_fCrankAngle = -(activeAnim->m_fCurrentTime / activeAnim->m_pHierarchy->m_fTotalTime) * TWO_PI;
        } else {
            m_fCrankAngle = 0.0f;
        }

        if (animLeft && animLeft->m_fBlendAmount > 0.1f) {
            m_fCrankAngle = (1.0f - animLeft->m_fBlendAmount) * m_fCrankAngle + PI * animLeft->m_fBlendAmount;
            m_bIsFreewheeling = true;
        } else if (animRight && animRight->m_fBlendAmount > 0.1f) {
            m_fCrankAngle = (1.0f - animRight->m_fBlendAmount) * m_fCrankAngle;
            m_bIsFreewheeling = true;
        } else if (animFwd && animFwd->m_fBlendAmount > 0.1f) {
            m_fCrankAngle = (1.0f - animFwd->m_fBlendAmount) * m_fCrankAngle + HALF_PI * animFwd->m_fBlendAmount;
        } else {
            m_fCrankAngle *= std::pow(0.97f, CTimer::GetTimeStep());
        }

        if (animDriveBy) {
            m_nFixRightHand = false;
            m_bIsFreewheeling = true;
        }
    }

    if (driver->IsPlayer()) {
        bikeFlags.bWheelieForCamera = false;
        if (m_WheelCounts[0] <= 0.0f &&
            m_WheelCounts[1] <= 0.0f &&
            GetForward().z > 0.0f &&
            (m_WheelCounts[2] == 0.0f || m_WheelCounts[3] == 0.0f) &&
            0.5f * m_BikeHandling->m_fWheelieAng < m_BikeHandling->m_fWheelieAng - GetForward().z)
        {
            bikeFlags.bWheelieForCamera = true;
        } else if (
            m_WheelCounts[2] <= 0.0f &&
            m_WheelCounts[3] <= 0.0f &&
            GetForward().z < 0.0f &&
            (m_WheelCounts[0] == 0.0f || m_WheelCounts[1] == 0.0f) &&
            0.6f * m_BikeHandling->m_fStoppieAng < m_BikeHandling->m_fStoppieAng - GetForward().z)
        {
            bikeFlags.bWheelieForCamera = true;
        }
    }

    m_fPedalAngleL = -m_fCrankAngle;
    m_fPedalAngleR = -m_fCrankAngle;
}

// data is a ptr to CBmx
// 0x6C0390
void CBmx::LaunchBunnyHopCB(CAnimBlendAssociation* assoc, void* data) {
    auto bmx = static_cast<CBmx*>(data);
    if ((bmx->m_WheelCounts[0] > 0.0f || bmx->m_WheelCounts[1] > 0.0f) &&
        (bmx->m_WheelCounts[2] > 0.0f || bmx->m_WheelCounts[3] > 0.0f)
    ) {
        auto power = std::min(bmx->m_fControlJump / 25.0f, 1.0f) + 1.0f;
        if (bmx->GetStatus() == STATUS_PLAYER) {
            power *= CStats::GetFatAndMuscleModifier(STAT_MOD_6);
        }
        if (CCheat::IsActive(CHEAT_HUGE_BUNNY_HOP)) {
            power *= 5.0f;
        }
        bmx->ApplyMoveForce(0.06f * bmx->m_fMass * power * bmx->m_matrix->GetUp());
        bmx->ApplyTurnForce(0.01f * bmx->m_fTurnMass * power * bmx->m_matrix->GetUp(), bmx->m_matrix->GetForward());
    }
}

// 0x6C0500 | inlined | see 0x6C11F3
void CBmx::GetFrameOffset(float& fZOffset, float& fAngleOffset) {
    const auto d1 = m_aWheelSuspensionHeights[0] - m_aWheelOrigHeights[0];
    const auto d2 = m_aWheelSuspensionHeights[1] - m_aWheelOrigHeights[1];

    fZOffset     = (1.0f - m_fMidWheelFracY) * d1 + d2 * m_fMidWheelFracY;
    fAngleOffset = std::atan2(d1 - d2, m_fMidWheelDistY);
}

// 0x6C0550
float CBmx::FindWheelWidth(bool bRear) {
    return 0.07f;
}

// 0x6C0560
void CBmx::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    // NOP
}

// 0x6C0590
void CBmx::ProcessBunnyHop() {
    auto* anim = m_pDriver
        ? RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_BUNNYHOP)
        : nullptr;

    if (GetStatus() != STATUS_PLAYER || !m_pDriver || !m_pDriver->IsPlayer()) {
        if (anim) {
            anim->SetFlag(ANIMATION_IS_PLAYING, true);
            anim->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE, true);
            anim->SetBlendDelta(-8.0f);
        }
        return;
    }

    auto pad = m_pDriver->AsPlayer()->GetPadFromPlayer();

    if (pad->IsLeftShoulder1Pressed() && !pad->DisablePlayerControls && m_fControlJump == 0.0f) {
        m_fControlJump += CTimer::GetTimeStep();
        anim = CAnimManager::BlendAnimation(m_pDriver->GetRpClump(), m_RideAnimData.AnimGroup, ANIM_ID_BIKE_BUNNYHOP, 8.0f);
        if (anim) {
            anim->SetCurrentTime(0.0f);
            anim->SetFlag(ANIMATION_IS_PLAYING, false);
        }
    }

    if (m_fControlJump > 0.0f) {
        if (!pad->DisablePlayerControls) {
            if (!anim) {
                m_fControlJump = 0.0f;
                return;
            }

            if (pad->IsLeftShoulder1()) {
                if (!anim->IsPlaying()) {
                    m_fControlJump = std::min(m_fControlJump + CTimer::GetTimeStep(), 25.0f);
                    anim->SetCurrentTime(m_fControlJump / 25.0f * 0.2f);
                }
            } else if (!anim->IsPlaying()) {
                if (anim->GetCurrentTime() < 0.2f) {
                    anim->SetCurrentTime((0.2f - anim->GetCurrentTime()) / 0.2f * (anim->GetHier()->GetTotalTime() - 0.2f) + 0.2f);
                }
                anim->SetFlag(ANIMATION_IS_PLAYING, true);
                anim->SetSpeed(1.5f);
                anim->SetFinishCallback(CBmx::LaunchBunnyHopCB, this);
            }
        } else {
            m_fControlJump = 0.0f;
        }
    }

    if (anim) {
        if (anim->GetBlendAmount() > 0.5f) {
            m_GasPedal                                   = 0.0f;
            FindPlayerPed()->GetPlayerData()->m_fMoveSpeed = 0.0f;
            if (!vehicleFlags.bIsHandbrakeOn && (m_aWheelRatios[0] < 1.0f || m_aWheelRatios[1] < 1.0f || m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f)) {
                m_bIsFreewheeling = true;
            }
        }
    }
}

// 0x6C0810
void CBmx::PreRender() {
    CVehicle::PreRender();

    auto* colModel = GetColModel();
    auto* colData  = colModel->m_pColData;

    if (vehicleFlags.bVehicleColProcessed) {
        DoBurstAndSoftGroundRatios();

        const float fSuspLenFront = 1.0f - (m_fSuspensionLength[0] / m_fLineLength[0]);
        const float minRatioFront = std::min(m_aWheelRatios[0], m_aWheelRatios[1]);
        const float fProgressFront = (minRatioFront - fSuspLenFront) / (1.0f - fSuspLenFront);
        float newHeightFront = colData->m_pLines[0].m_vecStart.z;
        if (fProgressFront > 0.0f) {
            newHeightFront -= fProgressFront * m_fSuspensionLength[0];
        }
        m_aWheelSuspensionHeights[0] += (newHeightFront - m_aWheelSuspensionHeights[0]) * 0.75f;

        const float fSuspLenRear = 1.0f - (m_fSuspensionLength[1] / m_fLineLength[1]);
        const float minRatioRear = std::min(m_aWheelRatios[2], m_aWheelRatios[3]);
        const float fProgressRear = (minRatioRear - fSuspLenRear) / (1.0f - fSuspLenRear);
        float newHeightRear = colData->m_pLines[2].m_vecStart.z;
        if (fProgressRear > 0.0f) {
            newHeightRear -= fProgressRear * m_fSuspensionLength[1];
        }
        m_aWheelSuspensionHeights[1] += (newHeightRear - m_aWheelSuspensionHeights[1]) * 0.75f;
    }

    const auto status = GetStatus();
    if (status == STATUS_PLAYER ||
        status == STATUS_PLAYER_PLAYBACK_FROM_BUFFER ||
        status == STATUS_SIMPLE ||
        status == STATUS_PHYSICS)
    {
        const float speedMag = m_vecMoveSpeed.Magnitude();
        for (int32 wheel = 0; wheel < 2; ++wheel) {
            int32 lineIdx;
            if (wheel == 0) {
                if (m_aWheelRatios[0] < 1.0f) {
                    lineIdx = 0;
                } else if (m_aWheelRatios[1] < 1.0f) {
                    lineIdx = 1;
                } else {
                    lineIdx = 0;
                }
            } else {
                if (m_aWheelRatios[3] < 1.0f) {
                    lineIdx = 3;
                } else if (m_aWheelRatios[2] < 1.0f) {
                    lineIdx = 2;
                } else {
                    lineIdx = 3;
                }
            }

            uint32 skidFlags = 0;
            if (wheel == 0 || m_WheelStates[1] == WHEEL_STATE_FIXED) {
                skidFlags = 4;
            }
            if (m_bWheelBloody[wheel]) {
                skidFlags |= 1;
            }
            if (m_bMoreSkidMarks[wheel]) {
                skidFlags |= 2;
            }

            const CVector offset = GetRight() * (std::sin(m_RideAnimData.LeanAngle) * colModel->m_boundBox.m_vecMin.z * 0.8f);
            CVector point = m_aWheelColPoints[lineIdx].m_vecPoint + offset;

            const float skidDir = (m_RideAnimData.LeanAngle > 0.0f) ? -1.0f : 1.0f;
            AddSingleWheelParticles(
                m_WheelStates[wheel],
                m_nWheelStatus[wheel],
                m_aWheelRatios[lineIdx],
                skidDir,
                &m_aWheelColPoints[lineIdx],
                &point,
                speedMag,
                wheel,
                m_aWheelSkidmarkType[wheel],
                &m_bWheelBloody[wheel],
                skidFlags
            );
        }
    }

    m_bLeanMatrixCalculated = false;
    CalculateLeanMatrix();
    CShadows::StoreShadowForVehicle(this, VEH_SHD_BIKE);

    auto* mi = GetVehicleModelInfo();

    const CVector steerDir(-std::sin(m_fSteerAngle), std::cos(m_fSteerAngle), 0.0f);
    const CVector rotatedSteerDir = GetMatrix().TransformVector(steerDir);

    if (m_WheelCounts[0] == 0.0f || m_WheelCounts[1] == 0.0f) {
        const float minRatioFront = std::min(m_aWheelRatios[0], m_aWheelRatios[1]);
        const CVector point(
            0.0f,
            (colData->m_pLines[0].m_vecStart.y + colData->m_pLines[1].m_vecStart.y) * 0.5f,
            colData->m_pLines[0].m_vecStart.z - minRatioFront * m_fSuspensionLength[0] - mi->m_fWheelSizeFront * 0.5f
        );
        const CVector speed = GetSpeed(point);
        m_aWheelAngularVelocity[0] = ProcessWheelRotation(m_WheelStates[0], rotatedSteerDir, speed, mi->m_fWheelSizeFront * 0.5f);
        m_aWheelPitchAngles[0] += m_aWheelAngularVelocity[0] * CTimer::GetTimeStep();
    }

    if (m_WheelCounts[2] == 0.0f || m_WheelCounts[3] == 0.0f) {
        const float minRatioRear = std::min(m_aWheelRatios[2], m_aWheelRatios[3]);
        const CVector point(
            0.0f,
            (colData->m_pLines[2].m_vecStart.y + colData->m_pLines[3].m_vecStart.y) * 0.5f,
            colData->m_pLines[2].m_vecStart.z - minRatioRear * m_fSuspensionLength[1] - mi->m_fWheelSizeRear * 0.5f
        );
        const CVector speed = GetSpeed(point);
        m_aWheelAngularVelocity[1] = ProcessWheelRotation(m_WheelStates[1], GetForward(), speed, mi->m_fWheelSizeRear * 0.5f);
        m_aWheelPitchAngles[1] += m_aWheelAngularVelocity[1] * CTimer::GetTimeStep();
    }

    CMatrix m1;
    if (m_aBikeNodes[BMX_FORKS_FRONT]) {
        CMatrix forksMat;
        forksMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_FORKS_FRONT]), false);
        const CVector origForksPos = forksMat.GetPosition();

        m1.SetUnity();
        m1.UpdateRW();

        CVector axis(0.0f, -std::sin(DegreesToRadians(mi->m_fBikeSteerAngle)), -std::cos(DegreesToRadians(mi->m_fBikeSteerAngle)));
        axis.Normalise();

        CQuaternion q;
        q.Set((RwV3d*)&axis, -m_RideAnimData.BarSteerAngle);
        RwMatrix qMat;
        q.Get(&qMat);
        m1.UpdateMatrix(&qMat);

        forksMat.SetUnity();
        forksMat *= m1;
        forksMat.GetPosition() += origForksPos;
        forksMat.UpdateRW();

        if (m_aBikeNodes[BMX_HANDLEBARS]) {
            CMatrix barsMat;
            barsMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_HANDLEBARS]), false);
            const CVector origBarsPos = barsMat.GetPosition();

            if (status == STATUS_ABANDONED || status == STATUS_WRECKED) {
                barsMat.SetUnity();
                barsMat *= m1;
                barsMat.GetPosition() += origBarsPos;
            } else {
                barsMat.SetTranslate(origBarsPos);
            }
            barsMat.UpdateRW();
        }
    }

    if (m_aBikeNodes[BMX_FORKS_REAR]) {
        CMatrix rearForksMat;
        rearForksMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_FORKS_REAR]), false);
        const CVector origRearForksPos = rearForksMat.GetPosition();

        const float angle = -std::atan((m_aWheelSuspensionHeights[1] - m_aWheelOrigHeights[1]) / m_fSwingArmLength);
        rearForksMat.SetRotate(angle, 0.0f, 0.0f);
        rearForksMat.GetPosition() += origRearForksPos;
        rearForksMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_WHEEL_FRONT]) {
        CMatrix wheelFrontMat;
        wheelFrontMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_WHEEL_FRONT]), false);
        const CVector origWheelFrontPos = wheelFrontMat.GetPosition();

        if (m_nWheelStatus[0] == 1) { // burst
            wheelFrontMat.SetRotate(m_aWheelPitchAngles[0], 0.0f, std::sin(m_aWheelPitchAngles[0]) * 0.02f);
        } else {
            wheelFrontMat.SetRotateX(m_aWheelPitchAngles[0]);
        }
        wheelFrontMat.GetPosition() += origWheelFrontPos;
        wheelFrontMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_WHEEL_REAR]) {
        CMatrix wheelRearMat;
        wheelRearMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_WHEEL_REAR]), false);
        const CVector origWheelRearPos = wheelRearMat.GetPosition();

        if (m_nWheelStatus[1] == 1) { // burst
            wheelRearMat.SetRotate(m_aWheelPitchAngles[1], 0.0f, std::sin(m_aWheelPitchAngles[1]) * 0.04f);
        } else {
            wheelRearMat.SetRotateX(m_aWheelPitchAngles[1]);
        }
        wheelRearMat.GetPosition() += origWheelRearPos;
        wheelRearMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_CHASSIS]) {
        float fZOffset = 0.0f;
        float fAngleOffset = 0.0f;
        GetFrameOffset(fZOffset, fAngleOffset);

        CMatrix chassisMat;
        chassisMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_CHASSIS]), false);
        CVector origChassisPos = chassisMat.GetPosition();

        origChassisPos.z += (1.0f - std::cos(m_RideAnimData.LeanAngle)) * colModel->m_boundBox.m_vecMin.z * 0.9f + fZOffset;
        chassisMat.SetRotateX(std::abs(m_RideAnimData.LeanAngle) * -0.05f + fAngleOffset);
        chassisMat.RotateY(m_fSprintLeanAngle + m_RideAnimData.LeanAngle);
        chassisMat.GetPosition() += origChassisPos;
        chassisMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_CHAINSET]) {
        CMatrix chainsetMat;
        chainsetMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_CHAINSET]), false);
        const CVector origChainsetPos = chainsetMat.GetPosition();

        chainsetMat.SetRotate(m_fCrankAngle, 0.0f, 0.0f);
        chainsetMat.GetPosition() += origChainsetPos;
        chainsetMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_PEDAL_R]) {
        CMatrix pedalRMat;
        pedalRMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_PEDAL_R]), false);
        const CVector origPedalRPos = pedalRMat.GetPosition();

        pedalRMat.SetRotate(m_fPedalAngleL, 0.0f, 0.0f);
        pedalRMat.GetPosition() += origPedalRPos;
        pedalRMat.UpdateRW();
    }

    if (m_aBikeNodes[BMX_PEDAL_L]) {
        CMatrix pedalLMat;
        pedalLMat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_PEDAL_L]), false);
        const CVector origPedalLPos = pedalLMat.GetPosition();

        pedalLMat.SetRotate(m_fPedalAngleR, 0.0f, 0.0f);
        pedalLMat.GetPosition() += origPedalLPos;
        pedalLMat.UpdateRW();
    }
}

// 0x6C1470
bool CBmx::ProcessAI(uint32& extraHandlingFlags) {
    m_autoPilot.m_nMovementFlags &= ~3;
    m_bIsFreewheeling = false;

    switch (GetStatus()) {
    case STATUS_PLAYER: {
        extraHandlingFlags += 2;
        bikeFlags.bGettingPickedUp = false;

        if (!m_pDriver) {
            return true;
        }
        if (!m_pDriver->IsPlayer()) {
            return false;
        }

        auto* playerDriver = m_pDriver->AsPlayer();
        ProcessControlInputs(playerDriver->m_nPlayerNum);
        auto* pad = playerDriver->GetPadFromPlayer();

        if (m_RideAnimData.LeanFwd > 0.0f) {
            m_vecCentreOfMass.z = m_pHandling->m_vecCentreOfMass.z + m_BikeHandling->m_fLeanBakCOM * m_RideAnimData.LeanFwd;
            if ((m_BrakePedal == 0.0f && !vehicleFlags.bIsHandbrakeOn) || !m_nBrakesOn) {
                const float speedMag = std::max(m_vecMoveSpeed.Magnitude(), 0.1f);
                const float gasFactor = (m_nModelIndex == MODEL_BIKE) ? (m_GasPedal * 0.7f + 0.3f) : ((m_GasPedal + 1.0f) * 0.5f);
                const float forceMag = speedMag * gasFactor * m_BikeHandling->m_fLeanBakForce * m_fTurnMass * m_RideAnimData.LeanFwd;
                ApplyTurnForce(GetUp() * (-CTimer::GetTimeStep() * forceMag), m_vecCentreOfMass + GetForward());
            }
        } else {
            m_vecCentreOfMass.z = m_pHandling->m_vecCentreOfMass.z + m_BikeHandling->m_fLeanFwdCOM * m_RideAnimData.LeanFwd;
            if (m_BrakePedal <= 0.0f || !m_nBrakesOn) {
                const float speedMag = std::max(m_vecMoveSpeed.Magnitude(), 0.1f);
                const float forceMag = speedMag * m_BikeHandling->m_fLeanFwdForce * m_fTurnMass * m_RideAnimData.LeanFwd;
                ApplyTurnForce(GetUp() * (-CTimer::GetTimeStep() * forceMag), m_vecCentreOfMass + GetForward());
            }
        }

        PruneReferences();
        if (GetStatus() == STATUS_PLAYER) {
            DoDriveByShootings();
        }
        DoSoftGroundResistance(extraHandlingFlags);

        if (m_aWheelRatios[0] == 1.0f &&
            m_aWheelRatios[1] == 1.0f &&
            m_aWheelRatios[2] == 1.0f &&
            m_aWheelRatios[3] == 1.0f)
        {
            const float turnUp = DotProduct(m_vecTurnSpeed, GetUp());
            const int16 steerLR = pad->GetSteeringLeftRight();
            if ((turnUp < 0.04f && steerLR > 0) || (turnUp > -0.04f && steerLR < 0)) {
                const float force = float(steerLR) * (1.0f / 128.0f) * m_fTurnMass * CTimer::GetTimeStep() * 0.002f;
                ApplyTurnForce(GetRight() * force, GetForward());
            }
        }

        ProcessBunnyHop();

        auto* animSprint = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_SPRINT);
        if (playerDriver->ControlButtonSprint(SPRINT_BMX) < 1.2f ||
            (playerDriver->GetButtonSprintResults(SPRINT_BMX) < 1.0f && (!animSprint || animSprint->m_fBlendAmount < 0.5f)))
        {
            const float sprintEnergy = 1.0f - float(pad->GetAccelerate()) * (1.0f / 255.0f) * 0.5f;
            playerDriver->HandleSprintEnergy(false, std::max(0.5f, sprintEnergy));

            if (playerDriver->GetButtonSprintResults(SPRINT_BMX) == 0.0f) {
                m_fControlPedaling = 4.9f;
                if (m_GasPedal == 0.0f && m_BrakePedal == 0.0f) {
                    m_GasPedal = 1.0f;
                }
            } else if (playerDriver->m_pPlayerData->m_fTimeCanRun > 0.0f) {
                m_fControlPedaling = pad->GetAccelerateJustDown() ? 4.9f : std::max(0.0f, m_fControlPedaling - 0.4f);
            } else {
                m_fControlPedaling = 0.0f;
            }
        } else {
            bikeFlags.bPlayerBoost = true;
            m_fControlPedaling = playerDriver->m_pPlayerData->m_fMoveSpeed;
        }

        CStats::UpdateStatsWhenCycling(bikeFlags.bPlayerBoost, this);

        if (pad->CarGunJustDown()) {
            ActivateBomb();
        }

        if (m_pDriver) {
            auto* animFwd = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_FWD);
            if (!animFwd) {
                animFwd = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_BACK);
            }
            auto* animDriveBy = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYLHS);
            if (!animDriveBy) {
                animDriveBy = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYRHS);
            }
            if (!animDriveBy) {
                animDriveBy = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_DRIVEBYFT);
            }

            if ((animFwd && animFwd->m_fBlendAmount >= 0.5f) || (animDriveBy && animDriveBy->m_fBlendAmount >= 0.5f)) {
                m_GasPedal = 0.0f;
                if (!vehicleFlags.bIsHandbrakeOn &&
                    (m_aWheelRatios[0] < 1.0f || m_aWheelRatios[1] < 1.0f || m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f))
                {
                    m_bIsFreewheeling = true;
                }
                return true;
            }
        }

        if (m_fControlPedaling > 5.0f && (m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f)) {
            float factor = std::clamp(2.4f - 1.5f * DotProduct(m_vecMoveSpeed, GetForward()) / m_pHandling->m_transmissionData.m_MaxFlatVelocity, 0.0f, 2.0f);
            if (GetStatus() == STATUS_PLAYER) {
                factor *= CStats::GetFatAndMuscleModifier(STAT_MOD_5);
            } else if (vehicleFlags.bUseCarCheats) {
                factor *= 1.25f;
            }
            ApplyMoveForce(GetForward() * (CTimer::GetTimeStep() * m_fMass * 0.3f * 0.008f * factor));
        }

        return true;
    }

    case STATUS_PLAYER_PLAYBACK_FROM_BUFFER: {
        extraHandlingFlags += 2;
        return true;
    }

    case STATUS_SIMPLE: {
        CCarAI::UpdateCarAI(this);
        CPhysical::ProcessControl();
        CCarCtrl::UpdateCarOnRails(this);

        auto* mi = GetVehicleModelInfo();
        m_pHandling->m_transmissionData.CalculateGearForSimpleCar(m_autoPilot.m_speed * 0.02f, m_nCurrentGear);
        m_aWheelPitchAngles[0] += ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeFront * 0.5f);
        m_aWheelPitchAngles[1] += ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeRear * 0.5f);
        PlayHornIfNecessary();
        ReduceHornCounter();
        vehicleFlags.bVehicleColProcessed = false;
        vehicleFlags.bAudioChangingGear = false;
        bikeFlags.bWheelieForCamera = false;
        m_fControlPedaling = 0.0f;
        return true;
    }

    case STATUS_PHYSICS: {
        CCarAI::UpdateCarAI(this);
        CCarCtrl::SteerAICarWithPhysics(this);
        PlayHornIfNecessary();
        extraHandlingFlags += 2;
        bikeFlags.bWheelieForCamera = false;

        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal = 0.0f;
            m_BrakePedal = 1.0f;
        } else {
            bikeFlags.bGettingPickedUp = false;
        }

        if (m_fControlPedaling > 0.0f) {
            const float decay = CTimer::GetTimeStep() * (m_fControlPedaling <= 5.0f ? 0.02f : (vehicleFlags.bUseCarCheats ? 0.02f : 0.01f));
            m_fControlPedaling = std::max(0.0f, m_fControlPedaling - decay);
        }
        return true;
    }

    case STATUS_ABANDONED: {
        m_BrakePedal = 0.0f;
        vehicleFlags.bIsHandbrakeOn = !(m_vecMoveSpeed.SquaredMagnitude() < 0.01f && !bikeFlags.bGettingPickedUp);
        m_GasPedal = 0.0f;
        m_HornCounter = 0;

        if (m_pDriver || m_apPassengers[0] || vehicleFlags.bIsBeingCarJacked) {
            if (!bikeFlags.bGettingPickedUp) {
                extraHandlingFlags += 2;
            }
        }

        bikeFlags.bWheelieForCamera = false;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd = 0.0f;
        m_fControlPedaling = 0.0f;

        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal = 0.0f;
            m_BrakePedal = 1.0f;
        }
        return true;
    }

    case STATUS_WRECKED: {
        vehicleFlags.bIsHandbrakeOn = true;
        bikeFlags.bWheelieForCamera = false;
        m_BrakePedal = 0.05f;
        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;
        m_HornCounter = 0;
        m_fControlPedaling = 0.0f;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd = 0.0f;
        return true;
    }

    case STATUS_FORCED_STOP: {
        if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f) {
            m_BrakePedal = 1.0f;
            vehicleFlags.bIsHandbrakeOn = true;
        } else {
            m_BrakePedal = 0.0f;
            vehicleFlags.bIsHandbrakeOn = false;
        }
        extraHandlingFlags += 2;
        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;
        m_HornCounter = 0;
        bikeFlags.bWheelieForCamera = false;
        m_fControlPedaling = 0.0f;
        return true;
    }

    default:
        return true;
    }
}
