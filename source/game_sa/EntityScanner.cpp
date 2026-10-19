#include "StdInc.h"

#include "EntityScanner.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "World.h"

void CEntityScanner::InjectHooks() {
    RH_ScopedClass(CEntityScanner);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FF990);
    RH_ScopedInstall(Destructor, 0x603480);

    RH_ScopedInstall(Clear, 0x5FF9D0);
    RH_ScopedInstall(ScanForEntitiesInRange, 0x5FFA20);
}

// 0x5FF990
CEntityScanner::CEntityScanner() {
    m_timer = {};

    rng::fill(m_apEntities, nullptr);
    m_pClosestEntityInRange = nullptr;

    m_timer.SetPeriod(MAX_NUM_ENTITIES);
}

// 0x603480
CEntityScanner::~CEntityScanner() {
    Clear();
}

// 0x5FF9D0
void CEntityScanner::Clear() {
    for (auto& entity : m_apEntities) {
        CEntity::ClearReference(entity);
    }

    CEntity::ClearReference(m_pClosestEntityInRange);
}

// 0x5FFA20
void CEntityScanner::ScanForEntitiesInRange(const eRepeatSectorList sectorList, const CPed& ped) {
    if (!m_timer.Tick()) {
        return;
    }

    Clear();

    const bool isPlayerPed = (&ped == FindPlayerPed(-1));
    const auto intel = ped.GetIntelligence();
    const float scanRange = std::max(intel->m_fHearingRange, intel->m_fSeeingRange);
    const CVector& pedPos = ped.GetPosition();

    const int32 minSectorX = std::max(0, static_cast<int32>((pedPos.x - scanRange) * 0.02f + 60.0f));
    const int32 maxSectorX = std::min(119, static_cast<int32>((pedPos.x + scanRange) * 0.02f + 60.0f));
    const int32 minSectorY = std::max(0, static_cast<int32>((pedPos.y - scanRange) * 0.02f + 60.0f));
    const int32 maxSectorY = std::min(119, static_cast<int32>((pedPos.y + scanRange) * 0.02f + 60.0f));

    CWorld::AdvanceCurrentScanCode();
    const_cast<CPed&>(ped).m_nScanCode = CWorld::ms_nCurrentScanCode;

    const float scanRangeSq = sq(scanRange);
    int32 count = 0;
    float distSqArray[MAX_NUM_ENTITIES];

    for (int32 y = minSectorY; y <= maxSectorY; ++y) {
        for (int32 x = minSectorX; x <= maxSectorX; ++x) {
            auto& repeatSector = CWorld::GetRepeatSector(x, y);

            auto processEntity = [&](CEntity* entity) {
                if (entity->m_nScanCode == CWorld::ms_nCurrentScanCode) {
                    return;
                }
                entity->m_nScanCode = CWorld::ms_nCurrentScanCode;

                if (sectorList == REPEATSECTOR_PEDS) {
                    auto* entityPed = static_cast<CPed*>(entity);
                    if (!isPlayerPed && entityPed->m_nPedState == PEDSTATE_STAGGER) {
                        return;
                    }
                }

                const float distSq = DistanceBetweenPointsSquared(entity->GetPosition(), pedPos);
                if (distSq > scanRangeSq) {
                    return;
                }

                for (int32 i = 0; i < static_cast<int32>(MAX_NUM_ENTITIES); ++i) {
                    if (!m_apEntities[i]) {
                        m_apEntities[i] = entity;
                        distSqArray[i] = distSq;
                        count++;
                        break;
                    }
                    if (distSq < distSqArray[i]) {
                        const int32 shiftFrom = std::min(count, static_cast<int32>(MAX_NUM_ENTITIES) - 1);
                        for (int32 k = shiftFrom; k > i; --k) {
                            m_apEntities[k] = m_apEntities[k - 1];
                            distSqArray[k] = distSqArray[k - 1];
                        }
                        m_apEntities[i] = entity;
                        distSqArray[i] = distSq;
                        if (count < static_cast<int32>(MAX_NUM_ENTITIES)) {
                            count++;
                        }
                        break;
                    }
                }
            };

            switch (sectorList) {
            case REPEATSECTOR_VEHICLES:
                for (auto* veh : repeatSector.Vehicles) {
                    processEntity(veh);
                }
                break;
            case REPEATSECTOR_PEDS:
                for (auto* p : repeatSector.Peds) {
                    processEntity(p);
                }
                break;
            case REPEATSECTOR_OBJECTS:
                for (auto* obj : repeatSector.Objects) {
                    processEntity(obj);
                }
                break;
            default:
                break;
            }
        }
    }

    for (int32 i = 0; i < count; ++i) {
        if (m_apEntities[i]) {
            CEntity::SafeRegisterRef(m_apEntities[i]);
        }
    }

    if (m_apEntities[0]) {
        m_pClosestEntityInRange = m_apEntities[0];
        CEntity::SafeRegisterRef(m_pClosestEntityInRange);
    }
}
