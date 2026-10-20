#include "StdInc.h"

#include "AEPedAudioEntity.h"
#include "AEAudioHardware.h"
#include "AEAudioUtility.h"

void CAEPedAudioEntity::InjectHooks() {
    RH_ScopedClass(CAEPedAudioEntity);
    RH_ScopedCategory("Audio/Entities");

    //RH_ScopedInstall(Constructor, 0x5DE8D0, { .Reversed = false });

    RH_ScopedInstall(Initialise, 0x4E0E80);
    RH_ScopedInstall(StaticInitialise, 0x5B98A0);
    RH_ScopedInstall(Terminate, 0x4E1360);
    RH_ScopedInstall(AddAudioEvent, 0x4E2BB0);
    RH_ScopedInstall(TurnOnJetPack, 0x4E28A0);
    RH_ScopedInstall(TurnOffJetPack, 0x4E2A70);
    RH_ScopedInstall(StopJetPackSound, 0x4E1120);
    RH_ScopedInstall(UpdateJetPack, 0x4E0EE0);
    RH_ScopedInstall(PlayWindRush, 0x4E1170);
    RH_ScopedInstall(UpdateParameters, 0x4E1180);
    RH_ScopedInstall(HandleFootstepEvent, 0x4E13A0);
    RH_ScopedInstall(HandleSkateEvent, 0x4E17E0);
    RH_ScopedInstall(HandleLandingEvent, 0x4E18E0);
    RH_ScopedInstall(HandlePedSwing, 0x4E1A40);
    RH_ScopedInstall(HandlePedHit, 0x4E1CC0);
    RH_ScopedInstall(HandlePedJacked, 0x4E2350);
    RH_ScopedInstall(HandleSwimSplash, 0x4E26A0);
    RH_ScopedInstall(HandleSwimWake, 0x4E2790);
    RH_ScopedInstall(PlayShirtFlap, 0x4E2A90);
    RH_ScopedInstall(Service, 0x4E2EE0);
}

// 0x5DE8D0
CAEPedAudioEntity::CAEPedAudioEntity() : CAEAudioEntity() {
    m_pPed = nullptr;
    m_bCanAddEvent = false;

    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_JetPackSound2 = nullptr;
}

// (CEntity* entity)
// 0x4E0E80
void CAEPedAudioEntity::Initialise(CPed* ped) {
    m_pPed = ped;
    m_nSfxId = 0;
    m_LastSwimWakeTriggerTimeMs = 0;

    m_bJetPackPlaying = false;
    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;

    field_150 = nullptr;
    field_154 = -100.0f;
    field_158 = -100.0f;
    m_bCanAddEvent = true;
}

// 0x5B98A0
void CAEPedAudioEntity::StaticInitialise() {
    AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING);
}

// 0x4E1360
void CAEPedAudioEntity::Terminate() {
    m_bCanAddEvent = false;
    m_pPed   = nullptr;
    StopJetPackSound();
    AESoundManager.CancelSoundsOwnedByAudioEntity(this, true);
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.StopSoundAndForget();
    }
}

// 0x4E2BB0
void CAEPedAudioEntity::AddAudioEvent(eAudioEvents event, float volume, float speed, CPhysical* ped, eSurfaceType surfaceId, int32 a7, uint32 maxVol) {
    if (!m_bCanAddEvent)
        return;

    if (!m_pPed)
        return;

    switch (event) {
    case AE_PED_FOOTSTEP_LEFT:
    case AE_PED_FOOTSTEP_RIGHT:
        HandleFootstepEvent(event, volume, speed, surfaceId);
        break;
    case AE_PED_SKATE_LEFT:
    case AE_PED_SKATE_RIGHT:
        HandleSkateEvent(event, volume, speed);
        break;
    case AE_PED_LAND_ON_FEET_AFTER_FALL:
    case AE_PED_COLLAPSE_AFTER_FALL:
        HandleLandingEvent(event);
        break;
    case AE_PED_SWING:
        HandlePedSwing(event, a7, maxVol);
        break;
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
        HandlePedHit(event, ped, surfaceId, volume, maxVol);
        break;
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        HandlePedJacked(event);
        break;
    case AE_PED_SWIM_STROKE_SPLASH:
    case AE_PED_SWIM_DIVE_SPLASH:
        HandleSwimSplash(event);
        break;
    case AE_PED_SWIM_WAKE:
        HandleSwimWake(event);
        break;
    case AE_PED_CRUNCH: {
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS)) {
            break;
        }

        volume += GetDefaultVolume(AE_PED_CRUNCH);

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_CRUNCH, this)) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = 29,
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = 0.06f,
            .PlayTime          = 0,
            .EventID           = AE_PED_CRUNCH
        });

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_KNOCK_DOWN, this) != 0) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(47, 49)),
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = 0.06f,
            .PlayTime          = 0,
            .EventID           = AE_PED_KNOCK_DOWN
        });
        break;
    }
    default:
        break;
    }
}

