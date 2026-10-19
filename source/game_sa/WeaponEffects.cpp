/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "WeaponEffects.h"

void CWeaponEffects::InjectHooks() {
    RH_ScopedClass(CWeaponEffects);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x742AB0);
    RH_ScopedInstall(Shutdown, 0x742B80);
    RH_ScopedInstall(IsLockedOn, 0x742BD0);
    RH_ScopedInstall(MarkTarget, 0x742BF0);
    RH_ScopedInstall(ClearCrossHair, 0x742C60);
    RH_ScopedInstall(ClearCrossHairs, 0x742C80);
    RH_ScopedInstall(ClearCrossHairImmediately, 0x742CA0);
    RH_ScopedInstall(ClearCrossHairsImmediately, 0x742CC0);
    RH_ScopedInstall(Render, 0x742CF0);
}

// 0x742AB0
void CWeaponEffects::Init() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_vecPosn = CVector();
        crossHair.m_bActive = false;
        crossHair.m_color = CRGBA(255, 0, 0, 127);
        crossHair.m_fSize = 1.0f;
        crossHair.field_1C = 0;
        crossHair.m_nTimeWhenToDeactivate = 0;
        crossHair.m_bClearImmediately = false;
        crossHair.m_fRotation = 0.0f;
    }

    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));
    gpCrossHairTex          = RwTextureRead("target256",  "target256m");
    gpCrossHairTexFlight[0] = RwTextureRead("lockon",     "lockonA");
    gpCrossHairTexFlight[1] = RwTextureRead("lockonFire", "lockonFireA");
    CTxdStore::PopCurrentTxd();
}

// 0x742B80
void CWeaponEffects::Shutdown() {
    for (auto& crossHair : gpCrossHairTexFlight) {
        RwTextureDestroy(crossHair);
        crossHair = nullptr;
    }

    RwTextureDestroy(gpCrossHairTex);
    gpCrossHairTex = nullptr;
}

// 0x742BD0
bool CWeaponEffects::IsLockedOn(CrossHairId id) {
    return gCrossHair[id].m_fRotation;
}

// 0x742BF0
void CWeaponEffects::MarkTarget(CrossHairId id, CVector posn, uint8 red, uint8 green, uint8 blue, uint8 alpha, float size, bool bClearImmediately) {
    auto& crossHair = gCrossHair[id];
    crossHair.m_vecPosn               = posn;
    crossHair.m_color.r               = red;
    crossHair.m_color.g               = green;
    crossHair.m_color.b               = blue;
    crossHair.m_color.a               = alpha;
    crossHair.m_bActive               = true;
    crossHair.m_fSize                 = size;
    crossHair.m_nTimeWhenToDeactivate = -1;
    crossHair.m_bClearImmediately     = bClearImmediately;
}

// 0x742C60
void CWeaponEffects::ClearCrossHair(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() + 400);
}

// 0x742C80
void CWeaponEffects::ClearCrossHairs() {
    for (auto& crossHair : gCrossHair) {
        crossHair.m_bActive = false;
    }
}

// 0x742CA0
void CWeaponEffects::ClearCrossHairImmediately(CrossHairId id) {
    gCrossHair[id].m_nTimeWhenToDeactivate = static_cast<int32>(CTimer::GetTimeInMS() - 100);
    gCrossHair[id].m_bActive = false;
}

// 0x742CC0
void CWeaponEffects::ClearCrossHairsImmediately() {
    for (auto i = 0u; i < gCrossHair.size(); i++) {
        ClearCrossHair(i);
    }
}

