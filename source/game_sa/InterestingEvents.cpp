#include "StdInc.h"

#include "InterestingEvents.h"

auto& g_InterestingEvents = StaticRef<CInterestingEvents>(0xC0B058);

/*
 * Commented hooks aren't tested.
 * */
void CInterestingEvents::InjectHooks() {
    RH_ScopedClass(CInterestingEvents);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x6023A0);
    RH_ScopedInstall(Destructor, 0x856880);
    RH_ScopedInstall(Add, 0x602590);
    RH_ScopedInstall(ScanForNearbyEntities, 0x605A30);
    RH_ScopedInstall(GetInterestingEvent, 0x6028A0);
    RH_ScopedInstall(InvalidateEvent, 0x602960);
    RH_ScopedInstall(InvalidateNonVisibleEvents, 0x6029C0);
}

// 0x6023A0
CInterestingEvents::CInterestingEvents() {
    m_nFlags = 0;
    m_b2 = true;
    m_b4 = true;
    m_b8 = true;

    m_nInterestingEvent = -1;
    m_nLastScanTimeUpdate = 0;
    m_fRadius = 30.0f;
    m_CurrentFrameCounter = CTimer::GetFrameCounter() - 1;

    m_Events.fill(TInterestingEvent{});
    m_nPriorities.fill(5);
    m_nDelays.fill(2000);
    m_nEndsOfTime.fill(0);

    const auto SetOptions = [this](size_t index, uint8 priority, uint16 delay, uint32 end = 0) {
        m_nPriorities[index] = priority;
        m_nDelays[index]     = delay;
        m_nEndsOfTime[index] = end;
    };

    SetOptions(INTERESTING_EVENT_0,     5,  2000);
    SetOptions(PEDS_CHATTING,           1,  5000);
    SetOptions(INTERESTING_EVENT_2,     1,  5000);
    SetOptions(INTERESTING_EVENT_3,     1,  5000);
    SetOptions(INTERESTING_EVENT_4,     2,  3000);
    SetOptions(INTERESTING_EVENT_5,     2,  3000);
    SetOptions(INTERESTING_EVENT_6,     2,  3000);
    SetOptions(INTERESTING_EVENT_7,     2,  3000);
    SetOptions(INTERESTING_EVENT_8,     4,  3000);
    SetOptions(INTERESTING_EVENT_9,     4,  3000);
    SetOptions(INTERESTING_EVENT_10,    5,  6000);
    SetOptions(INTERESTING_EVENT_11,    6,  6000);
    SetOptions(INTERESTING_EVENT_12,    6,  8000);
    SetOptions(INTERESTING_EVENT_13,    6,  5000);
    SetOptions(INTERESTING_EVENT_14,    5,  6000);
    SetOptions(INTERESTING_EVENT_15,    9,  6000);
    SetOptions(INTERESTING_EVENT_16,    9,  6000);
    SetOptions(VEHICLE_DAMAGE,          8,  6000);
    SetOptions(INTERESTING_EVENT_18,    7,  6000);
    SetOptions(INTERESTING_EVENT_19,    6,  5000);
    SetOptions(INTERESTING_EVENT_20,    7,  6000);
    SetOptions(INTERESTING_EVENT_21,    8,  8000);
    SetOptions(INTERESTING_EVENT_22,    9,  5000);
    SetOptions(GANG_ATTACKING_PED,      9,  6000);
    SetOptions(GANG_FIGHT,              9,  6000);
    SetOptions(INTERESTING_EVENT_25,    9,  6000);
    SetOptions(ZELDICK_OCCUPATION,      9,  8000);
    SetOptions(EVENT_ATTRACTOR,         10, 4000);
    SetOptions(INTERESTING_EVENT_28,    10, 4000);
}

CInterestingEvents* CInterestingEvents::Constructor() {
    this->CInterestingEvents::CInterestingEvents();
    return this;
}

// 0x856880
CInterestingEvents::~CInterestingEvents() {
    for (auto& event : m_Events) {
        CEntity::ClearReference(event.entity);
    }
}

