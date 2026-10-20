#include "StdInc.h"

#include "StencilShadowObject.h"

void CStencilShadowObject::InjectHooks() {
    RH_ScopedClass(CStencilShadowObject);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x70F8D0);
    RH_ScopedInstall(Destructor, 0x70F8E0);
    RH_ScopedInstall(Destroy, 0x711310);
    RH_ScopedInstall(Allocate, 0x711280);
}

// 0x711280
CStencilShadowObject* CStencilShadowObject::Allocate(CEntity* entity, eStencilShadowObjType type) {
    if (type <= eStencilShadowObjType::NONE || type > eStencilShadowObjType::VEHICLE) {
        return nullptr;
    }

    auto* modelInfo = entity->GetModelInfo();
    if (!modelInfo) {
        return nullptr;
    }

    auto* colModel = modelInfo->GetColModel();
    if (!colModel || !colModel->m_pColData || !colModel->m_pColData->bHasShadowInfo) {
        return nullptr;
    }

    const auto numShadowTriangles = static_cast<int32>(colModel->m_pColData->m_nNumShadowTriangles);
    if (numShadowTriangles <= 0) {
        return nullptr;
    }

    m_NumShadowFaces = static_cast<int16>(numShadowTriangles);
    m_FaceID = static_cast<uint32>(numShadowTriangles * 15);
    m_pOwner = entity;
    m_Type = type;
    m_ShadowFacesData = static_cast<CVector*>(CMemoryMgr::Malloc(m_FaceID * sizeof(CVector)));
    CEntity::RegisterReference(m_pOwner);
    CStencilShadows::UpdateHierarchy(CStencilShadows::pFirstAvailableStencilShadowObject, CStencilShadows::pFirstActiveStencilShadowObject, this);
    return this;
}

// 0x711310
void CStencilShadowObject::Destroy() {
    CEntity::SafeCleanUpRef(m_pOwner);
    m_pOwner                = nullptr;
    m_Type                  = eStencilShadowObjType::NONE;
    m_SizeOfShadowFacesData = 0;
    m_FaceID                = 0;

    if (m_ShadowFacesData) {
        CMemoryMgr::Free(std::exchange(m_ShadowFacesData, nullptr));
    }

    if (m_pPrev) {
        if (m_pNext) {
            m_pNext->m_pPrev = m_pPrev;
            m_pPrev->m_pNext = m_pNext;
        } else {
            m_pPrev->m_pNext = nullptr;
        }
    } else {
        CStencilShadows::pFirstActiveStencilShadowObject = m_pNext;
        if (CStencilShadows::pFirstActiveStencilShadowObject) {
            CStencilShadows::pFirstActiveStencilShadowObject->m_pPrev = nullptr;
        }
    }
    m_pNext = CStencilShadows::pFirstAvailableStencilShadowObject;
    m_pPrev = nullptr;
    CStencilShadows::pFirstAvailableStencilShadowObject = this;

    if (m_pNext) {
        m_pNext->m_pPrev = this;
    }
}
