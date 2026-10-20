#include "StdInc.h"
#include "StencilShadowObject.h"

// TODO: Statically allocate after reversing RenderForVehicle&RenderForObject.
static inline auto& s_ShadowTrianglePointsUnk         = StaticRef<RxVertexIndex*>(0xC6A170);
static inline auto& s_ShadowTrianglePoints            = StaticRef<CVector*>(0xC6A174);
static inline auto& s_TransformedShadowTrianglePoints = StaticRef<CVector*>(0xC6A178);
static inline auto& s_SunPosNrm                       = StaticRef<CVector>(0x8D5244); // CVector(1.0, 1.0, -2.0)

void CStencilShadows::InjectHooks() {
    RH_ScopedClass(CStencilShadows);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x70F9E0);
    RH_ScopedInstall(Shutdown, 0x711390);
    RH_ScopedInstall(Process, 0x711D90);
    RH_ScopedInstall(GraphicsHighQuality, 0x70F9B0);
    RH_ScopedInstall(UpdateHierarchy, 0x710BC0);
    RH_ScopedInstall(RegisterStencilShadows, 0x711760);
    RH_ScopedInstall(RenderStencilShadows, 0x7113B0);
    RH_ScopedInstall(RenderForVehicle, 0x70FAE0);
    RH_ScopedInstall(RenderForObject, 0x710310);
    RH_ScopedInstall(Render, 0x710D50);
    RH_ScopedInstall(RenderBuffer, 0x710B50);
    RH_ScopedInstall(SunSetPositionFromEntity, 0x710AF0);
    RH_ScopedInstall(sub_710CC0, 0x710CC0);
}

// 0x70F9E0
void CStencilShadows::Init() {
    ZoneScoped;

    RwD3D9SetStencilClear(0);
    pFirstAvailableStencilShadowObject = m_StencilShadowObjects.data();
    pFirstActiveStencilShadowObject = nullptr;

    for (auto&& [i, obj] : rngv::enumerate(m_StencilShadowObjects)) {
        obj.m_pOwner                = nullptr;
        obj.m_NumShadowFaces        = 0;
        obj.m_Type                  = eStencilShadowObjType::NONE;
        obj.m_SizeOfShadowFacesData = 0;
        obj.m_FaceID                = 0;
        obj.m_ShadowFacesData       = 0;

        obj.m_pPrev = i ? &m_StencilShadowObjects[i - 1] : nullptr;
        obj.m_pNext = (i != m_StencilShadowObjects.size() - 1) ? &m_StencilShadowObjects[i + 1] : nullptr;
    }
}

// 0x711390
void CStencilShadows::Shutdown() {
    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* next = obj->m_pNext;
        obj->Destroy();
        obj = next;
    }
}

// 0x710D50
void CStencilShadows::Render(const CRGBA& color) {
    uiTempBufferIndicesStored = 0;
    uiTempBufferVerticesStored = 0;

    const auto intColor = color.ToIntARGB();

    for (auto* shadow = pFirstActiveStencilShadowObject; shadow; shadow = shadow->m_pNext) {
        auto* facePts = shadow->m_ShadowFacesData;
        const auto numQuads = shadow->m_SizeOfShadowFacesData / 6;

        for (uint32 quad = 0; quad < numQuads; quad++) {
            sub_710CC0(6, 6);

            for (uint32 i = 0; i < 6; i++) {
                aTempBufferIndices[uiTempBufferIndicesStored + i] = static_cast<RxVertexIndex>(uiTempBufferVerticesStored + i);
                TempBufferVertices.m_3d[uiTempBufferVerticesStored + i].objVertex = facePts[i];
                TempBufferVertices.m_3d[uiTempBufferVerticesStored + i].color = intColor;
            }

            uiTempBufferIndicesStored += 6;
            uiTempBufferVerticesStored += 6;
            facePts += 6;
        }

        if ((shadow->m_SizeOfShadowFacesData / 3) & 1) {
            sub_710CC0(3, 3);

            for (uint32 i = 0; i < 3; i++) {
                aTempBufferIndices[uiTempBufferIndicesStored + i] = static_cast<RxVertexIndex>(uiTempBufferVerticesStored + i);
                TempBufferVertices.m_3d[uiTempBufferVerticesStored + i].objVertex = facePts[i];
                TempBufferVertices.m_3d[uiTempBufferVerticesStored + i].color = intColor;
            }

            uiTempBufferIndicesStored += 3;
            uiTempBufferVerticesStored += 3;
        }
    }

    if (uiTempBufferIndicesStored && uiTempBufferVerticesStored) {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        LittleTest();
        if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
            RwIm3DEnd();
        }
        uiTempBufferVerticesStored = 0;
        uiTempBufferIndicesStored = 0;
    }
}

