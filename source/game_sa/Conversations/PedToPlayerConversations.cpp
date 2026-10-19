#include "StdInc.h"

#include "PedToPlayerConversations.h"
#include "Conversations.h"
#include "Ragdoll/IKChainManager.h"
#include "Tasks/TaskComplexSequence.h"
#include "Tasks/TaskTypes/TaskSimpleStandStill.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFoot.h"
#include "Events/EventScriptCommand.h"
#include "Stats.h"
#include "Weather.h"
#include "Clock.h"
#include "Pad.h"
#include "VehicleModelInfo.h"

void CPedToPlayerConversations::InjectHooks() {
    RH_ScopedClass(CPedToPlayerConversations);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Clear, 0x43AAE0);
    RH_ScopedInstall(Update, 0x43B0F0);
    RH_ScopedInstall(EndConversation, 0x43AB10);
}

// 0x43AAE0
void CPedToPlayerConversations::Clear() {
    ZoneScoped;

    if (m_State != eP2pState::INACTIVE) {
        CAEPedSpeechAudioEntity::ReleasePlayerConversation();
        m_State = eP2pState::INACTIVE;
    }

    m_pPed                         = nullptr;
    m_TimeOfLastPlayerConversation = 0;
}

// 0x43B0F0
void CPedToPlayerConversations::Update() {
    ZoneScoped;

    CPed* player = FindPlayerPed(-1);
    if (!player)
        return;

    CVehicle* playerVeh = FindPlayerVehicle(-1, false);
    if (playerVeh) {
        m_pVehicle = playerVeh;
        m_pVehicle->RegisterReference(reinterpret_cast<CEntity**>(&m_pVehicle));
    }

    switch (m_State) {
    case eP2pState::INACTIVE: {
        if (CTimer::m_snTimeInMilliseconds <= m_TimeOfLastPlayerConversation + 30000)
            break;

        if (FindPlayerVehicle(-1, false))
            break;

        if (player->physicalFlags.bAttachedToEntity)
            break;

        if (CConversations::IsConversationGoingOn())
            break;

        if (!player->PedIsReadyForConversation(true))
            break;

        CPedPool* pedPool = GetPedPool();
        for (int32 scanned = 0; scanned < 8; scanned++) {
            m_LastPedScanned++;
            if (m_LastPedScanned >= static_cast<int32>(pedPool->GetSize())) {
                m_LastPedScanned = 0;
            }

            CPed* ped = pedPool->GetAt(m_LastPedScanned);
            if (!ped)
                continue;

            if (ped->m_nCreatedBy != PED_GAME)
                continue;

            if (!ped->PedIsReadyForConversation(true))
                continue;

            if (ped->bFadeOut)
                continue;

            if (!ped->IsAlive())
                continue;

            CVector pedToPlayer = player->GetPosition() - ped->GetPosition();
            CVector playerToPed = ped->GetPosition() - player->GetPosition();

            if (DotProduct(ped->GetForward(), pedToPlayer) <= 0.0f)
                continue;

            if (DotProduct(player->GetForward(), playerToPed) <= 0.0f)
                continue;

            if (DistanceBetweenPoints(ped->GetPosition(), player->GetPosition()) > 7.0f)
                continue;

            if (ped->m_nPedType == PED_TYPE_GANG1 || (ped->m_nPedType >= PED_TYPE_GANG3 && ped->m_nPedType <= PED_TYPE_GANG10)) {
                m_Topic = CGeneral::GetRandomNumberInRange(8, 11);
            } else {
                m_Topic = CGeneral::GetRandomNumberInRange(0, 8);
            }

            if (!ped->m_pedSpeechAudioEntity.WillPedChatAboutTopic(static_cast<int16>(m_Topic)))
                continue;

            bool bTopicSuccess = false;
            switch (m_Topic) {
            case 0: { // Vehicle
                if (m_pVehicle && DistanceBetweenPoints(ped->GetPosition(), m_pVehicle->GetPosition()) <= 10.0f) {
                    auto* mi = static_cast<CVehicleModelInfo*>(CModelInfo::GetModelInfo(m_pVehicle->m_nModelIndex));
                    int32 vehClass = static_cast<int32>(mi->m_nVehicleClass) - 1;
                    if (vehClass >= 0 && vehClass <= 8) {
                        static constexpr uint8 s_VehicleClassTypes[9] = { 0, 1, 1, 0, 0, 0, 2, 2, 1 };
                        uint8 classType = s_VehicleClassTypes[vehClass];
                        if (classType == 0) {
                            if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                m_bPositiveOpening = false;
                                ped->Say(static_cast<eGlobalSpeechContext>(0x30), 0, 1.0f, false, true, false);
                                bTopicSuccess = true;
                            }
                        } else if (classType == 1) {
                            if (m_pVehicle->m_fHealth > 800.0f) {
                                if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                    m_bPositiveOpening = true;
                                    ped->Say(static_cast<eGlobalSpeechContext>(0x39), 0, 1.0f, false, true, false);
                                    bTopicSuccess = true;
                                }
                            }
                        }
                    }
                }
                break;
            }
            case 1: { // Fat
                float statVal = CStats::GetStatValue(STAT_CLOTHES_RESPECT) + static_cast<float>(ped->m_nRandomSeed & 0x1FF) - 256.0f;
                int32 statInt = static_cast<int32>(statVal);
                if (statInt > 500) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = true;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x3A), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                } else if (statInt < 150) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = false;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x31), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                }
                break;
            }
            case 2: { // Muscle / Haircut
                float statVal = CStats::GetStatValue(STAT_CLOTHES_RESPECT) + static_cast<float>(ped->m_nRandomSeed & 0x1FF) - 256.0f;
                int32 statInt = static_cast<int32>(statVal);
                if (player->GetPlayerData() && player->GetPlayerData()->m_pPedClothesDesc->HasVisibleNewHairCut(0)) {
                    if (statInt > 500) {
                        if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                            m_bPositiveOpening = true;
                            ped->Say(static_cast<eGlobalSpeechContext>(0x3B), 0, 1.0f, false, true, false);
                            bTopicSuccess = true;
                        }
                    } else if (statInt < 150) {
                        if (CStats::GetStatValue(STAT_BODY_MUSCLE) < 5.0f) {
                            if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                                m_bPositiveOpening = false;
                                ped->Say(static_cast<eGlobalSpeechContext>(0x32), 0, 1.0f, false, true, false);
                                bTopicSuccess = true;
                            }
                        }
                    }
                }
                break;
            }
            case 3: { // Muscle & Fat
                int32 muscleVal = static_cast<int32>(CStats::GetStatValue(STAT_MUSCLE) + static_cast<float>(ped->m_nRandomSeed & 0xFF) - 128.0f);
                int32 fatVal = static_cast<int32>(CStats::GetStatValue(STAT_FAT) + static_cast<float>(ped->m_nRandomSeed & 0xFF) - 128.0f);
                if (muscleVal > 400 && fatVal < 250) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = true;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x3C), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                } else if ((muscleVal - fatVal) < 200 && fatVal > 500) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = false;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x33), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                }
                break;
            }
            case 4: { // Clothes Respect
                float statVal = CStats::GetStatValue(STAT_CLOTHES_RESPECT) + static_cast<float>(ped->m_nRandomSeed & 0x3FF) - 512.0f;
                int32 statInt = static_cast<int32>(statVal);
                if (statInt > 700) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = true;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x3D), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                } else if (statInt < 300) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = false;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x34), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                }
                break;
            }
            case 5: { // Sex Appeal
                float statVal = CStats::GetStatValue(STAT_SEX_APPEAL) + static_cast<float>(ped->m_nRandomSeed & 0x1FF) - 256.0f;
                int32 statInt = static_cast<int32>(statVal);
                if (statInt > 700) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = true;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x3E), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                } else if (statInt < 300) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = false;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x35), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                }
                break;
            }
            case 6: { // Tattoos
                if (player->GetPlayerData() && player->GetPlayerData()->m_pPedClothesDesc->HasVisibleTattoo()) {
                    int32 roll = CGeneral::GetRandomNumberInRange(0, 1000);
                    if (roll > 700) {
                        if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                            m_bPositiveOpening = true;
                            ped->Say(static_cast<eGlobalSpeechContext>(0x3F), 0, 1.0f, false, true, false);
                            bTopicSuccess = true;
                        }
                    } else if (roll < 300) {
                        if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                            m_bPositiveOpening = false;
                            ped->Say(static_cast<eGlobalSpeechContext>(0x36), 0, 1.0f, false, true, false);
                            bTopicSuccess = true;
                        }
                    }
                }
                break;
            }
            case 7: { // Weather
                if (CWeather::Wind < 0.1f && CClock::ms_nGameClockHours > 6 && CClock::ms_nGameClockHours < 20) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = true;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x40), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                } else if (CWeather::Wind >= 0.5f) {
                    if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                        m_bPositiveOpening = false;
                        ped->Say(static_cast<eGlobalSpeechContext>(0x37), 0, 1.0f, false, true, false);
                        bTopicSuccess = true;
                    }
                }
                break;
            }
            case 8: { // Gang 1
                if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                    m_bPositiveOpening = true;
                    ped->Say(static_cast<eGlobalSpeechContext>(0xFB), 0, 1.0f, false, true, false);
                    bTopicSuccess = true;
                }
                break;
            }
            case 9: { // Gang 2
                if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                    m_bPositiveOpening = true;
                    ped->Say(static_cast<eGlobalSpeechContext>(0xFC), 0, 1.0f, false, true, false);
                    bTopicSuccess = true;
                }
                break;
            }
            default:
                break;
            }

            if (bTopicSuccess) {
                m_State = eP2pState::PEDHASOPENED;
                m_pPed = ped;
                m_pPed->RegisterReference(reinterpret_cast<CEntity**>(&m_pPed));
                m_TimeOfLastPlayerConversation = CTimer::m_snTimeInMilliseconds;
                m_StartTime = CTimer::m_snTimeInMilliseconds;
                ped->DisablePedSpeech(false);
                g_ikChainMan.LookAt("Ped2Pl_Conversation", ped, player, 100000, BONE_HEAD, nullptr, false, 0.25f, 500, 8, false);
                g_ikChainMan.LookAt("Ped2Pl_ConversationP", player, ped, 100000, BONE_HEAD, nullptr, false, 0.25f, 500, 8, false);
                return;
            }

            if ((rand() & 0xFFF) == 3) {
                ped->Say(static_cast<eGlobalSpeechContext>(0x58), 0, 1.0f, false, false, false);
                return;
            }
        }
        break;
    }
    case eP2pState::PEDHASOPENED: {
        if (!m_pPed) {
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            return;
        }

        if (CTimer::m_snTimeInMilliseconds > m_StartTime + 4000) {
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            m_pPed->EnablePedSpeech();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            if (g_ikChainMan.IsLooking(m_pPed)) {
                g_ikChainMan.AbortLookAt(m_pPed, 250);
            }
            if (m_bPositiveOpening) {
                m_pPed->Say(static_cast<eGlobalSpeechContext>(0x38), 0, 1.0f, false, false, false);
            }
            return;
        }

        CPad* pad = CPad::GetPad(0);
        if (pad->ConversationYesJustDown()) {
            if (m_bPositiveOpening) {
                switch (m_Topic) {
                case 0: player->Say(static_cast<eGlobalSpeechContext>(0x81), 0, 1.0f, true, false, false); break;
                case 1: player->Say(static_cast<eGlobalSpeechContext>(0x82), 0, 1.0f, true, false, false); break;
                case 2: player->Say(static_cast<eGlobalSpeechContext>(0x86), 0, 1.0f, true, false, false); break;
                case 3: player->Say(static_cast<eGlobalSpeechContext>(0x89), 0, 1.0f, true, false, false); break;
                case 4: player->Say(static_cast<eGlobalSpeechContext>(0x8A), 0, 1.0f, true, false, false); break;
                case 5: player->Say(static_cast<eGlobalSpeechContext>(0x8B), 0, 1.0f, true, false, false); break;
                case 6: player->Say(static_cast<eGlobalSpeechContext>(0x8C), 0, 1.0f, true, false, false); break;
                case 7: player->Say(static_cast<eGlobalSpeechContext>(0xEA), 0, 1.0f, true, false, false); break;
                case 8:
                    player->Say(static_cast<eGlobalSpeechContext>(0xEC), 0, 1.0f, true, false, false);
                    if (CGeneral::GetRandomNumberInRange(0, 100) < 75) {
                        auto* seq = new CTaskComplexSequence();
                        seq->AddTask(new CTaskSimpleStandStill(2000, false, false, 8.0f));
                        seq->AddTask(new CTaskComplexKillPedOnFoot(player, -1, 0, 0, 0, false));
                        CEventScriptCommand event(TASK_PRIMARY_EVENT_RESPONSE_TEMP, seq, false);
                        m_pPed->GetIntelligence()->GetEventGroup().Add(&event, false);
                        EndConversation();
                        return;
                    }
                    break;
                case 9:
                    player->Say(static_cast<eGlobalSpeechContext>(0x6B), 0, 1.0f, true, false, false);
                    if (CGeneral::GetRandomNumberInRange(0, 100) < 75) {
                        auto* seq = new CTaskComplexSequence();
                        seq->AddTask(new CTaskSimpleStandStill(2000, false, false, 8.0f));
                        seq->AddTask(new CTaskComplexKillPedOnFoot(player, -1, 0, 0, 0, false));
                        CEventScriptCommand event(TASK_PRIMARY_EVENT_RESPONSE_TEMP, seq, false);
                        m_pPed->GetIntelligence()->GetEventGroup().Add(&event, false);
                        EndConversation();
                        return;
                    }
                    break;
                default:
                    break;
                }
            } else {
                bool isFemale = (m_pPed->m_nPedType == PED_TYPE_CIVFEMALE || m_pPed->m_nPedType == PED_TYPE_PROSTITUTE);
                player->Say(static_cast<eGlobalSpeechContext>(isFemale ? 0x83 : 0x84), 0, 1.0f, true, false, false);
            }
            m_State = eP2pState::WAITINGFORFINALWORD;
            m_StartTime = CTimer::m_snTimeInMilliseconds;
        } else if (pad->ConversationNoJustDown()) {
            if (m_bPositiveOpening) {
                if (m_Topic == 8) {
                    player->Say(static_cast<eGlobalSpeechContext>(0xEB), 0, 1.0f, true, false, false);
                    EndConversation();
                    return;
                }
                if (m_Topic == 9) {
                    player->Say(static_cast<eGlobalSpeechContext>(0x6A), 0, 1.0f, true, false, false);
                    EndConversation();
                    return;
                }
                bool isFemale = (m_pPed->m_nPedType == PED_TYPE_CIVFEMALE || m_pPed->m_nPedType == PED_TYPE_PROSTITUTE);
                player->Say(static_cast<eGlobalSpeechContext>(isFemale ? 0x83 : 0x84), 0, 1.0f, true, false, false);
                m_State = eP2pState::WAITINGFORFINALWORD;
                m_StartTime = CTimer::m_snTimeInMilliseconds;
            } else {
                bool isFemale = (m_pPed->m_nPedType == PED_TYPE_CIVFEMALE || m_pPed->m_nPedType == PED_TYPE_PROSTITUTE);
                player->Say(static_cast<eGlobalSpeechContext>(isFemale ? 0x87 : 0x88), 0, 1.0f, true, false, false);
                if (m_pPed->m_nPedType != PED_TYPE_GANG1) {
                    if (CGeneral::GetRandomNumberInRange(0, 100) < 40) {
                        auto* seq = new CTaskComplexSequence();
                        seq->AddTask(new CTaskSimpleStandStill(2000, false, false, 8.0f));
                        seq->AddTask(new CTaskComplexKillPedOnFoot(player, -1, 0, 0, 0, false));
                        CEventScriptCommand event(TASK_PRIMARY_EVENT_RESPONSE_TEMP, seq, false);
                        m_pPed->GetIntelligence()->GetEventGroup().Add(&event, false);
                    }
                }
                m_State = eP2pState::WAITINGTOFINISH;
                m_StartTime = CTimer::m_snTimeInMilliseconds;
            }
        }
        break;
    }
    case eP2pState::WAITINGFORFINALWORD: {
        if (!m_pPed) {
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            return;
        }

        if (CTimer::m_snTimeInMilliseconds > m_StartTime + 3000 && !m_pPed->IsPedSayingSomething()) {
            m_pPed->Say(static_cast<eGlobalSpeechContext>(0x85), 0, 1.0f, true, false, false);
            m_State = eP2pState::WAITINGTOFINISH;
            m_StartTime = CTimer::m_snTimeInMilliseconds;
        }
        break;
    }
    case eP2pState::WAITINGTOFINISH: {
        if (CTimer::m_snTimeInMilliseconds > m_StartTime + 2500 || !m_pPed) {
            EndConversation();
        }
        break;
    }
    default:
        break;
    }
}

// 0x43AB10
void CPedToPlayerConversations::EndConversation() {
    m_State           = eP2pState::INACTIVE;

    CAEPedSpeechAudioEntity::ReleasePlayerConversation();

    if (m_pPed) {
        m_pPed->EnablePedSpeech();
    }

    const auto player = FindPlayerPed(-1);
    if (g_ikChainMan.IsLooking(player)) {
        g_ikChainMan.AbortLookAt(player, 250);
    }

    if (m_pPed && g_ikChainMan.IsLooking(m_pPed)) {
        return g_ikChainMan.AbortLookAt(m_pPed, 250);
    }
}