CInterestingEvents* CInterestingEvents::Destructor() {
    this->CInterestingEvents::~CInterestingEvents();
    return this;
}

// 0x602590
void CInterestingEvents::Add(CInterestingEvents::EType type, CEntity* entity) {
    if (!m_b1 || !entity) {
        return;
    }

    const auto& camPos = CCamera::GetActiveCamera().m_vecSource;
    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();

        CPlayerPed* player = FindPlayerPed();
        const auto& playerPos = player->GetPosition();
        vec148 = playerPos - camPos;
        vec148.z = 0.0f;
        if (vec148.NormaliseAndMag() == 0.0f) {
            vec148 = player->GetMatrix().GetForward();
        }
        m_vecCenter = playerPos + vec148 * m_fRadius;
    }

    CVector2D distance = m_vecCenter - entity->GetPosition();
    if (distance.SquaredMagnitude() > m_fRadius * m_fRadius) {
        return;
    }

    if (m_b2 && DotProduct(vec148, entity->GetPosition() - camPos) < 0.0f) {
        return;
    }

    if (!CWorld::GetIsLineOfSightClear(camPos, entity->GetPosition(), true, false, false, false, false, true, false)) {
        return;
    }

    const auto curTime = CTimer::GetTimeInMS();
    const auto newPriority = m_nPriorities[type];
    const bool bTimeAfterEnd = (curTime > m_nEndsOfTime[type]);

    for (int32 index = 0; index < MAX_INTERESTING_EVENTS; index++) {
        TInterestingEvent& event = m_Events[index];
        const bool bIsActiveInterestingEvent = (m_nInterestingEvent == index);
        const bool bEventExpired = (curTime > event.time + (uint32)m_nDelays[event.type]);

        if (!event.entity) {
            event.type = 0;
            event.entity = nullptr;
        } else {
            if (event.type != 0) {
                if (newPriority < m_nPriorities[event.type] && !bEventExpired) {
                    continue;
                }
                if (!bTimeAfterEnd || bIsActiveInterestingEvent) {
                    continue;
                }
            }
        }

        CEntity::ClearReference(event.entity);
        event.type = type;
        event.entity = entity;
        event.time = curTime;
        entity->RegisterReference(&event.entity);

        if (m_b8) {
            m_nEndsOfTime[type] = curTime + (m_nDelays[type] >> 1);
        } else {
            m_nEndsOfTime[type] = curTime;
        }
        break;
    }
}