// unused
// 0x710AF0
void CStencilShadows::SunSetPositionFromEntity(const CEntity* entity) {
    if (!entity) {
        return;
    }
    s_SunPosNrm = entity->GetPosition().Normalized();
}

// 0x710B50
void CStencilShadows::RenderBuffer(const CVector& pos) {
    s_SunPosNrm = pos.Normalized(); 
}

// 0x710CC0
void CStencilShadows::sub_710CC0(int32 indices, int32 vertices) {
    if (uiTempBufferIndicesStored + indices < TOTAL_TEMP_BUFFER_INDICES
        && uiTempBufferVerticesStored + vertices < TOTAL_TEMP_BUFFER_3DVERTICES) {
        return;
    }

    if (!uiTempBufferIndicesStored || !uiTempBufferVerticesStored) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    LittleTest();
    if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
        RwIm3DEnd();
    }
    uiTempBufferVerticesStored = uiTempBufferIndicesStored = 0;
}

// 0x7113B0
void CStencilShadows::RenderStencilShadows() {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONMASK,      RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONALWAYS));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONINCR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLFRONT));

    Render(CRGBA{ 0, 0, 0, 255 });

    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONDECR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));

    Render(CRGBA{ 0, 0, 0, 255 });

    // WTF is up with these states?
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE)); // same state
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE)); // same state
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(1u));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONLESSEQUAL));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));

    CSprite2d::InitPerFrame();
    CSprite2d::DrawRect(
        CRect{ 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT },
        CRGBA{ 0, 0, 0, (uint8)(50u * CTimeCycle::m_CurrentColours.m_nShadowStrength / 256) }
    );

    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
}

struct StencilShadowEdge {
    RxVertexIndex vA;
    RxVertexIndex vB;
};

// 0x70FA70
static void AddSilhouetteEdge(StencilShadowEdge* edges, uint16& numEdges, RxVertexIndex vA, RxVertexIndex vB) {
    for (uint32 i = 0; i < numEdges; i++) {
        if ((edges[i].vA == vA && edges[i].vB == vB) || (edges[i].vA == vB && edges[i].vB == vA)) {
            if (numEdges > 1) {
                edges[i] = edges[numEdges - 1];
            }
            numEdges--;
            return;
        }
    }
    edges[numEdges] = { vA, vB };
    numEdges++;
}