// 0x742CF0
void CWeaponEffects::Render() {
    ZoneScoped;

    if (FrontEndMenuManager.m_bMenuActive) {
        return;
    }

    for (size_t i = 0; i < gCrossHair.size(); i++) {
        auto& crossHair = gCrossHair[i];
        if (crossHair.m_nTimeWhenToDeactivate != 0 && (uint32)crossHair.m_nTimeWhenToDeactivate < CTimer::GetTimeInMS()) {
            crossHair.m_bActive = false;
            crossHair.m_nTimeWhenToDeactivate = 0;
            crossHair.m_fRotation = 0.0f;
        }

        if (crossHair.m_nTimeWhenToDeactivate != -1) {
            crossHair.m_fRotation = 0.0f;
        }

        CPed* player = FindPlayerPed((int32)i);
        CEntity* target = player ? player->m_pTargetedObject : nullptr;
        if (!target) {
            crossHair.m_bActive = false;
        }

        if (!crossHair.m_bActive) {
            continue;
        }

        if (crossHair.m_bClearImmediately) {
            RwV3d screenPos{};
            float szX{}, szY{};
            if (CSprite::CalcScreenCoors(crossHair.m_vecPosn, &screenPos, &szX, &szY, true, true)) {
                float invZ = 1.0f / screenPos.z;
                if (invZ <= 1.0f) {
                    szX *= invZ;
                    szY *= invZ;
                }

                RwRenderStateSet(rwRENDERSTATEZTESTENABLE,      RWRSTATE(FALSE));
                RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,     RWRSTATE(FALSE));
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
                RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
                RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
                RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCrossHairTexFlight[0])));

                static const float s_fScaleMult[2] = { 0.95f, 1.05f };
                for (int32 j = 0; j < 2; j++) {
                    float sX = std::max(1.0f, szX) * s_fScaleMult[j] * 0.04f;
                    float sY = std::max(1.0f, szY) * s_fScaleMult[j] * 0.04f;
                    CSprite::RenderOneXLUSprite(screenPos.x, screenPos.y, screenPos.z, sX, sY, 0, 0, 0, 255, 1.0f / 40.0f, 0, 255);
                }

                float sX = std::max(1.0f, szX) * 0.04f;
                float sY = std::max(1.0f, szY) * 0.04f;
                CSprite::RenderOneXLUSprite(screenPos.x, screenPos.y, screenPos.z, sX, sY, crossHair.m_color.r, crossHair.m_color.g, crossHair.m_color.b, crossHair.m_color.a, 1.0f / 40.0f, 0, 255);

                RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpCrossHairTexFlight[1])));

                float angle = (float)(CTimer::GetTimeInMS() & 0x3FF) * (2.0f * PI / 1024.0f);
                crossHair.m_fRotation -= 1.0f * angle;
                if (crossHair.m_fRotation <= 0.0f) {
                    crossHair.m_fRotation = 0.0f;
                }
                crossHair.field_1C = (crossHair.m_fRotation > 0.0f) ? 1 : 0;

                float sinA = std::sin(angle) * crossHair.m_fRotation;
                float cosA = std::cos(angle) * crossHair.m_fRotation;

                for (int32 j = 0; j < 2; j++) {
                    float ssX = std::max(1.0f, szX) * s_fScaleMult[j] * 0.02f;
                    float ssY = std::max(1.0f, szY) * s_fScaleMult[j] * 0.02f;
                    CSprite::RenderOneXLUSprite(screenPos.x - cosA, screenPos.y - sinA, screenPos.z, ssX, ssY, 0, 0, 0, 255, 1.0f / 40.0f, 0, 255);
                }

                float ssX = std::max(1.0f, szX) * 0.02f;
                float ssY = std::max(1.0f, szY) * 0.02f;
                CSprite::RenderOneXLUSprite(screenPos.x - cosA, screenPos.y - sinA, screenPos.z, ssX, ssY, crossHair.m_color.r, crossHair.m_color.g, crossHair.m_color.b, 255, 1.0f / 40.0f, 0, 255);
            }
        } else {
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,      RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,     RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(nullptr));

            RwV3d screenPos{};
            float szX{}, szY{};
            if (CSprite::CalcScreenCoors(crossHair.m_vecPosn, &screenPos, &szX, &szY, false, false)) {
                if (screenPos.x >= 0.0f && screenPos.x <= (float)RsGlobal.maximumWidth &&
                    screenPos.y >= 0.0f && screenPos.y <= (float)RsGlobal.maximumHeight)
                {
                    float size = std::clamp(crossHair.m_fSize, 0.3f, 1.2f);
                    crossHair.m_fSize = size;
                }
            }
        }
    }

    for (int32 p = 0; p < 2; p++) {
        FindPlayerInfo(p).RenderWeaponTargeting(p);
    }
}