// 0x4E28A0
void CAEPedAudioEntity::TurnOnJetPack() {
    if (!m_pPed)
        return;

    if (m_bJetPackPlaying || m_JetPackSound1 || m_JetPackSound0 || m_JetPackSound2)
        return;

    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;
    m_fVolume3 = +2.000f;
    m_JetPackSoundSpeedMult = 0.400f;

    m_bJetPackPlaying = true;

    const auto PlayJetPackSound = [&](eSoundBankSlot slot, eSoundID soundID) {
        return AESoundManager.PlaySound({
            .BankSlotID  = slot,
            .SoundID     = soundID,
            .AudioEntity = this,
            .Pos         = m_pPed->GetPosition(),
            .Volume      = -100.f,
            .Flags       = SOUND_REQUEST_UPDATES,
        });
    };
    m_JetPackSound0 = PlayJetPackSound(SND_BANK_SLOT_VEHICLE_GEN, 26);
    m_JetPackSound1 = PlayJetPackSound(SND_BANK_SLOT_WEAPON_GEN, 10);
    m_JetPackSound2 = PlayJetPackSound(SND_BANK_SLOT_FRONTEND_GAME, 0);
}

// 0x4E2A70
void CAEPedAudioEntity::TurnOffJetPack() {
    StopJetPackSound();
    m_bJetPackPlaying = false;
}

// 0x4E1120
void CAEPedAudioEntity::StopJetPackSound() {
    if (m_JetPackSound0) {
        m_JetPackSound0->StopSoundAndForget();
        m_JetPackSound0 = nullptr;
    }

    if (m_JetPackSound1) {
        m_JetPackSound1->StopSoundAndForget();
        m_JetPackSound1 = nullptr;
    }

    if (m_JetPackSound2) {
        m_JetPackSound2->StopSoundAndForget();
        m_JetPackSound2 = nullptr;
    }
}

// 0x4E0EE0
void CAEPedAudioEntity::UpdateJetPack(float thrustFwd, float thrustAngle) {
    if (!m_bJetPackPlaying || !m_JetPackSound1 || !m_JetPackSound0 || !m_JetPackSound2) {
        return;
    }

    if (thrustFwd <= 0.5f) { // flying
        m_fVolume1 = std::max(m_fVolume1 - 5.0f, -100.0f);
        m_fVolume2 = std::min(m_fVolume2 + 6.0f, -17.0f);
        m_fVolume3 = std::max(m_fVolume3 - 0.3f, 2.0f);
        m_JetPackSoundSpeedMult = std::max(m_JetPackSoundSpeedMult - 0.031f, 0.4f);
    } else { // idle
        m_fVolume1 = std::min(m_fVolume1 + 15.0f, -15.0f);
        m_fVolume2 = std::max(m_fVolume2 - 7.1f, -100.0f);
        m_fVolume3 = std::min(m_fVolume3 + 0.3f, 11.0f);
        m_JetPackSoundSpeedMult = std::min(m_JetPackSoundSpeedMult + 0.031f, 0.71f);
    }

    const auto angle = std::sin(thrustAngle);
    const float speed = angle < 0.0f ? -angle : angle; // maybe wrong

    // 0.0f == 0xB61384 (uninitialized)
    m_JetPackSound0->m_Volume = m_fVolume1 + 0.0f;
    m_JetPackSound0->m_Speed = speed * -0.07f + 1.0f;

    m_JetPackSound1->m_Volume = m_fVolume2 + 0.0f;
    m_JetPackSound1->m_Speed = 0.56f;

    m_JetPackSound2->m_Volume = m_fVolume3 + 0.0f;
    m_JetPackSound2->m_Speed = (speed / 5.0f + 1.0f) * m_JetPackSoundSpeedMult;
}

// 0x4E1170
void CAEPedAudioEntity::PlayWindRush(float, float) {
    // NOP
}

