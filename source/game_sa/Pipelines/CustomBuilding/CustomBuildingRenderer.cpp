#include "StdInc.h"

#include "CustomBuildingRenderer.h"

#include "CustomBuildingDNPipeline.h"
#include "CustomBuildingPipeline.h"

void CCustomBuildingRenderer::InjectHooks() {
    RH_ScopedClass(CCustomBuildingRenderer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5D7EC0);
    RH_ScopedInstall(Shutdown, 0x5D7EE0);
    RH_ScopedInstall(PluginAttach, 0x5D7EF0);
    RH_ScopedInstall(AtomicSetup, 0x5D7F00);
    RH_ScopedInstall(IsCBPCPipelineAttached, 0x5D7F40);
    RH_ScopedInstall(UpdateDayNightBalanceParam, 0x5D7F80);
    RH_ScopedInstall(Update, 0x5D8050);
}

// 0x5D7EC0
bool CCustomBuildingRenderer::Initialise() {
    auto pipe = CCustomBuildingPipeline::CreatePipe();
    if (pipe)
        return CCustomBuildingDNPipeline::CreatePipe();

    return pipe;
}

// 0x5D7EE0
void CCustomBuildingRenderer::Shutdown() {
    CCustomBuildingPipeline::DestroyPipe();
    CCustomBuildingDNPipeline::DestroyPipe();
}

// 0x5D7EF0
bool CCustomBuildingRenderer::PluginAttach() {
    return CCustomBuildingDNPipeline::ExtraVertColourPluginAttach();
}

// 0x5D7F00
void CCustomBuildingRenderer::AtomicSetup(RpAtomic* atomic) {
    const auto geom = RpAtomicGetGeometry(atomic);
    if (CCustomBuildingDNPipeline::GetExtraVertColourPtr(geom) && geom->preLitLum) {
        CCustomBuildingDNPipeline::CustomPipeAtomicSetup(atomic);
    } else {
        CCustomBuildingPipeline::CustomPipeAtomicSetup(atomic);
    }
}

// 0x5D7F40
bool CCustomBuildingRenderer::IsCBPCPipelineAttached(RpAtomic* atomic) {
    const auto id = GetPipelineID(atomic);
    if (id == rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C) || id == CUSTOM_BUILDING_DN_PIPELINE_ID) {
        return true;
    }
    const auto geom = RpAtomicGetGeometry(atomic);
    if (CCustomBuildingDNPipeline::GetExtraVertColourPtr(geom) && geom->preLitLum) {
        return true;
    }
    return false;
}

// 0x5D7F80
void CCustomBuildingRenderer::UpdateDayNightBalanceParam() {
    const auto minutes = (float)(CClock::GetGameClockHours() * 60 + CClock::GetGameClockMinutes()) + (float)CClock::GetGameClockSeconds() / 60.0f;

    constexpr auto six_hours = 360.0f;
    if (minutes < six_hours) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
        return;
    }

    constexpr auto seven_hours = 420.0f;
    if (minutes < seven_hours) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = (seven_hours - minutes) / 60.0f;
        return;
    }

    constexpr auto twenty_hours = 1200.0f;
    if (minutes < twenty_hours) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.0f;
        return;
    }

    constexpr auto twenty_one_hour = 1260.0f;
    if (minutes < twenty_one_hour) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f - (twenty_one_hour - minutes) / 60.0f;
        return;
    }

    CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
}

// 0x5D8050
void CCustomBuildingRenderer::Update() {
    ZoneScoped;

    UpdateDayNightBalanceParam();

    auto& s_Magic1 = StaticRef<uint32>(0xC02C14);
    auto& s_Magic2 = StaticRef<uint32>(0xC02C18);
    s_Magic1 = (s_Magic1 + 1) & 0xF;
    s_Magic2 = 0;
}
