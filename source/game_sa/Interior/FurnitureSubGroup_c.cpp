#include "StdInc.h"
#include "FurnitureSubGroup_c.h"
#include "Furniture_c.h"
#include "ModelInfo.h"

static inline auto& g_currFurnitureId = StaticRef<uint32>(0xBAB378);
static inline auto& g_furnitureStore  = StaticRef<std::array<Furniture_c, 256>>(0xBAE1F8);

void FurnitureSubGroup_c::InjectHooks() {
    RH_ScopedClass(FurnitureSubGroup_c);
    RH_ScopedCategory("Interior");

    RH_ScopedInstall(GetFurniture, 0x590EE0);
    RH_ScopedInstall(GetRandomId, 0x590FD0);
    RH_ScopedInstall(AddFurniture, 0x5C00C0);
}

// 0x5C00C0
bool FurnitureSubGroup_c::AddFurniture(uint16 modelId, int16 id, uint8 wealthMin, uint8 wealthMax, uint8 maxAng) {
    if (g_currFurnitureId >= std::size(g_furnitureStore)) {
        return false;
    }

    auto* furniture = &g_furnitureStore[g_currFurnitureId++];

    const auto  colModel = CModelInfo::GetModelInfo(modelId)->GetColModel();
    const auto& box      = colModel->GetBoundingBox();

    float dimX, dimY;
    if (m_bCanSteal) {
        dimX = box.m_vecMax.x - box.m_vecMin.x;
        dimY = box.m_vecMax.y - box.m_vecMin.y;
    } else {
        dimX = box.m_vecMax.x + 0.5f;
        dimY = box.m_vecMax.y + 0.5f;
    }

    const auto CalcWidth = [](float dim) -> int8 {
        int32 intVal = static_cast<int32>(dim);
        if (dim - static_cast<float>(intVal) > 0.02f) {
            intVal++;
        }
        return static_cast<int8>(intVal);
    };

    furniture->m_nModelId                 = modelId;
    furniture->m_nId                      = id;
    furniture->m_nWidthX                  = CalcWidth(dimX);
    furniture->m_nWidthY                  = CalcWidth(dimY);
    furniture->m_nWealthMin               = wealthMin;
    furniture->m_nWealthMax               = wealthMax;
    furniture->m_bCanPlaceInFrontOfWindow = m_bCanPlaceInFrontOfWindow;
    furniture->m_bIsTall                  = m_bIsTall;
    furniture->m_bCanSteal                = m_bCanSteal;
    furniture->m_nMaxAng                  = maxAng;

    m_Furnitures.AddItem(furniture);
    return true;
}

// 0x590EE0
Furniture_c* FurnitureSubGroup_c::GetFurniture(int16 id, uint8 wealth) {
    if (id > -1) {
        for (auto& item : m_Furnitures) {
            if (item.m_nId == id) {
                return &item;
            }
        }
        return nullptr;
    }

    if (wealth == 255) {
        const auto numItems = static_cast<int32>(m_Furnitures.GetNumItems());
        if (numItems <= 0) {
            return nullptr;
        }
        return m_Furnitures.GetItemOffset(true, CGeneral::GetRandomNumberInRange(0, numItems));
    }

    int32 matchingCount = 0;
    for (auto& item : m_Furnitures) {
        if (wealth >= static_cast<uint8>(item.m_nWealthMin) && wealth <= static_cast<uint8>(item.m_nWealthMax)) {
            matchingCount++;
        }
    }

    if (matchingCount == 0) {
        return nullptr;
    }

    const int32 target = CGeneral::GetRandomNumberInRange(0, matchingCount);
    int32 current = 0;
    for (auto& item : m_Furnitures) {
        if (wealth >= static_cast<uint8>(item.m_nWealthMin) && wealth <= static_cast<uint8>(item.m_nWealthMax)) {
            if (current == target) {
                return &item;
            }
            current++;
        }
    }

    return nullptr;
}

// 0x590FD0
int32 FurnitureSubGroup_c::GetRandomId(uint8 wealth) {
    if (auto* furniture = GetFurniture(-1, wealth)) {
        return furniture->m_nId;
    }
    return -1;
}

void FurnitureSubGroup_c::Exit() {
    m_Furnitures.RemoveAll();
}