// 0x4E1180
void CAEPedAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (sound == m_JetPackSound1) {
        if (curPlayPos == -1) {
            m_JetPackSound1 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound0) {
        if (curPlayPos == -1) {
            m_JetPackSound0 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound2) {
        if (curPlayPos == -1) {
            m_JetPackSound2 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    // shit
    if (sound == field_150) {
        if (curPlayPos == -1) {
            field_150 = nullptr;
        }
        return;
    }

    if (curPlayPos < 0)
        return;

    switch (sound->m_Event) {
    case AE_PED_SWING:
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        if (CTimer::GetTimeInMS() < (uint32)sound->m_ClientVariable)
            return;
        sound->m_Speed = 1.0f;
        return;
    case AE_PED_SWIM_WAKE: {
        const auto volume = GetDefaultVolume(AE_PED_SWIM_WAKE);

        if (CTimer::GetTimeInMS() <= m_LastSwimWakeTriggerTimeMs + 100) {
            if (sound->m_Volume >= volume) {
                return;
            }

            if (sound->m_Volume + 0.6f >= volume) {
                sound->m_Volume = volume;
                return;
            }
        } else {
            auto vol = volume - 20.0f;
            if (sound->m_Volume <= vol) {
                sound->StopSoundAndForget();
                m_LastSwimWakeTriggerTimeMs = 0;
                return;
            }
            sound->m_Volume = std::max(sound->m_Volume - 0.6f, vol);
        }

        return;
    }
    default:
        return;
    }
}

// 0x4E13A0
void CAEPedAudioEntity::HandleFootstepEvent(eAudioEvents event, float volume, float speed, eSurfaceType forcedSurfaceType) {
    volume += GetDefaultVolume(event);

    if (m_pPed->bIsInTheAir) {
        return;
    }

    const auto PlayFootstepSound = [&] (eSoundBank bank, eSoundBankSlot slot, eSoundID sfx, int16 playTime = 0) {
        if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, slot)) {
            AESoundManager.PlaySound({
                .BankSlotID         = slot,
                .SoundID            = sfx,
                .AudioEntity        = this,
                .Pos                = m_pPed->GetPosition(),
                .Volume             = volume,
                .Speed              = speed,
                .Flags              = SOUND_START_PERCENTAGE,
                .FrequencyVariance  = 0.0588f,
                .PlayTime           = playTime,
                .RegisterWithEntity = m_pPed,
            });
        } else {
            AEAudioHardware.LoadSoundBank(bank, slot);
        }
    };

    const auto PlayRandomGenericFootstepSound = [&] () {
        PlayFootstepSound(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(1, 5)));
    };

    if (FindPlayerPed(-1) != m_pPed) {
        PlayRandomGenericFootstepSound();
    } else {
        const auto TryPlayPlayerFootstepSound = [&] (eSoundBank bank, eSoundBankSlot slot, eSoundID sfx, bool needCancelSoundsInSlot = false, int16 playTime = 0) {
            if (AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
                PlayFootstepSound(bank, slot, sfx);
                return true;
            } else {
                if (needCancelSoundsInSlot) {
                    if (AESoundManager.AreSoundsPlayingInBankSlot(slot) - 1 <= 1) {
                        AESoundManager.CancelSoundsInBankSlot(slot, false);
                    }
                }
                AEAudioHardware.LoadSoundBank(bank, slot);
                PlayRandomGenericFootstepSound();
                return false;
            }
        };
        if (g_surfaceInfos.IsAudioWater(forcedSurfaceType) || g_surfaceInfos.IsAudioWater(m_pPed->m_nContactSurface)) { // 0x4E1703
            TryPlayPlayerFootstepSound(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), false, 50);
        } else if (g_surfaceInfos.IsAudioConcrete(m_pPed->m_nContactSurface)) { // 0x4E145E
            PlayRandomGenericFootstepSound();
        } else if (g_surfaceInfos.IsAudioGrass(m_pPed->m_nContactSurface) || g_surfaceInfos.IsAudioLongGrass(m_pPed->m_nContactSurface)) { // 0x4E16BA
            TryPlayPlayerFootstepSound(SND_BANK_FEET_GRASS, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioSand(m_pPed->m_nContactSurface)) { // 0x4E14B7
            TryPlayPlayerFootstepSound(SND_BANK_FEET_SAND, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 3)), true);
        } else if (g_surfaceInfos.IsAudioGravel(m_pPed->m_nContactSurface)) { // 0x4E1531
            TryPlayPlayerFootstepSound(SND_BANK_FEET_GRAVEL, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioWood(m_pPed->m_nContactSurface)) { // 0x4E157D
            TryPlayPlayerFootstepSound(SND_BANK_FEET_WOOD, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioTile(m_pPed->m_nContactSurface)) { // 0x4E15DB
            TryPlayPlayerFootstepSound(SND_BANK_FEET_TILE, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else if (g_surfaceInfos.IsAudioMetal(m_pPed->m_nContactSurface)) { // 0x4E1636
            TryPlayPlayerFootstepSound(SND_BANK_FEET_METAL, SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), true);
        } else { // 0x4E168F
            PlayRandomGenericFootstepSound();
        }
    }
}

// 0x4E17E0
void CAEPedAudioEntity::HandleSkateEvent(eAudioEvents event, float volume, float speed) {
    if (m_pPed->bIsInTheAir) {
        return;
    }
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        return;
    }
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_FOOTSTEPS_GENERIC,
        .SoundID            = (eSoundID)(event == AE_PED_SKATE_LEFT ? 7 : 8),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = GetDefaultVolume(event) + (float)(CAEAudioUtility::GetRandomNumberInRange(-3, 3)) + volume,
        .Speed              = speed,
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E18E0
void CAEPedAudioEntity::HandleLandingEvent(eAudioEvents event) {
    if (m_pPed->bIsInTheAir) {
        return;
    }

    const auto PlayLandingSound = [&](eSoundBankSlot slot, eSoundID soundID, float volume, int16 playPos) {
        AESoundManager.PlaySound({
            .BankSlotID = slot,
            .SoundID            = soundID,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = volume,
            .Flags              = SOUND_START_PERCENTAGE,
            .FrequencyVariance  = 0.0588f,
            .PlayTime           = playPos,
            .RegisterWithEntity = m_pPed
        });
    };
    const auto volume = GetDefaultVolume(event);
    if (g_surfaceInfos.IsAudioWater(m_pPed->m_nContactSurface)) {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
            PlayLandingSound(
                SND_BANK_SLOT_SWIMMING,
                CAEAudioUtility::GetRandomNumberInRange<eSoundID>(SND_GENRL_SWIMMING_SWIM1, SND_GENRL_SWIMMING_SWIM5),
                std::max(0.f, volume),
                50
            );
        }
    } else {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
            PlayLandingSound(
                SND_BANK_SLOT_FOOTSTEPS_GENERIC,
                event == AE_PED_LAND_ON_FEET_AFTER_FALL ? 6 : 0,
                volume,
                0
            );
        }
    }
}

