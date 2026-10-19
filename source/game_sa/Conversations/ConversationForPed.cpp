#include "ConversationForPed.h"
#include "Conversations.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "PedGroups.h"
#include "Timer.h"
#include "Pad.h"
#include "Messages.h"
#include "Text.h"

void CConversationForPed::InjectHooks() {
    RH_ScopedClass(CConversationForPed);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Update, 0x43C190);
    RH_ScopedInstall(IsPlayerInPositionForConversation, 0x43AC40);
}

void CConversationForPed::Clear(bool dontClearNodes) {
    if (!dontClearNodes) {
        CConversations::m_Nodes[m_FirstNode].ClearRecursively();
    }
    m_FirstNode                 = -1;
    m_CurrentNode               = -1;
    m_pPed                      = nullptr;
    m_LastChange                = 0;
    m_LastTimeWeWereCloseEnough = 0;
}

// 0x43C190
void CConversationForPed::Update() {
    if (!m_pPed || !m_Enabled || !IsPlayerInPositionForConversation(false)) {
        m_Status = INACTIVE;
        return;
    }

    const uint32 now = CTimer::GetTimeInMS();

    bool triggerPedSpeak = false;
    if (now > m_LastTimeWeWereCloseEnough + 10000) {
        triggerPedSpeak = true;
    } else if (m_Status == PLAYER_SPEAKING) {
        if (now > m_LastChange + 4000 || CPad::GetPad(0)->ConversationNoJustDown() || CPad::GetPad(0)->ConversationYesJustDown()) {
            triggerPedSpeak = true;
        }
    }

    if (triggerPedSpeak) {
        if (!m_SuppressSubtitles) {
            CMessages::ClearSmallMessagesOnly();
            CMessages::AddMessageJump(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true);
        }
        int32 speech = CConversations::m_Nodes[m_CurrentNode].m_Speech;
        if (speech > 0) {
            m_pPed->Say(static_cast<uint16>(speech), 0, 1.0f, false, false, false);
        } else if (speech < 0) {
            CConversations::AwkwardSay(-speech, m_pPed);
        }
        m_Status     = PED_SPEAKING;
        m_LastChange = now;
    } else if (now > m_LastChange + 400) {
        if (m_Status == PED_SPEAKING || m_Status == WAITINGFORINPUT || m_Status == INACTIVE) {
            if (CPad::GetPad(0)->ConversationNoJustDown()) {
                if (CConversations::m_Nodes[m_CurrentNode].m_NodeNo >= 0) {
                    if (!m_SuppressSubtitles) {
                        CMessages::ClearSmallMessagesOnly();
                        char key[16];
                        strcpy_s(key, CConversations::m_Nodes[m_CurrentNode].m_Name);
                        strcat_s(key, "N");
                        CMessages::AddMessageJump(TheText.Get(key), 4000, 3, true);
                    }
                    int32 speechN = CConversations::m_Nodes[m_CurrentNode].m_SpeechN;
                    if (speechN > 0) {
                        FindPlayerPed()->Say(static_cast<uint16>(speechN), 0, 1.0f, false, false, false);
                    } else if (speechN < 0) {
                        CConversations::AwkwardSay(-speechN, FindPlayerPed());
                    }
                    m_CurrentNode = CConversations::m_Nodes[m_CurrentNode].m_NodeNo;
                    m_LastChange  = CTimer::GetTimeInMS();
                    if (!m_SuppressSubtitles) {
                        CMessages::AddMessageQ(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true);
                    }
                    m_Status = PLAYER_SPEAKING;
                }
            } else if (CPad::GetPad(0)->ConversationYesJustDown()) {
                if (CConversations::m_Nodes[m_CurrentNode].m_NodeYes >= 0) {
                    if (!m_SuppressSubtitles) {
                        CMessages::ClearSmallMessagesOnly();
                        char key[16];
                        strcpy_s(key, CConversations::m_Nodes[m_CurrentNode].m_Name);
                        strcat_s(key, "Y");
                        CMessages::AddMessageJump(TheText.Get(key), 4000, 3, true);
                    }
                    int32 speechY = CConversations::m_Nodes[m_CurrentNode].m_SpeechY;
                    if (speechY > 0) {
                        FindPlayerPed()->Say(static_cast<uint16>(speechY), 0, 1.0f, false, false, false);
                    } else if (speechY < 0) {
                        CConversations::AwkwardSay(-speechY, FindPlayerPed());
                    }
                    m_CurrentNode = CConversations::m_Nodes[m_CurrentNode].m_NodeYes;
                    m_LastChange  = CTimer::GetTimeInMS();
                    if (!m_SuppressSubtitles) {
                        CMessages::AddMessageQ(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true);
                    }
                    m_Status = PLAYER_SPEAKING;
                }
            }
        }
    }

    m_LastTimeWeWereCloseEnough = CTimer::GetTimeInMS();
    if (m_Status == PED_SPEAKING) {
        if (CTimer::GetTimeInMS() > m_LastChange + 4000) {
            m_Status = WAITINGFORINPUT;
        }
    }
}

// 0x43AC40
bool CConversationForPed::IsPlayerInPositionForConversation(bool randomConversation) {
    CPlayerPed* player = FindPlayerPed();
    if (!player || !m_pPed) {
        return false;
    }

    if (DistanceBetweenPoints(player->GetPosition(), m_pPed->GetPosition()) > 4.0f) {
        return false;
    }

    if (DotProduct(player->GetForward(), m_pPed->GetForward()) >= 0.0f) {
        return false;
    }

    if (player->m_vecMoveSpeed.Magnitude2D() >= 0.01f) {
        return false;
    }

    if (m_pPed->m_pTalkingPed == player && CTimer::GetTimeInMS() < m_pPed->m_nTalkTimer + 6000) {
        return false;
    }

    if (randomConversation) {
        if (player->m_pPlayerData && CPedGroups::GetGroup(player->m_pPlayerData->m_nPlayerGroup).GetMembership().CountMembersExcludingLeader() >= 1) {
            return false;
        }
    }

    return player->PedIsReadyForConversation(randomConversation) && m_pPed->PedIsReadyForConversation(randomConversation);
}
