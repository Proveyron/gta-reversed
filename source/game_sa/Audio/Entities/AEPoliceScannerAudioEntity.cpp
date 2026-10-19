#include "StdInc.h"

#include "AEPoliceScannerAudioEntity.h"

#include "AEAudioHardware.h"
#include "AEAudioUtility.h"

namespace {
constexpr int16 s_FirstSlotsSample[3] = { 0, 5, 8 }; // 0x8C8160

constexpr int16 s_CrimeSlotsSample[23] = { -1, -1, 0, 7, 5, 8, 2, 1, 1, 1, 0, 8, 10, 3, 8, 3, 10, 4, 5, 8, 9, 6, 6 }; // 0x8C8168

struct tScannerZone {
    char  name[8];
    int16 sample;
    bool  hasDirections;
};

// 0x8C8198 (names), 0x8C87A8 (samples), 0x8C8930 (hasDirections)
constexpr tScannerZone s_ScannerZones[194] = {
    { "ALDEA", 0, true },
    { "ANGPI", 1, true },
    { "ARCO", 2, true },
    { "CUNTC", 3, false },
    { "BACKO", 4, true },
    { "BATTP", 5, true },
    { "SUNNN", 6, true },
    { "SUNMA", 7, false },
    { "BYTUN", 8, true },
    { "BEACO", 9, true },
    { "BFLD", 10, true },
    { "BFC", 11, true },
    { "BINT", 12, true },
    { "BLUAC", 13, true },
    { "BLUEB", 14, true },
    { "BONE", 15, true },
    { "CALI", 16, false },
    { "CALT", 17, true },
    { "CALT", 18, true },
    { "CITYS", 19, false },
    { "LOT", 20, false },
    { "COM", 21, true },
    { "CONF", 22, false },
    { "CRANB", 23, false },
    { "DILLI", 24, true },
    { "DOH", 25, true },
    { "SFDWT", 26, true },
    { "LDT", 27, true },
    { "ELS", 28, false },
    { "EBE", 29, false },
    { "EASB", 30, true },
    { "SFAIR", 31, false },
    { "EBAY", 32, false },
    { "ETUNN", 33, false },
    { "ELCA", 34, true },
    { "ELCO", 35, true },
    { "ELQUE", 36, true },
    { "ESPE", 37, false },
    { "ESPN", 38, false },
    { "HAUL", 39, true },
    { "FALLO", 40, false },
    { "FERN", 41, true },
    { "FINA", 42, true },
    { "FISH", 43, true },
    { "FLINTC", 44, true },
    { "FLINTI", 45, true },
    { "FLINTR", 46, true },
    { "FLINW", 47, true },
    { "CARSO", 48, false },
    { "SILLY", 49, true },
    { "FRED", 50, false },
    { "GAN", 51, true },
    { "GANTB", 52, false },
    { "GARC", 53, true },
    { "GARV", 54, false },
    { "GLN", 55, true },
    { "GGC", 56, false },
    { "PALMS", 57, true },
    { "HBARNS", 58, true },
    { "HANKY", 59, true },
    { "HGP", 60, true },
    { "HASH", 61, true },
    { "TOPFA", 62, false },
    { "QUARY", 63, false },
    { "IWD", 64, true },
    { "JTE", 65, false },
    { "JTN", 66, false },
    { "JTS", 67, false },
    { "JTW", 68, false },
    { "JUNIHI", 69, true },
    { "JUNIHO", 70, true },
    { "KACC", 71, false },
    { "KINC", 72, false },
    { "THEA", 73, true },
    { "BARRA", 74, true },
    { "BRUJA", 75, true },
    { "PAYAS", 76, true },
    { "LDM", 77, false },
    { "LEAFY", 79, true },
    { "PROBE", 80, false },
    { "LDS", 81, true },
    { "LINDEN", 82, false },
    { "LMEX", 83, true },
    { "CHC", 84, true },
    { "LFL", 85, true },
    { "LSINL", 87, false },
    { "LAIR", 88, false },
    { "???", 89, true },
    { "VAIR", 90, false },
    { "LVA", 91, false },
    { "MAR", 92, false },
    { "MKT", 93, false },
    { "MARKST", 94, false },
    { "MART", 95, false },
    { "HILLP", 96, true },
    { "MONT", 97, true },
    { "MONINT", 98, true },
    { "MTCHI", 99, true },
    { "MUL", 100, true },
    { "MULINT", 101, true },
    { "NROCK", 102, true },
    { "LDOC", 103, true },
    { "OCEAF", 104, false },
    { "OCTAN", 105, false },
    { "OVS", 106, true },
    { "OPEN", 107, false },
    { "BAYV", 108, true },
    { "PALO", 109, true },
    { "PARA", 110, true },
    { "PILG", 111, true },
    { "PINT", 112, true },
    { "PLS", 113, true },
    { "PRP", 114, true },
    { "????", 115, true },
    { "RIE", 116, false },
    { "RED", 117, true },
    { "REDE", 118, false },
    { "REDW", 119, false },
    { "TOM", 120, false },
    { "RIH", 121, true },
    { "ROCE", 122, true },
    { "RSE", 123, false },
    { "RSW", 124, false },
    { "ROD", 125, true },
    { "ROY", 126, false },
    { "SASO", 127, false },
    { "SANB", 129, true },
    { "CIVI", 130, true },
    { "SMB", 131, true },
    { "CREEK", 132, true },
    { "SHERR", 133, false },
    { "SRY", 134, false },
    { "SPIN", 135, true },
    { "STAR", 136, false },
    { "SUN", 137, true },
    { "SUN", 138, true },
    { "BIGE", 139, false },
    { "CAM", 140, false },
    { "RING", 141, false },
    { "ISLE", 142, false },
    { "FARM", 143, false },
    { "DRAG", 144, false },
    { "HIGH", 145, false },
    { "MAKO", 146, false },
    { "PANOP", 147, false },
    { "PINK", 148, false },
    { "PIRA", 149, false },
    { "DAM", 150, false },
    { "VISA", 151, false },
    { "ROBAD", 152, true },
    { "UNITY", 153, false },
    { "VALLE", 154, true },
    { "BLUF", 155, false },
    { "MEAD", 156, true },
    { "VERO", 157, true },
    { "VIN", 158, true },
    { "WHET", 159, true },
    { "WWE", 160, true },
    { "LIND", 161, true },
    { "YBELL", 162, false },
    { "YELLOW", 163, false },
    { "VE", 78, true },
    { "LA", 86, true },
    { "SF", 128, true },
    { "SAN_AND", -1, true },
    { "JEF", -1, true },
    { "PER1", 86, false },
    { "YBELL1", 162, false },
    { "YBELL2", 162, false },
    { "STRIP", 78, false },
    { "WESTP", -1, true },
    { "CUNTC1", 3, false },
    { "CUNTC2", 3, false },
    { "CUNTC3", 3, false },
    { "KINC1", 72, false },
    { "KINC2", 72, false },
    { "GARV1", 54, false },
    { "GARV2", 54, false },
    { "MONT1", 97, false },
    { "MTCHI1", 99, false },
    { "MTCHI2", 99, false },
    { "MTCHI3", 99, false },
    { "MTCHI4", 99, false },
    { "SHACA", -1, true },
    { "REST", -1, true },
    { "SANB2", 129, false },
    { "SFBAG1", 31, false },
    { "SFBAG2", 31, false },
    { "SFBAG3", 31, false },
    { "LVBAG", 90, false },
    { "LBAG1", 88, false },
    { "LBAG2", 88, false },
    { "LBAG3", 88, false },
    { "CONST1", 78, false },
};

struct tScannerVehType {
    int16 sample;
    bool  hasColor;
};

// 0x8C89F8 (samples), 0x8C8A54 (hasColor)
constexpr tScannerVehType s_ScannerVehTypes[46] = {
    { 34, true },
    { 0, true },
    { 47, true },
    { 44, true },
    { 49, true },
    { -1, true },
    { 56, true },
    { 17, false },
    { 20, false },
    { 30, true },
    { 31, true },
    { 57, true },
    { 2, true },
    { 25, true },
    { 52, false },
    { 36, true },
    { 27, true },
    { 7, true },
    { 39, false },
    { 6, true },
    { 11, true },
    { 51, false },
    { 14, true },
    { 24, false },
    { 56, true },
    { 32, true },
    { 55, false },
    { 22, true },
    { 37, true },
    { 33, true },
    { 40, true },
    { 15, true },
    { 8, false },
    { 18, true },
    { 53, true },
    { 12, true },
    { 23, true },
    { 29, true },
    { 38, false },
    { 54, false },
    { 26, true },
    { 5, true },
    { 43, true },
    { 41, false },
    { 10, true },
    { 1, true },
};

// 0x8C8A88
constexpr int16 s_VehicleColorSample[128] = {
     0, 14,  1, 12,  1,  6, -1,  1,  1, 10,  1,  9,  1,  9, 10,  9,
     8, 12, 12, 10,  1, 12, 12,  9,  9,  6,  9,  9,  1,  9,  6, 12,
     1,  9, 10,  9,  6,  8, 10,  1,  6,  6, 12, 12, 12, 12, 10,  8,
    10,  9,  9,  8,  8,  1,  1,  6,  1, 10, 12,  1, 10, 10, 12,  9,
     9, 10,  6,  1, 10, 10, 12,  1,  9, 10, 12,  6,  9, 10, 12,  1,
    12, 10, 12,  8,  6, 12, 10,  1, 12, 10, 13,  1,  9, 10,  1,  1,
    13,  1,  1, 10,  1,  6, 10,  1, 10,  9,  1, 10, 10,  9,  6,  9,
     1,  6,  8, 12,  1, 12, 10,  6, 10, 12,  9,  6, 12,  1, 11,  0,
};
} // namespace