// 0x4E1A40
void CAEPedAudioEntity::HandlePedSwing(eAudioEvents event, int32 a3, uint32 volume) {
    if (!m_pPed) {
        return;
    }

    CWeapon& activeWeapon = m_pPed->GetActiveWeapon();
    if (!&activeWeapon) {
        return;
    }

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    int16 sfxId = -1;
    float volumeOffset = 0.0f;
    float speed = 1.0f;
    const float defaultVolume = GetDefaultVolume(event);

    const auto weaponType = activeWeapon.m_Type;
    switch (weaponType) {
    case WEAPON_GOLFCLUB:
    case WEAPON_CANE:
        sfxId = 47;
        break;
    case WEAPON_NIGHTSTICK:
    case WEAPON_POOL_CUE:
        sfxId = 86;
        speed = 0.84f;
        break;
    case WEAPON_KNIFE:
    case WEAPON_KATANA:
        sfxId = 88;
        volumeOffset = -3.0f;
        break;
    case WEAPON_BASEBALLBAT:
    case WEAPON_SHOVEL:
        sfxId = 86;
        speed = 0.67f;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        sfxId = 86;
        speed = 0.84f;
        volumeOffset = -6.0f;
        break;
    default:
        if ((weaponType >= WEAPON_UNARMED && weaponType <= WEAPON_BRASSKNUCKLE) ||
            weaponType == WEAPON_CHAINSAW ||
            weaponType == WEAPON_FLOWERS ||
            (weaponType >= WEAPON_GRENADE && weaponType <= WEAPON_MOLOTOV) ||
            (weaponType >= WEAPON_PISTOL && weaponType <= WEAPON_SNIPER_RIFLE) ||
            (weaponType >= WEAPON_FLAMETHROWER && weaponType <= WEAPON_PARACHUTE)) {
            sfxId = 87;
            speed = 0.84f;
            volumeOffset = -6.0f;
        }
        break;
    }

    if (a3 >= 64 && a3 <= 68) {
        sfxId = 87;
        speed = 0.84f;
        if (a3 <= 66) {
            volumeOffset = -6.0f;
        } else {
            volumeOffset = -2.0f;
        }
    }

    if (sfxId < 0) {
        return;
    }

    m_tempSound.Initialise(
        SND_BANK_SLOT_WEAPON_GEN,
        static_cast<eSoundID>(sfxId),
        this,
        m_pPed->GetPosition(),
        defaultVolume + volumeOffset,
        1.0f,
        speed,
        1.0f,
        0,
        SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
        0.04f,
        0
    );

    m_tempSound.RegisterWithPhysicalEntity(m_pPed);

    if (volume > 0) {
        m_tempSound.m_Speed = 0.0f;
        m_tempSound.m_SpeedVariance = 0.0f;
        m_tempSound.m_ClientVariable = static_cast<float>(CTimer::GetTimeInMS() + volume);
        m_tempSound.m_Event = event;
        m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, 1);
    }

    AESoundManager.RequestNewSound(&m_tempSound);
}

