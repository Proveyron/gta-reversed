#include "StdInc.h"

#include <reversiblebugfixes/Bugs.hpp>
#include "PedDamageResponseCalculator.h"
#include "TaskSimpleFight.h"

void CPedDamageResponseCalculator::InjectHooks() {
    RH_ScopedClass(CPedDamageResponseCalculator);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x4AD3F0);
    RH_ScopedInstall(AccountForPedDamageStats, 0x4AD430);
    RH_ScopedInstall(AccountForPedArmour, 0x4AD550);
    RH_ScopedInstall(ComputeWillForceDeath, 0x4AD610);
    RH_ScopedInstall(ComputeWillKillPed, 0x4B3210);
    RH_ScopedInstall(IsBleedingWeapon, 0x4B5C2A);
    RH_ScopedInstall(ComputeDamageResponse, 0x4B5AC0);
}

// 0x4AD3F0
CPedDamageResponseCalculator::CPedDamageResponseCalculator(const CEntity* entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak) {
    m_pDamager      = entity;
    m_fDamageFactor = fDamage;
    m_bodyPart      = bodyPart;
    m_weaponType    = weaponType;
    m_bSpeak        = bSpeak;
}

CPedDamageResponseCalculator* CPedDamageResponseCalculator::Constructor(CEntity* entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak) {
    this->CPedDamageResponseCalculator::CPedDamageResponseCalculator(entity, fDamage, weaponType, bodyPart, bSpeak);
    return this;
}

/*!
 *
 * @param ped
 * @param response
 * @addr 0x4AD430
 */
void CPedDamageResponseCalculator::AccountForPedDamageStats(CPed* ped, CPedDamageResponse& response) {
    const bool bDamageFactorMax = (m_fDamageFactor == ms_damageFactor);

    if (ped->IsPlayer()) {
        m_fDamageFactor *= 0.33f;
    } else {
        m_fDamageFactor *= ped->m_pStats->m_fDefenseMultiplier;
    }

    auto applyReduction = [this]() {
        m_fDamageFactor = std::min(m_fDamageFactor * 0.1f, 12.5f);
    };

    if (m_pDamager && m_pDamager->IsPed()) {
        auto* damagerPed = m_pDamager->AsPed();
        if (damagerPed->IsPlayer()) {
            if (CPedGroups::AreInSameGroup(ped, damagerPed)) {
                if (m_weaponType != WEAPON_EXPLOSION && m_weaponType != WEAPON_FLAMETHROWER && !bDamageFactorMax) {
                    applyReduction();
                    return;
                }
            }
        }
    }

    if (ped && ped->IsPlayer()) {
        if (m_pDamager && m_pDamager->IsPed()) {
            auto* damagerPed = m_pDamager->AsPed();
            if (CPedIntelligence::AreFriends(ped, damagerPed)) {
                applyReduction();
                return;
            }
        }
    }

    if (m_pDamager && m_pDamager->IsPed()) {
        auto* damagerPed = m_pDamager->AsPed();
        if (ped->m_nCreatedBy == PED_MISSION) {
            if (CPedGroups::AreInSameGroup(ped, damagerPed)) {
                if (m_weaponType != WEAPON_EXPLOSION && m_weaponType != WEAPON_FLAMETHROWER) {
                    applyReduction();
                    return;
                }
            }
        }
    }
}

/*!
 *
 * @param ped
 * @param response
 * @addr 0x4AD550
 */
void CPedDamageResponseCalculator::AccountForPedArmour(CPed* ped, CPedDamageResponse& response) {
    if (ped->m_fArmour <= 0.0f)
        return;

    if (m_weaponType == WEAPON_DROWNING || m_weaponType == WEAPON_FALL)
        return;

    if (FindPlayerPed() == ped)
        CWorld::Players[CWorld::PlayerInFocus].m_nLastTimeArmourLost = CTimer::GetTimeInMS();

    if (m_fDamageFactor > ped->m_fArmour) {
        m_fDamageFactor = m_fDamageFactor - ped->m_fArmour;
        response.m_fDamageArmor = ped->m_fArmour;
        ped->m_fArmour = 0.0f;
    } else {
        response.m_fDamageArmor = m_fDamageFactor;
        ped->m_fArmour = ped->m_fArmour - m_fDamageFactor;
        m_fDamageFactor = 0.0f;
    }
}

/*!
 *
 * @param ped
 * @addr Added in Android
 */