// 0x4E6E00
CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity() {
    if (s_pPSControlling == this && s_nScannerPlaybackState != STATE_INITIAL) {
        s_bStoppingScanner = true;
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x5B9C30
void CAEPoliceScannerAudioEntity::StaticInitialise() {
    s_NextNewScannerDialogueTime = 0;
    s_bScannerDisabled           = false;
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_fVolumeOffset              = 0.0f;
}

// 0x4E6E90
void CAEPoliceScannerAudioEntity::Reset() {
    StopScanner(true);
    FinishedPlayingScannerDialogue();
}

// 0x4E71E0
void CAEPoliceScannerAudioEntity::AddAudioEvent(eAudioEvents event, eCrimeType crimeType, const CVector& point) {
    if (event != AE_CRIME_COMMITTED) {
        return;
    }

    if (crimeType <= 1 || (int32)crimeType >= 23) {
        return;
    }

    tScannerSlot first[NUM_POLICE_SCANNER_SLOTS];
    tScannerSlot second[NUM_POLICE_SCANNER_SLOTS];

    first[0].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
    first[0].SoundID = s_FirstSlotsSample[CAEAudioUtility::GetRandomNumberInRange(0, 3)];

    first[1].Bank = SND_BANK_SCRIPT_SCANNER_NUMBERS;
    first[1].SoundID = s_CrimeSlotsSample[(int32)crimeType];

    CZone* zone = CTheZones::FindSmallestZoneForPosition(&point, true);
    if (!zone) {
        return;
    }

    int32 zoneIndex = -1;
    for (int32 i = 0; i < 194; ++i) {
        if (strncmp(zone->m_TextLabel, s_ScannerZones[i].name, 8) == 0 && s_ScannerZones[i].sample >= 0) {
            zoneIndex = i;
            break;
        }
    }

    if (zoneIndex == -1) {
        return;
    }

    if (s_ScannerZones[zoneIndex].hasDirections) {
        const float midX = (float)(zone->m_fX1 + zone->m_fX2) * 0.5f;
        const float midY = (float)(zone->m_fY1 + zone->m_fY2) * 0.5f;
        const float quarterWidth = (float)(zone->m_fX2 - zone->m_fX1) * 0.25f;
        const float quarterHeight = (float)(zone->m_fY2 - zone->m_fY1) * 0.25f;

        bool hasDirection = false;
        if (point.y > midY + quarterHeight) {
            first[2].SoundID = 2;
            first[2].Bank = SND_BANK_SCRIPT_SCANNER_DIRECTIONS;
            hasDirection = true;
        } else if (point.y < midY - quarterHeight) {
            first[2].SoundID = 3;
            first[2].Bank = SND_BANK_SCRIPT_SCANNER_DIRECTIONS;
            hasDirection = true;
        }

        if (point.x > midX + quarterWidth) {
            first[3].SoundID = 1;
            first[3].Bank = SND_BANK_SCRIPT_SCANNER_DIRECTIONS;
        } else if (point.x < midX - quarterWidth) {
            first[3].SoundID = 4;
            first[3].Bank = SND_BANK_SCRIPT_SCANNER_DIRECTIONS;
        } else if (!hasDirection) {
            first[3].SoundID = 0;
            first[3].Bank = SND_BANK_SCRIPT_SCANNER_DIRECTIONS;
        }
    }

    first[4].Bank = SND_BANK_SCRIPT_SCANNER_AREAS;
    first[4].SoundID = s_ScannerZones[zoneIndex].sample;

    CPlayerPed* player = nullptr;
    for (int32 i = 0; i < 2; ++i) {
        CWanted* wanted = FindPlayerWanted(i);
        if (wanted && this == &wanted->m_PoliceScannerAudioEntity) {
            player = FindPlayerPed(i);
        }
    }

    if (player) {
        if (player->bInVehicle) {
            CVehicle* vehicle = player->m_pVehicle;
            if (vehicle) {
                const auto audioType = vehicle->m_vehicleAudio.m_AuSettings.VehicleAudioTypeForName;
                if (audioType >= 0 && audioType < 46 && s_ScannerVehTypes[audioType].sample >= 0) {
                    second[0].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
                    second[0].SoundID = 7;

                    second[1].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
                    if (audioType == AE_VAT_MOPED || audioType == AE_VAT_BIKE || audioType == AE_VAT_QUADBIKE ||
                        audioType == AE_VAT_MOWER || audioType == AE_VAT_BICYCLE || audioType == AE_VAT_TRACTOR) {
                        second[1].SoundID = 3;
                    } else {
                        second[1].SoundID = 1;
                    }

                    if (s_ScannerVehTypes[audioType].hasColor) {
                        if (vehicle->GetRemapIndex() != -1) {
                            second[3].SoundID = 4;
                            second[3].Bank = SND_BANK_SCRIPT_SCANNER_COLOURS;
                        } else {
                            const uint8 primaryColor = vehicle->m_nPrimaryColor;
                            if (primaryColor > 0 && primaryColor < 127) {
                                const int16 colorSample = s_VehicleColorSample[primaryColor];
                                if (colorSample >= 0) {
                                    second[3].SoundID = colorSample;
                                    second[3].Bank = SND_BANK_SCRIPT_SCANNER_COLOURS;
                                }
                            }
                        }
                    }

                    second[4].Bank = SND_BANK_SCRIPT_SCANNER_VEHICLES;
                    second[4].SoundID = s_ScannerVehTypes[audioType].sample;
                }
            }
        } else {
            if (player->GetIntelligence()->GetTaskSwim()) {
                second[4].SoundID = 2;
                second[0].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
                second[0].SoundID = 7;
                second[4].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
            } else if (!player->GetIntelligence()->GetTaskJetPack()) {
                second[4].SoundID = 4;
                second[0].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
                second[0].SoundID = 7;
                second[4].Bank = SND_BANK_SCRIPT_SCANNER_INSTRUCTIONS;
            }
        }
    }

    PlayPoliceScannerDialogue(first, second);
}

// 0x4E6BC0
void CAEPoliceScannerAudioEntity::PrepSlots() {
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        s_SlotState[i] = s_pCurrentSlots[i].IsActive();
    }
}

// 0x4E6CD0
void CAEPoliceScannerAudioEntity::LoadSlots() {
    if (!s_pCurrentSlots) {
        return;
    }

    bool canPlay = true;
    for (auto i = 0; i < NUM_POLICE_SCANNER_SLOTS; i++) {
        const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + i);
        auto& currentSlot = s_pCurrentSlots[i];

        if (s_SlotState[i]) {
            if (s_SlotState[i] == 2) {
                bool loaded = AEAudioHardware.IsSoundLoaded(currentSlot.Bank, currentSlot.SoundID, slot);
                if (loaded) {
                    s_SlotState[i] = 3;
                } else {
                    canPlay = false;
                }
            }
        } else if (currentSlot.IsActive()) {
            s_SlotState[i] = 1;
        } else {
            if (!CStreaming::IsVeryBusy()) {
                AEAudioHardware.LoadSound(currentSlot.Bank, currentSlot.SoundID, slot);
                s_SlotState[i] = 2;
            }
            canPlay = false;
        }
    }
    if (canPlay) {
        s_nScannerPlaybackState = FOUR;
    }
}