static void RenderForEntity(CStencilShadowObject* object, float shadowLength, CMatrix* matrix) {
    auto* owner = object->m_pOwner;
    if (!owner) {
        return;
    }

    auto* modelInfo = owner->GetModelInfo();
    if (!modelInfo) {
        return;
    }

    auto* colModel = modelInfo->GetColModel();
    if (!colModel || !colModel->m_pColData) {
        return;
    }

    auto* colData = colModel->m_pColData;
    if (colData->m_nNumShadowTriangles != static_cast<uint32>(object->m_NumShadowFaces)) {
        return;
    }

    CMatrix invMatrix;
    Invert(*matrix, invMatrix);
    const CVector sunDirLocal = invMatrix.TransformVector(s_SunPosNrm);

    for (uint32 i = 0; i < colData->m_nNumShadowVertices; i++) {
        colData->GetShadTrianglePoint(s_ShadowTrianglePoints[i], static_cast<int32>(i));
    }

    TransformPoints(
        reinterpret_cast<RwV3d*>(s_TransformedShadowTrianglePoints),
        colData->m_nNumShadowVertices,
        *matrix,
        reinterpret_cast<RwV3d*>(s_ShadowTrianglePoints)
    );

    uint16 numEdges = 0;
    auto* edges = reinterpret_cast<StencilShadowEdge*>(s_ShadowTrianglePointsUnk);

    object->m_SizeOfShadowFacesData = 0;

    for (uint32 i = 0; i < colData->m_nNumShadowTriangles; i++) {
        const auto& tri = colData->m_pShadowTriangles[i];
        const auto& v0 = s_ShadowTrianglePoints[tri.m_nVertA];
        const auto& v1 = s_ShadowTrianglePoints[tri.m_nVertB];
        const auto& v2 = s_ShadowTrianglePoints[tri.m_nVertC];

        const CVector normal = CrossProduct(v1 - v0, v2 - v0);
        const float dot = DotProduct(normal, sunDirLocal);

        const auto& pA = s_TransformedShadowTrianglePoints[tri.m_nVertA];
        const auto& pB = s_TransformedShadowTrianglePoints[tri.m_nVertB];
        const auto& pC = s_TransformedShadowTrianglePoints[tri.m_nVertC];

        if (dot >= 0.0f) {
            AddSilhouetteEdge(edges, numEdges, tri.m_nVertA, tri.m_nVertB);
            AddSilhouetteEdge(edges, numEdges, tri.m_nVertB, tri.m_nVertC);
            AddSilhouetteEdge(edges, numEdges, tri.m_nVertC, tri.m_nVertA);

            if (object->m_SizeOfShadowFacesData + 3 < object->m_FaceID) {
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 0] = pA;
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 1] = pC;
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 2] = pB;
                object->m_SizeOfShadowFacesData += 3;
            }
        } else {
            const CVector ext = s_SunPosNrm * shadowLength;
            if (object->m_SizeOfShadowFacesData + 3 < object->m_FaceID) {
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 0] = pA + ext;
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 1] = pC + ext;
                object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 2] = pB + ext;
                object->m_SizeOfShadowFacesData += 3;
            }
        }
    }

    const CVector ext = s_SunPosNrm * shadowLength;
    for (uint32 i = 0; i < numEdges; i++) {
        if (object->m_SizeOfShadowFacesData + 6 >= object->m_FaceID) {
            break;
        }

        const auto& pA = s_TransformedShadowTrianglePoints[edges[i].vA];
        const auto& pB = s_TransformedShadowTrianglePoints[edges[i].vB];
        const CVector pA_ext = pA + ext;
        const CVector pB_ext = pB + ext;

        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 0] = pA;
        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 1] = pB;
        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 2] = pA_ext;
        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 3] = pB;
        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 4] = pB_ext;
        object->m_ShadowFacesData[object->m_SizeOfShadowFacesData + 5] = pA_ext;
        object->m_SizeOfShadowFacesData += 6;
    }
}

// 0x70FAE0
void CStencilShadows::RenderForVehicle(CStencilShadowObject* object) {
    auto* owner = object->m_pOwner;
    if (!owner) {
        return;
    }

    float shadowLength = 5.0f;
    if (owner->GetIsTypeVehicle()) {
        auto* vehicle = owner->AsVehicle();
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI || vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE) {
            shadowLength = 40.0f;
        }
    }

    if (!owner->m_matrix) {
        owner->AllocateMatrix();
        owner->m_placement.UpdateMatrix(owner->m_matrix);
    }

    CMatrix* pMatrix = owner->m_matrix;
    if (owner->GetIsTypeVehicle()) {
        auto* vehicle = owner->AsVehicle();
        if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            auto* bike = owner->AsBike();
            bike->CalculateLeanMatrix();
            pMatrix = &bike->m_mLeanMatrix;
        }
    }

    RenderForEntity(object, shadowLength, pMatrix);
}

// 0x710310
void CStencilShadows::RenderForObject(CStencilShadowObject* object) {
    auto* owner = object->m_pOwner;
    if (!owner) {
        return;
    }

    if (!owner->m_matrix) {
        owner->AllocateMatrix();
        owner->m_placement.UpdateMatrix(owner->m_matrix);
    }

    RenderForEntity(object, 60.0f, owner->m_matrix);
}