void CPedDamageResponseCalculator::AdjustPedDamage(CPed* ped) {
    // TODO: Reverted function from GTA SA 2.10 version ABI : arm64-v8a (x64)
    // offset: 0x444370

    if (!ped) {
        return;
    }

    const auto plyr = FindPlayerPed(-1);
    if (!plyr) {
        return;
    }

    const bool isUnarmedBrassOrParachute = m_weaponType == WEAPON_UNARMED
        || m_weaponType == WEAPON_BRASSKNUCKLE
        || m_weaponType == WEAPON_PARACHUTE;

    if (CCheat::m_aCheatsActive[CHEAT_COUNTRY_TRAFFIC] && m_weaponType <= eWeaponType::WEAPON_PARACHUTE && isUnarmedBrassOrParachute) {
        m_fDamageFactor = ped->m_fHealth;
    }

    if (plyr == ped && CTheScripts::pActiveScripts && !strcmp(CTheScripts::pActiveScripts->m_szName, "intro1")) {
        m_fDamageFactor = m_fDamageFactor * 0.77f;
    }
}

/*!
 *
 * @param ped
 * @param response
 * @return
 * @addr 0x4AD610
 */
bool CPedDamageResponseCalculator::ComputeWillForceDeath(CPed* ped, CPedDamageResponse& response) {
    if (ped->bImmuneToHeadshots) {
        return false;
    }

    switch (m_weaponType) {
    case WEAPON_KATANA: {
        if (!m_pDamager || !m_pDamager->IsPed()) {
            return false;
        }
        auto* damagerPed = m_pDamager->AsPed();
        if (!damagerPed->IsPlayer()) {
            return false;
        }
        auto* fightTask = damagerPed->GetIntelligence()->GetTaskFighting();
        if (!fightTask || fightTask->m_nComboSet != 11) {
            return false;
        }
        if (fightTask->m_nCurrentMove == FIGHT_ATTACK_HIT_3) {
            return (CGeneral::GetRandomNumber() & 1) != 0;
        }
        return (CGeneral::GetRandomNumber() & 7) == 0;
    }
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
    case WEAPON_SNIPERRIFLE:
    case WEAPON_MINIGUN:
    case static_cast<eWeaponType>(52): {
        int32 chance;
        if (!ped->IsPlayer() && !ped->bImmuneToHeadshots) {
            if (m_weaponType == WEAPON_SNIPERRIFLE || m_weaponType == WEAPON_COUNTRYRIFLE) {
                chance = 0;
            } else {
                chance = CGeneral::GetRandomNumber() & 7;
            }
        } else {
            chance = 1;
        }

        if (ped->bIsPlayerFriend && ped->m_nPedType != PED_TYPE_PLAYER1 && !ped->IsPlayer() && !ped->bImmuneToHeadshots) {
            return m_bodyPart == PED_PIECE_HEAD;
        }

        if (!ped->IsPlayer() && m_pDamager && m_pDamager->IsPed()) {
            auto* damagerPed = m_pDamager->AsPed();
            if (damagerPed->IsPlayer()) {
                auto* player = damagerPed->AsPlayerPed();
                if (player->m_pPlayerData && (player->m_pPlayerData->m_nTargetBone == 5 || player->m_pPlayerData->m_bFreeAiming)) {
                    return m_bodyPart == PED_PIECE_HEAD;
                }
            }
        }

        if (chance == 0) {
            return m_bodyPart == PED_PIECE_HEAD;
        }
        return false;
    }
    case WEAPON_GRENADE:
    case WEAPON_RLAUNCHER:
    case WEAPON_RLAUNCHER_HS:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_EXPLOSION: {
        if (!CLocalisation::KillPeds()) {
            return false;
        }
        if (ped->IsPlayer() || ped->bIsPlayerFriend) {
            return false;
        }
        return (m_fDamageFactor + 1.0f) >= ped->m_fHealth;
    }
    default:
        return false;
    }
}

/*!
 *
 * @param ped
 * @param response
 * @param bSpeak
 * @addr 0x4B3210
 */