// 0x4E6DB0
void CAEPoliceScannerAudioEntity::EnableScanner() {
    s_bScannerDisabled = false;
}

// 0x4E71B0
void CAEPoliceScannerAudioEntity::DisableScanner(bool a1, bool bStopSound) {
    s_bScannerDisabled = true;
    if (a1 && s_nScannerPlaybackState != STATE_INITIAL) {
        if (s_pPSControlling) {
            StopScanner(bStopSound);
        }
    }
}

// 0x4E6DC0
void CAEPoliceScannerAudioEntity::StopScanner(bool bStopSound) {
    if (s_nScannerPlaybackState == STATE_INITIAL)
        return;

    s_bStoppingScanner = true;
    if (bStopSound) {
        if (s_pSound) {
            s_pSound->StopSoundAndForget();
            s_pSound = nullptr;
        }
        FinishedPlayingScannerDialogue();
    }
}

// 0x4E6C30
void CAEPoliceScannerAudioEntity::FinishedPlayingScannerDialogue() {
    AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP);
    s_nScannerPlaybackState      = STATE_INITIAL;
    s_pPSControlling             = nullptr;
    s_pCurrentSlots              = nullptr;
    s_bStoppingScanner           = false;
    s_NextNewScannerDialogueTime = s_NextNewScannerDialogueTime + CTimer::GetTimeInMS();
    s_fVolumeOffset              = 0.0f;

    std::ranges::fill(s_SlotState, -1);
    s_SlotState[4] = 1;

    rng::fill(s_ScannerSlotFirst, tScannerSlot{});
    rng::fill(s_ScannerSlotSecond, tScannerSlot{});
}