// 0x605A30
void CInterestingEvents::ScanForNearbyEntities() {
    ZoneScoped;

    if (!m_b1) {
        return;
    }

    const auto curTime = CTimer::GetTimeInMS();
    if (curTime - m_nLastScanTimeUpdate < 500) {
        return;
    }
    m_nLastScanTimeUpdate = curTime;

    CPlayerPed* player = FindPlayerPed();
    if (!player) {
        return;
    }

    if (m_CurrentFrameCounter != CTimer::GetFrameCounter()) {
        m_CurrentFrameCounter = CTimer::GetFrameCounter();

        const auto& camPos = CCamera::GetActiveCamera().m_vecSource;
        const auto& playerPos = player->GetPosition();
        vec148 = playerPos - camPos;
        vec148.z = 0.0f;
        if (vec148.NormaliseAndMag() == 0.0f) {
            vec148 = player->GetMatrix().GetForward();
        }
        m_vecCenter = playerPos + vec148 * m_fRadius;
    }

    const int32 startSectorX = std::max(0, CWorld::GetSectorX(m_vecCenter.x - m_fRadius));
    const int32 startSectorY = std::max(0, CWorld::GetSectorY(m_vecCenter.y - m_fRadius));
    const int32 endSectorX   = std::min(MAX_SECTORS_X - 1, CWorld::GetSectorX(m_vecCenter.x + m_fRadius));
    const int32 endSectorY   = std::min(MAX_SECTORS_Y - 1, CWorld::GetSectorY(m_vecCenter.y + m_fRadius));

    CWorld::AdvanceCurrentScanCode();
    player->SetCurrentScanCode();

    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            auto& rs = CWorld::GetRepeatSector(sectorX, sectorY);

            for (auto* const ped : rs.Peds) {
                if (ped->IsScanCodeCurrent()) {
                    continue;
                }

                ped->SetCurrentScanCode();

                if (ped->m_nPedState == PEDSTATE_DEAD) {
                    continue;
                }

                CEntity* entity = (ped->bInVehicle && ped->m_pVehicle) ? (CEntity*)ped->m_pVehicle : (CEntity*)ped;

                switch (ped->m_nPedType) {
                case PED_TYPE_COP:
                    Add(INTERESTING_EVENT_5, entity);
                    break;
                case PED_TYPE_CRIMINAL:
                    Add(INTERESTING_EVENT_6, entity);
                    break;
                case PED_TYPE_PROSTITUTE:
                    Add(INTERESTING_EVENT_4, entity);
                    break;
                default:
                    if (ped->m_nPedType >= PED_TYPE_GANG1 && ped->m_nPedType <= PED_TYPE_GANG10) {
                        Add(INTERESTING_EVENT_7, entity);
                    }
                    break;
                }
            }

            for (auto* const vehicle : rs.Vehicles) {
                if (vehicle->IsScanCodeCurrent()) {
                    continue;
                }

                vehicle->SetCurrentScanCode();
                if (vehicle->physicalFlags.bRenderScorched) {
                    continue;
                }

                if (!vehicle->m_pDriver) {
                    continue;
                }

                const auto style = vehicle->m_autoPilot.m_nCarDrivingStyle;
                if (!style) {
                    continue;
                }

                if (style == DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS) {
                    continue;
                }

                Add(INTERESTING_EVENT_14, vehicle);
            }
        }
    }
}

// 0x6028A0
TInterestingEvent* CInterestingEvents::GetInterestingEvent() {
    const auto curTime = CTimer::GetTimeInMS();

    if (m_b4 && m_nInterestingEvent != -1) {
        TInterestingEvent& curEvent = m_Events[m_nInterestingEvent];
        if (curEvent.entity && curTime < curEvent.time + (uint32)m_nDelays[curEvent.type]) {
            return &curEvent;
        }
    }

    uint32 maxPriority = 0;
    int32 bestEvent = -1;

    for (int32 i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        TInterestingEvent& event = m_Events[i];
        if (!event.entity) {
            continue;
        }

        if (curTime >= event.time + (uint32)m_nDelays[event.type]) {
            continue;
        }

        const uint32 priority = m_nPriorities[event.type];
        if (priority <= maxPriority) {
            if ((uint16)CGeneral::GetRandomNumber() >= 128) {
                continue;
            }
        }

        maxPriority = priority;
        bestEvent = i;
    }

    m_nInterestingEvent = (int8)bestEvent;
    return (bestEvent == -1) ? nullptr : &m_Events[bestEvent];
}

// 0x602960
void CInterestingEvents::InvalidateEvent(const TInterestingEvent* event) {
    for (auto index = 0; index < MAX_INTERESTING_EVENTS; index++) {
        TInterestingEvent* tevent = &m_Events[index];
        if (tevent != event)
            continue;

        tevent->time = 0;
        CEntity::ClearReference(tevent->entity);
        if (m_nInterestingEvent == index) {
            m_nInterestingEvent = -1;
        }
    }
}

// 0x6029C0
void CInterestingEvents::InvalidateNonVisibleEvents() {
    const auto& camPos = CCamera::GetActiveCamera().m_vecSource;
    for (auto i = 0; i < MAX_INTERESTING_EVENTS; i++) {
        TInterestingEvent& event = m_Events[i];
        if (!event.entity)
            continue;

        CVector pos = event.entity->GetPosition();
        if (CWorld::GetIsLineOfSightClear(camPos, pos, true, false, false, false, false, true, false))
            continue;

        event.time = 0;
        CEntity::ClearReference(event.entity);
        if (m_nInterestingEvent == i) {
            m_nInterestingEvent = -1;
        }
    }
}