// 0x4E1CC0
void CAEPedAudioEntity::HandlePedHit(eAudioEvents event, CPhysical* physical, uint8 surfaceId, float volume, uint32 maxVol) {
    if (!m_pPed) {
        return;
    }

    CWeapon& activeWeapon = m_pPed->GetActiveWeapon();
    if (!&activeWeapon) {
        return;
    }

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    const float defaultVolume = GetDefaultVolume(event);
    const float finalVolume = volume + defaultVolume;

    int16 sfxId = -1;
    float freqVariance = 0.0588f;

    const auto weaponType = activeWeapon.m_Type;
    switch (weaponType) {
    case WEAPON_UNARMED:
    case WEAPON_NIGHTSTICK:
    case WEAPON_PARACHUTE:
        sfxId = 58;
        break;
    case WEAPON_BRASSKNUCKLE:
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_PISTOL:
    case WEAPON_PISTOL_SILENCED:
    case WEAPON_DESERT_EAGLE:
    case WEAPON_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_MICRO_UZI:
    case WEAPON_MP5:
    case WEAPON_AK47:
    case WEAPON_M4:
    case WEAPON_TEC9:
    case WEAPON_COUNTRYRIFLE:
    case WEAPON_SNIPER_RIFLE:
    case WEAPON_FLAMETHROWER:
    case WEAPON_MINIGUN:
    case WEAPON_SATCHEL_CHARGE:
    case WEAPON_DETONATOR:
    case WEAPON_SPRAYCAN:
    case WEAPON_EXTINGUISHER:
    case WEAPON_CAMERA:
    case WEAPON_NIGHTVISION:
    case WEAPON_INFRARED:
        sfxId = 44;
        break;
    case WEAPON_GOLFCLUB:
        sfxId = 48;
        break;
    case WEAPON_KNIFE:
        sfxId = 35;
        break;
    case WEAPON_BASEBALLBAT:
        sfxId = 57;
        freqVariance = 0.03f;
        break;
    case WEAPON_SHOVEL:
        sfxId = 34;
        break;
    case WEAPON_POOL_CUE:
        sfxId = 75;
        break;
    case WEAPON_KATANA:
        sfxId = 67;
        break;
    case WEAPON_CHAINSAW:
        sfxId = 56;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        sfxId = 50;
        break;
    case WEAPON_FLOWERS:
        sfxId = 54;
        freqVariance = 0.07f;
        break;
    case WEAPON_CANE:
        sfxId = 46;
        break;
    default:
        sfxId = -1;
        break;
    }

    int16 sfxId2 = -1;
    float volOffset1 = 0.0f;
    float volOffset2 = -3.0f;
    eSoundBankSlot bankSlot1 = SND_BANK_SLOT_WEAPON_GEN;

    if (physical && physical->IsPed()) {
        switch (event) {
        case AE_PED_HIT_HIGH: {
            switch (weaponType) {
            case WEAPON_BRASSKNUCKLE:
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
                volOffset1 = 1.0f;
                break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxId2 = 81;
                volOffset2 = -9.0f;
                break;
            case WEAPON_BASEBALLBAT:
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
                volOffset1 = -10.0f;
                break;
            case WEAPON_CHAINSAW: {
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_FRONTEND_GAME)) {
                    return;
                }
                bankSlot1 = SND_BANK_SLOT_FRONTEND_GAME;
                sfxId = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(7, 9));
                sfxId2 = -1;
                break;
            }
            case WEAPON_FLOWERS:
                sfxId2 = -1;
                break;
            default:
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
                volOffset1 = -15.0f;
                break;
            }
            break;
        }
        case AE_PED_HIT_LOW: {
            switch (weaponType) {
            case WEAPON_BRASSKNUCKLE:
                volOffset1 = 1.0f;
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
                break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxId2 = 81;
                volOffset2 = -9.0f;
                break;
            case WEAPON_BASEBALLBAT:
                volOffset1 = -10.0f;
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
                break;
            case WEAPON_CHAINSAW: {
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_FRONTEND_GAME)) {
                    return;
                }
                bankSlot1 = SND_BANK_SLOT_FRONTEND_GAME;
                sfxId = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(7, 9));
                sfxId2 = -1;
                break;
            }
            case WEAPON_FLOWERS:
                sfxId2 = -1;
                break;
            default:
                volOffset1 = -15.0f;
                sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
                break;
            }
            break;
        }
        case AE_PED_HIT_GROUND: {
            switch (weaponType) {
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxId2 = 81;
                volOffset2 = -9.0f;
                break;
            case WEAPON_BASEBALLBAT:
                volOffset1 = -10.0f;
                sfxId2 = 82;
                volOffset2 = -6.0f;
                break;
            case WEAPON_CHAINSAW: {
                if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_FRONTEND_GAME)) {
                    return;
                }
                bankSlot1 = SND_BANK_SLOT_FRONTEND_GAME;
                sfxId = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(7, 9));
                sfxId2 = -1;
                break;
            }
            case WEAPON_FLOWERS:
                sfxId2 = -1;
                break;
            default:
                volOffset1 = -15.0f;
                sfxId2 = 82;
                volOffset2 = -6.0f;
                break;
            }
            break;
        }
        case AE_PED_HIT_GROUND_KICK: {
            sfxId2 = 82;
            volOffset2 = -6.0f;
            break;
        }
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH: {
            sfxId = 58;
            sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
            break;
        }
        case AE_PED_HIT_LOW_UNARMED: {
            sfxId = 58;
            sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
            break;
        }
        case AE_PED_HIT_MARTIAL_KICK: {
            sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(59, 62));
            break;
        }
        default:
            break;
        }
    } else {
        switch (event) {
        case AE_PED_HIT_HIGH:
        case AE_PED_HIT_LOW:
        case AE_PED_HIT_GROUND: {
            volOffset2 = plugin::CallAndReturn<float, 0x4DA510, uint8>(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, static_cast<eSurfaceType>(surfaceId), m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            break;
        }
        case AE_PED_HIT_GROUND_KICK:
        case AE_PED_HIT_MARTIAL_KICK: {
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, static_cast<eSurfaceType>(surfaceId), m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            return;
        }
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_LOW_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH: {
            sfxId = 58;
            volOffset2 = plugin::CallAndReturn<float, 0x4DA510, uint8>(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, static_cast<eSurfaceType>(surfaceId), m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            break;
        }
        default:
            break;
        }
    }

    if (sfxId >= 0) {
        m_tempSound.Initialise(
            bankSlot1,
            static_cast<eSoundID>(sfxId),
            this,
            m_pPed->GetPosition(),
            finalVolume + volOffset1,
            1.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            freqVariance,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        if (maxVol > 0) {
            m_tempSound.m_Speed = 0.0f;
            m_tempSound.m_SpeedVariance = 0.0f;
            m_tempSound.m_ClientVariable = static_cast<float>(CTimer::GetTimeInMS() + maxVol);
            m_tempSound.m_Event = event;
            m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, 1);
        }
        AESoundManager.RequestNewSound(&m_tempSound);
    }

    if (sfxId2 >= 0) {
        m_tempSound.Initialise(
            SND_BANK_SLOT_WEAPON_GEN,
            static_cast<eSoundID>(sfxId2),
            this,
            m_pPed->GetPosition(),
            finalVolume + volOffset2,
            1.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            0.0588f,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        if (maxVol > 0) {
            m_tempSound.m_Speed = 0.0f;
            m_tempSound.m_SpeedVariance = 0.0f;
            m_tempSound.m_ClientVariable = static_cast<float>(CTimer::GetTimeInMS() + maxVol);
            m_tempSound.m_Event = event;
            m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, 1);
        }
        AESoundManager.RequestNewSound(&m_tempSound);
    }
}

// 0x4E2350
void CAEPedAudioEntity::HandlePedJacked(eAudioEvents event) {
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    const float volume = GetDefaultVolume(event);
    const uint32 curTime = CTimer::GetTimeInMS();

    int16 sfxId1 = -1;
    int16 sfxId2 = -1;
    uint32 delayTime1 = 0;
    uint32 delayTime2 = 0;

    switch (event) {
    case AE_PED_JACKED_CAR_PUNCH:
        sfxId1 = 58;
        sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
        delayTime1 = curTime + 500;
        delayTime2 = curTime + 2133;
        break;
    case AE_PED_JACKED_CAR_HEAD_BANG:
        sfxId1 = 50;
        sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
        delayTime1 = curTime + 933;
        delayTime2 = curTime + 2633;
        break;
    case AE_PED_JACKED_CAR_KICK:
        sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
        delayTime1 = curTime + 900;
        delayTime2 = curTime + 2800;
        break;
    case AE_PED_JACKED_BIKE:
        sfxId1 = 58;
        sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(40, 43));
        delayTime1 = curTime + 33;
        delayTime2 = curTime + 366;
        break;
    case AE_PED_JACKED_DOZER:
        sfxId2 = static_cast<int16>(CAEAudioUtility::GetRandomNumberInRange(36, 39));
        delayTime1 = curTime + 266;
        delayTime2 = curTime + 833;
        break;
    default:
        break;
    }

    if (sfxId1 >= 0) {
        m_tempSound.Initialise(
            SND_BANK_SLOT_WEAPON_GEN,
            static_cast<eSoundID>(sfxId1),
            this,
            m_pPed->GetPosition(),
            volume,
            2.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY | SOUND_REQUEST_UPDATES,
            0.0f,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        m_tempSound.m_ClientVariable = static_cast<float>(delayTime1);
        m_tempSound.m_Event = event;
        AESoundManager.RequestNewSound(&m_tempSound);
    }

    if (sfxId2 >= 0) {
        m_tempSound.Initialise(
            SND_BANK_SLOT_WEAPON_GEN,
            static_cast<eSoundID>(sfxId2),
            this,
            m_pPed->GetPosition(),
            volume,
            2.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY | SOUND_REQUEST_UPDATES,
            0.0f,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        m_tempSound.m_ClientVariable = static_cast<float>(delayTime1);
        m_tempSound.m_Event = event;
        AESoundManager.RequestNewSound(&m_tempSound);
    }

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_CAR_COLLISION, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_CAR_COLLISION, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
        return;
    }

    m_tempSound.Initialise(
        SND_BANK_SLOT_FOOTSTEPS_GENERIC,
        static_cast<eSoundID>(0),
        this,
        m_pPed->GetPosition(),
        volume,
        2.0f,
        1.0f,
        1.0f,
        0,
        SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY | SOUND_REQUEST_UPDATES,
        0.0f,
        0
    );
    m_tempSound.RegisterWithPhysicalEntity(m_pPed);
    m_tempSound.m_ClientVariable = static_cast<float>(delayTime2);
    m_tempSound.m_Event = event;
    AESoundManager.RequestNewSound(&m_tempSound);
}

// 0x4E26A0
void CAEPedAudioEntity::HandleSwimSplash(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
        return;
    }
    m_nSfxId = std::max(0, m_nSfxId + 1);
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_SWIMMING,
        .SoundID            = (eSoundID)(m_nSfxId),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = GetDefaultVolume(event),
        .Flags              = SOUND_PLAY_PHYSICALLY | SOUND_START_PERCENTAGE | SOUND_IS_DUCKABLE,
        .FrequencyVariance  = 0.0588f,
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E2790
void CAEPedAudioEntity::HandleSwimWake(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS, true)) {
        return;
    }
    if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(event, this, m_pPed)) {
        AESoundManager.PlaySound({
            .BankSlotID         = SND_BANK_SLOT_COLLISIONS,
            .SoundID            = 3,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = GetDefaultVolume(event) - 20.0f,
            .RollOffFactor      = 1.f,
            .Speed              = 0.75f,
            .Doppler            = 1.f,
            .FrameDelay         = 0,
            .Flags              = SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            .RegisterWithEntity = m_pPed,
            .EventID            = event,
        });
    }
    m_LastSwimWakeTriggerTimeMs = CTimer::GetTimeInMS();
}