// 0x4E6F60
void CAEPoliceScannerAudioEntity::PlayLoadedDialogue() {
    int32 startSlot = 0;
    for (int32 i = 0; i < NUM_POLICE_SCANNER_SLOTS; ++i) {
        if (s_SlotState[i] == FIVE) {
            startSlot = i;
            break;
        }
    }

    int32 playSlot = -1;
    for (int32 i = startSlot; i < NUM_POLICE_SCANNER_SLOTS; ++i) {
        if (s_SlotState[i] == 3) {
            playSlot = i;
            break;
        }
    }

    if (playSlot != -1) {
        const auto volume = GetDefaultVolume(AE_CRIME_COMMITTED) + s_fVolumeOffset;
        CAESound sound;
        sound.Initialise((eSoundBankSlot)(SND_BANK_SLOT_SCANNER_FIRST + playSlot), s_pCurrentSlots[playSlot].SoundID, this, { 0.0f, 1.0f, 0.0f }, volume, 1.0f, 1.0f, 1.0f, 0, SOUND_DEFAULT, 0.0f, 0);
        sound.m_ClientVariable = (float)playSlot;
        sound.m_Flags = SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_IS_DUCKABLE;
        sound.m_Event = AE_CRIME_COMMITTED;

        s_pSound = AESoundManager.RequestNewSound(&sound);
        if (s_pSound) {
            s_SlotState[playSlot] = 5;
            s_nScannerPlaybackState = SEVEN;
        }
    } else {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK, s_fVolumeOffset, 1.0f);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_STOP, 0.0f, 1.0f);
        if (s_nSectionPlaying) {
            FinishedPlayingScannerDialogue();
        } else {
            s_pCurrentSlots = s_ScannerSlotSecond;
            s_nSectionPlaying = 1;
            for (int32 k = 0; k < NUM_POLICE_SCANNER_SLOTS; ++k) {
                s_SlotState[k] = s_ScannerSlotSecond[k].IsActive() ? 1 : 0;
            }
            s_nPlaybackStartTime = 0;
            s_nAbortPlaybackTime = 5000 + CTimer::GetTimeInMS();
            s_nScannerPlaybackState = TWO;
        }
    }
}