// 0x711D90
void CStencilShadows::Process(CVector& cameraPos) {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    static uint8 s_RegisterShadowCounter{}, s_RenderForObjCounter{};

    RegisterStencilShadows(cameraPos, ++s_RegisterShadowCounter % 8);

    // why do we even do this?
    s_ShadowTrianglePointsUnk         = (RxVertexIndex*)CMemoryMgr::Malloc(12'288 * sizeof(RxVertexIndex));
    s_ShadowTrianglePoints            = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));
    s_TransformedShadowTrianglePoints = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));

    auto i{ 0 };
    for (auto* obj = pFirstActiveStencilShadowObject; obj; obj = obj->m_pNext) {
        switch (obj->m_Type) {
        case eStencilShadowObjType::OBJECT:
            if ((i++ % 4) == s_RenderForObjCounter) {
                RenderForObject(obj);
            }
            break;
        case eStencilShadowObjType::VEHICLE:
            RenderForVehicle(obj);
            break;
        default:
            break;
        }
    }
    s_RenderForObjCounter = (s_RenderForObjCounter + 1) % 4;

    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePointsUnk, nullptr));
    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePoints, nullptr));
    CMemoryMgr::Free(std::exchange(s_TransformedShadowTrianglePoints, nullptr));
}

// 0x70F9B0
bool CStencilShadows::GraphicsHighQuality() {
    return ::GraphicsHighQuality();
}

// 0x710BC0
void CStencilShadows::UpdateHierarchy(CStencilShadowObject*& firstAvailable, CStencilShadowObject*& firstActive, CStencilShadowObject* newOne) {
    if (auto* prev = newOne->m_pPrev) {
        auto* next = newOne->m_pNext;
        if (next) {
            next->m_pPrev = prev;
            newOne->m_pPrev->m_pNext = newOne->m_pNext;
        } else {
            prev->m_pNext = nullptr;
        }
    } else {
        auto* next     = newOne->m_pNext;
        firstAvailable = next;
        if (next) {
            next->m_pPrev = nullptr;
        }
    }
    newOne->m_pNext = firstActive;
    newOne->m_pPrev = nullptr;
    firstActive     = newOne;

    if (newOne->m_pNext) {
        newOne->m_pNext->m_pPrev = newOne;
    }
}

// 0x711160 / 0x7111F0
static float GetDistanceSqToEntityBoundingSphere(const CEntity* entity, const CVector& cameraPos) {
    const auto dist = DistanceBetweenPoints(cameraPos, entity->GetBoundCentre());
    const auto radius = entity->GetModelInfo()->GetColModel()->GetBoundRadius();
    if (dist < radius) {
        return -1.0f;
    }
    const auto d = dist - radius;
    return d * d;
}