void CPedDamageResponseCalculator::ComputeWillKillPed(CPed* ped, CPedDamageResponse& rsp, bool bSpeak) {
    if (ped->IsPlayer()) {
        if (CCheat::IsActive(CHEAT_NO_ONE_CAN_STOP_US) && m_weaponType < WEAPON_LAST_WEAPON) {
            return;
        }
    }

    rsp.m_bForceDeath = ComputeWillForceDeath(ped, rsp);

    if (CCheat::IsActive(CHEAT_MEGA_PUNCH)) {
        if (notsa::contains({ WEAPON_UNARMED, WEAPON_BRASSKNUCKLE, WEAPON_PARACHUTE }, m_weaponType)) {
            m_fDamageFactor = ped->m_fHealth;
        }
    }

    if (!rsp.m_bForceDeath && m_weaponType == WEAPON_FALL && m_bSpeak) {
        const auto prevHP   = ped->m_fHealth;
        ped->m_fHealth      = std::max(prevHP - m_fDamageFactor, 5.0f);
        rsp.m_bHealthZero   = false;
        rsp.m_fDamageHealth = prevHP - ped->m_fHealth;
        if (bSpeak) {
            ped->Say(CTX_GLOBAL_PAIN_LOW, 0, 1.0, 0, 0, 0);
        }
        return;
    }

    if (rsp.m_bForceDeath || ped->m_fHealth - m_fDamageFactor < 1.0f) {
        rsp.m_fDamageHealth = ped->m_fHealth;
        rsp.m_bHealthZero   = true;
        ped->m_fHealth      = 0.0f;
        return;
    }

    rsp.m_bHealthZero   = false;
    rsp.m_fDamageHealth = m_fDamageFactor;
    ped->m_fHealth      = ped->m_fHealth - m_fDamageFactor;

    if (bSpeak) {
        ped->Say(
            m_weaponType == WEAPON_DROWNING
                ? CTX_GLOBAL_PAIN_CJ_DROWNING
                : m_fDamageFactor < 5.0f && ped->m_fHealth > 10.0f
                    ? CTX_GLOBAL_PAIN_LOW
                    : CTX_GLOBAL_PAIN_HIGH
        );
    }
}

/*!
 *
 * @param ped
 * @return
 * @addr 0x4B5C2A inlined
 */
bool CPedDamageResponseCalculator::IsBleedingWeapon(CPed* ped) const {
    if (ped->IsPlayer())
        return false;

    if (m_weaponType == WEAPON_KNIFE || m_weaponType == WEAPON_KATANA || m_weaponType == WEAPON_CHAINSAW)
        return true;

    return false;
}

/*!
 *
 * @param ped
 * @param response
 * @param bSpeak
 * @addr 0x4B5AC0
 */
void CPedDamageResponseCalculator::ComputeDamageResponse(CPed* ped, CPedDamageResponse& response, bool bSpeak) {
    if (response.m_bDamageCalculated) {
        return;
    }

    response.m_fDamageHealth = 0.0f;
    response.m_fDamageArmor = 0.0f;
    response.m_bHealthZero = false;
    response.m_bForceDeath = false;
    response.m_bDamageCalculated = true;
    response.m_bCheckIfAffectsPed = true;

    AccountForPedDamageStats(ped, response);
    AccountForPedArmour(ped, response);
    ComputeWillKillPed(ped, response, bSpeak);

    if (m_pDamager) {
        auto* player = FindPlayerPed();
        auto* playerVeh = FindPlayerVehicle();
        if ((m_pDamager == player || m_pDamager == playerVeh) && m_pDamager != ped) {
            if (response.m_fDamageHealth + response.m_fDamageArmor > 0.0f) {
                CWorld::Players[CWorld::PlayerInFocus].m_nHavocCaused++;
            }
        }
    }

    if (ped == FindPlayerPed()) {
        FindPlayerPed()->AnnoyPlayerPed(false);
    }

    if (ped->IsPlayer()) {
        if (m_pDamager && m_pDamager->IsPed()) {
            CEventDamage eventDamage(
                const_cast<CEntity*>(m_pDamager),
                0,
                m_weaponType,
                m_bodyPart,
                0,
                false,
                ped->bInVehicle
            );
            ped->AsPlayerPed()->MakeGroupRespondToPlayerTakingDamage(eventDamage);
        }
    }

    if (response.m_fDamageHealth + response.m_fDamageArmor > 0.0f) {
        if (IsBleedingWeapon(ped)) {
            ped->field_72F = 200;
        }

        if (ped == FindPlayerPed()) {
            CWorld::Players[CWorld::PlayerInFocus].m_nLastTimeEnergyLost = CTimer::GetTimeInMS();
        }

        ped->m_nLastWeaponDamage = static_cast<char>(m_weaponType);
        if (m_pDamager) {
            ped->m_pLastEntityDamage = const_cast<CEntity*>(m_pDamager);
            CEntity::RegisterReference(ped->m_pLastEntityDamage);
            ped->field_768 = static_cast<int32>(CTimer::GetTimeInMS());

            if (m_pDamager->IsPed() && m_pDamager->AsPed()->IsPlayer()) {
                auto* player = FindPlayerPed();
                if (player && player->m_pPlayerData) {
                    CPedGroups::GetGroup(player->m_pPlayerData->m_nPlayerGroup).GetMembership().IsMember(ped);
                }
            }
        }
    }
}