// 0x4E6B60
void CAEPoliceScannerAudioEntity::PopulateScannerDialogueLists(const tScannerSlot* first, const tScannerSlot* second) {
    assert(first && second);
    if (s_nScannerPlaybackState == STATE_INITIAL) {
        for (auto slotIndex = 0; slotIndex < NUM_POLICE_SCANNER_SLOTS; slotIndex++) {
            s_ScannerSlotFirst[slotIndex] = first[slotIndex];
            s_ScannerSlotSecond[slotIndex] = second[slotIndex];
        }
    }
}

// inlined
// 0x4E6C00
bool CAEPoliceScannerAudioEntity::CanWePlayNewScannerDialogue() {
    if (s_nScannerPlaybackState == STATE_INITIAL)
        return false;

    if (CTimer::GetTimeInMS() < s_NextNewScannerDialogueTime)
        return false;

    if (TheCamera.m_bWideScreenOn)
        return false;

    if (s_bScannerDisabled)
        return false;

    return true;
}

// 0x4E6ED0
void CAEPoliceScannerAudioEntity::PlayPoliceScannerDialogue(tScannerSlot* first, tScannerSlot* second) {
    assert(first && second);
    if (CanWePlayNewScannerDialogue()) { // todo: maybe little bit wrong
        PopulateScannerDialogueLists(first, second);
        s_pPSControlling = this;
        s_nSectionPlaying = 0;
        s_pCurrentSlots = s_ScannerSlotFirst;
        PrepSlots();
        s_nScannerPlaybackState = TWO;
        s_nPlaybackStartTime = CTimer::GetTimeInMS() + 2000; // todo: gSpeechContextLookup[365][6]
        s_nAbortPlaybackTime = CTimer::GetTimeInMS() + 5000; // todo: gSpeechContextLookup[366][0]
    }
}