// 0x4E2A90
void CAEPedAudioEntity::PlayShirtFlap(float volume, float speed) {
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.UpdateTwinLoopSound(m_pPed->GetPosition(), volume, speed);
    } else {
        m_sTwinLoopSoundEntity.Initialise(SND_BANK_SLOT_WEAPON_GEN, 19, 20, this, 200, 1000, -1, -1);
        m_sTwinLoopSoundEntity.PlayTwinLoopSound(m_pPed->GetPosition(), volume, speed, 2.0f, 1.0f, SOUND_DEFAULT);
    }
}

// 0x4E2EE0
void CAEPedAudioEntity::Service() {
    float volume = -100.0f;
    float speed = 1.0f;
    bool bCheckMovement = true;
    bool bUsingParachute = false;
    float blendSkyDive = 0.0f;
    float blendParaDecel = 0.0f;

    if (!m_pPed->IsPlayer()) {
        return;
    }

    if ((m_pPed->m_standingOnEntity && m_pPed->m_standingOnEntity->IsVehicle()) || m_pPed->bInVehicle) {
        bCheckMovement = false;
    }

    if (m_pPed->m_nPedState == PEDSTATE_DRIVING && m_pPed->m_pVehicle) {
        CVehicle* veh = m_pPed->m_pVehicle;
        if (veh->m_nVehicleSubClass == VEHICLE_SUBCLASS_BIKE ||
            (veh->m_nVehicleSubClass == VEHICLE_SUBCLASS_AUTOMOBILE && veh->IsOpenTopCar())) {
            float vehSpeed = veh->m_vecMoveSpeed.Magnitude();
            if (vehSpeed != 0.0f) {
                float baseVolume = veh->IsMissionVehicle() ? -17.0f : -10.0f;
                float clampedSpeed = std::clamp(vehSpeed, 0.0f, 1.0f);
                volume = baseVolume + 20.0f * CAEAudioUtility::AudioLog10(clampedSpeed);
                speed = 0.8f + clampedSpeed * 0.4f;
            }
        }
    }

    if (m_pPed->m_pPedIntelligence) {
        bUsingParachute = m_pPed->m_pPedIntelligence->GetUsingParachute();
    }

    if (bCheckMovement) {
        float speedNorm = m_pPed->m_vecMoveSpeed.Magnitude() / 0.7f;
        float baseVolume = -16.0f;
        if (bUsingParachute) {
            baseVolume = -7.0f;
            if (m_pPed->m_pRwClump) {
                if (auto* anim = RpAnimBlendClumpGetAssociation(m_pPed->m_pRwClump, "FALL_SkyDive_accel")) {
                    blendSkyDive = std::clamp(anim->m_fBlendAmount, 0.0f, 1.0f);
                }
                if (auto* anim = RpAnimBlendClumpGetAssociation(m_pPed->m_pRwClump, "Para_decel")) {
                    blendParaDecel = std::clamp(anim->m_fBlendAmount, 0.0f, 1.0f);
                }
            }
        } else {
            if (speedNorm <= 0.5f) {
                speedNorm = 0.0f;
            } else {
                speedNorm = (speedNorm - 0.5f) / 0.5f;
            }
        }

        speedNorm = std::clamp(speedNorm, 0.0f, 1.0f);
        volume = baseVolume + 20.0f * CAEAudioUtility::AudioLog10(speedNorm);
        speed = 0.8f + speedNorm * 0.8f;
    }

    float finalSpeed = speed * (1.0f + 0.5f * blendSkyDive);
    float targetVolume = volume + 4.0f * blendSkyDive - 6.0f * blendParaDecel;

    if (targetVolume > field_158) {
        field_158 = std::min(targetVolume, field_158 + 5.0f);
    } else if (targetVolume < field_158) {
        field_158 = std::max(targetVolume, field_158 - 2.0f);
    }

    if (targetVolume < -20.0f && field_158 < -20.0f) {
        field_158 = targetVolume;
    }

    if (field_158 > -100.0f) {
        PlayShirtFlap(field_158, finalSpeed);
    } else {
        m_sTwinLoopSoundEntity.StopSoundAndForget();
    }
}