// 0x711760
void CStencilShadows::RegisterStencilShadows(CVector& cameraPos, bool doNotCreateNew) {
    if (doNotCreateNew) {
        for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
            auto* next = obj->m_pNext;
            auto* owner = obj->m_pOwner;
            if (!owner) {
                obj->Destroy();
            } else if (owner->GetIsTypeObject()) {
                auto* object = owner->AsObject();
                if (!object->m_bIsVisible || object->objectFlags.bIsExploded || object->objectFlags.bIsBroken) {
                    obj->Destroy();
                }
            }
            obj = next;
        }
        return;
    }

    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* next = obj->m_pNext;
        auto* owner = obj->m_pOwner;
        if (!owner) {
            obj->Destroy();
        } else if (owner->GetIsTypeObject() && (!owner->m_bIsVisible || owner->AsObject()->objectFlags.bIsExploded || owner->AsObject()->objectFlags.bIsBroken)) {
            obj->Destroy();
        } else {
            switch (obj->m_Type) {
            case eStencilShadowObjType::OBJECT: {
                if (GetDistanceSqToEntityBoundingSphere(owner, cameraPos) > 2500.0f || !owner->IsInCurrentArea()) {
                    obj->Destroy();
                }
                break;
            }
            case eStencilShadowObjType::VEHICLE: {
                if (GetDistanceSqToEntityBoundingSphere(owner, cameraPos) > 2500.0f) {
                    obj->Destroy();
                }
                break;
            }
            default:
                break;
            }
        }
        obj = next;
    }

    if (!pFirstAvailableStencilShadowObject) {
        return;
    }

    auto isRegistered = [](const CEntity* entity) {
        for (auto* shadow = pFirstActiveStencilShadowObject; shadow; shadow = shadow->m_pNext) {
            if (shadow->m_pOwner == entity) {
                return true;
            }
        }
        return false;
    };

    for (int32 i = 0; i < CPools::ms_pVehiclePool->GetSize(); i++) {
        auto* vehicle = CPools::ms_pVehiclePool->GetAt(i);
        if (!vehicle) {
            continue;
        }

        auto* modelInfo = vehicle->GetModelInfo();
        if (!modelInfo) {
            continue;
        }

        auto* colModel = modelInfo->GetColModel();
        if (!colModel || !colModel->m_pColData || !colModel->m_pColData->bHasShadowInfo) {
            continue;
        }

        if (!vehicle->GetRwObject()) {
            continue;
        }

        if (isRegistered(vehicle)) {
            continue;
        }

        if (GetDistanceSqToEntityBoundingSphere(vehicle, cameraPos) <= 2500.0f) {
            if (!pFirstAvailableStencilShadowObject) {
                return;
            }
            pFirstAvailableStencilShadowObject->Allocate(vehicle, eStencilShadowObjType::VEHICLE);
            if (!pFirstAvailableStencilShadowObject) {
                return;
            }
        }
    }

    if (!pFirstAvailableStencilShadowObject) {
        return;
    }

    const auto minSectorX = std::max(0, CWorld::GetSectorX(cameraPos.x - 50.0f));
    const auto maxSectorX = std::min(MAX_SECTORS_X - 1, CWorld::GetSectorX(cameraPos.x + 50.0f));
    const auto minSectorY = std::max(0, CWorld::GetSectorY(cameraPos.y - 50.0f));
    const auto maxSectorY = std::min(MAX_SECTORS_Y - 1, CWorld::GetSectorY(cameraPos.y + 50.0f));

    CWorld::AdvanceCurrentScanCode();

    for (int32 y = minSectorY; y <= maxSectorY; y++) {
        for (int32 x = minSectorX; x <= maxSectorX; x++) {
            auto processEntity = [&](CEntity* entity) -> bool {
                if (entity->m_bIsProcObject) {
                    return true;
                }

                if (entity->GetIsTypeObject()) {
                    if (!entity->m_bIsVisible) {
                        return true;
                    }
                    auto* obj = entity->AsObject();
                    if (obj->objectFlags.bIsExploded || obj->objectFlags.bIsBroken) {
                        return true;
                    }
                }

                auto* modelInfo = entity->GetModelInfo();
                if (!modelInfo) {
                    return true;
                }

                auto* colModel = modelInfo->GetColModel();
                if (!colModel || !colModel->m_pColData || !colModel->m_pColData->bHasShadowInfo) {
                    return true;
                }

                if (entity->IsScanCodeCurrent()) {
                    return true;
                }
                entity->SetCurrentScanCode();

                if (!entity->IsInCurrentArea()) {
                    return true;
                }

                if (CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetModelType() != MODEL_INFO_ATOMIC) {
                    return true;
                }

                if (isRegistered(entity)) {
                    return true;
                }

                if (GetDistanceSqToEntityBoundingSphere(entity, cameraPos) <= 2500.0f) {
                    if (!pFirstAvailableStencilShadowObject) {
                        return false;
                    }
                    pFirstAvailableStencilShadowObject->Allocate(entity, eStencilShadowObjType::OBJECT);
                    if (!pFirstAvailableStencilShadowObject) {
                        return false;
                    }
                }
                return true;
            };

            auto& sector = CWorld::GetSector(x, y);
            for (auto* building : sector.Buildings) {
                if (!processEntity(building)) {
                    return;
                }
            }

            auto& repeatSector = CWorld::GetRepeatSector(x, y);
            for (auto* object : repeatSector.Objects) {
                if (!processEntity(object)) {
                    return;
                }
            }
        }
    }
}