// 0x4E7590
void CAEPoliceScannerAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (curPlayPos == -1) {
        s_pSound = nullptr;
        if (s_bStoppingScanner) {
            if (s_nScannerPlaybackState != STATE_INITIAL) {
                s_bStoppingScanner = true; // V1048 [CWE-1164] The 's_bStoppingScanner' variable was assigned the same value
                FinishedPlayingScannerDialogue();
            }
            return;
        }
        PlayLoadedDialogue();
        return;
    }

    if (s_bStoppingScanner) {
        sound->m_Volume = sound->m_Volume - 6.0f; // todo: *(float*)&gSpeechContextLookup[366][4]
        return;
    }

    if (sound->m_Length > 0 && curPlayPos > sound->m_Length - 40 && sound->m_BankSlot != 37) { // todo: -40 should be replaced with by gSpeechContextLookup[366][2]
        sound->SetFlags(eSoundEnvironment::SOUND_REQUEST_UPDATES, false);
        s_pSound = nullptr;
        PlayLoadedDialogue();
        return;
    }
}

// 0x4E7630
void CAEPoliceScannerAudioEntity::Service() {
    static constexpr uint32 startDelay = 300;   // 0x8C8154
    static constexpr float noiseVolume = -6.0f; // 0x8C8158
    static constexpr float clickVolume = +0.0f; // 0xB61D54

    bool finishPlaying;
    if (TheCamera.m_bWideScreenOn && s_nScannerPlaybackState != STATE_INITIAL) {
        finishPlaying = true;
        s_bStoppingScanner = true;
    } else {
        finishPlaying = s_bStoppingScanner;
    }

    switch (s_nScannerPlaybackState) {
    case TWO:
        if (CTimer::GetTimeInMS() > s_nAbortPlaybackTime || finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        LoadSlots();
        break;
    case FOUR:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime) {
            s_fVolumeOffset = CAEVehicleAudioEntity::s_pVehicleAudioSettingsForRadio ? 0.0f : -8.0f; // todo: -8 is gSpeechContextLookup[367][2]
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_CLICK,       s_fVolumeOffset + clickVolume, 1.0f);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SCANNER_NOISE_START, s_fVolumeOffset + noiseVolume, 1.0f); // todo: noiseVolume is gSpeechContextLookup[367][0]
            s_nPlaybackStartTime = startDelay + CTimer::GetTimeInMS(); // todo: startDelay is gSpeechContextLookup[366][6]
            s_nScannerPlaybackState = FIVE;
        }
        break;
    case FIVE:
        if (finishPlaying) {
            FinishedPlayingScannerDialogue();
            break;
        }

        if (CTimer::GetTimeInMS() >= s_nPlaybackStartTime && s_pPSControlling) {
            s_nScannerPlaybackState = SEVEN;
            s_pPSControlling->PlayLoadedDialogue();
        }
        break;
    default:
        return;
    }
}

void CAEPoliceScannerAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEPoliceScannerAudioEntity, 0x85F368, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x56DA00);
    RH_ScopedInstall(Destructor, 0x4E6E00);
    RH_ScopedInstall(StaticInitialise, 0x5B9C30);
    RH_ScopedInstall(Reset, 0x4E6E90);
    RH_ScopedInstall(AddAudioEvent, 0x4E71E0);
    RH_ScopedInstall(PrepSlots, 0x4E6BC0);
    RH_ScopedInstall(LoadSlots, 0x4E6CD0);
    RH_ScopedInstall(EnableScanner, 0x4E6DB0);
    RH_ScopedInstall(DisableScanner, 0x4E71B0);
    RH_ScopedInstall(StopScanner, 0x4E6DC0);
    RH_ScopedInstall(FinishedPlayingScannerDialogue, 0x4E6C30);
    RH_ScopedInstall(PlayLoadedDialogue, 0x4E6F60);
    RH_ScopedInstall(PopulateScannerDialogueLists, 0x4E6B60);
    RH_ScopedInstall(CanWePlayNewScannerDialogue, 0x4E6C00);
    RH_ScopedInstall(PlayPoliceScannerDialogue, 0x4E6ED0);
    RH_ScopedVMTInstall(UpdateParameters, 0x4E7590);
    RH_ScopedInstall(Service, 0x4E7630);
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Constructor() {
    this->CAEPoliceScannerAudioEntity::CAEPoliceScannerAudioEntity();
    return this;
}

CAEPoliceScannerAudioEntity* CAEPoliceScannerAudioEntity::Destructor() {
    this->CAEPoliceScannerAudioEntity::~CAEPoliceScannerAudioEntity();
    return this;
}
