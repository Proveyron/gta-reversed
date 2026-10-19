#include "StdInc.h"

#include "CarFXRenderer.h"
#include "CustomCarEnvMapPipeline.h"
#include "TxdStore.h"
#include "ClothesBuilder.h"

void CCarFXRenderer::InjectHooks() {
    RH_ScopedClass(CCarFXRenderer);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(RegisterPlugins, 0x5D5B00);
    RH_ScopedInstall(Initialise, 0x5D5AC0);
    RH_ScopedInstall(InitialiseDirtTexture, 0x5D5BC0);
    RH_ScopedInstall(Shutdown, 0x5D5AD0);
    RH_ScopedInstall(PreRenderUpdate, 0x5D5B10);
    RH_ScopedInstall(IsCCPCPipelineAttached, 0x5D5B80);
    RH_ScopedInstall(CustomCarPipeAtomicSetup, 0x5D5B20);
    RH_ScopedInstall(CustomCarPipeClumpSetup, 0x5D5B40);
    RH_ScopedInstall(SetCustomFXAtomicRenderPipelinesVMICB, 0x5D5B60);
    RH_ScopedInstall(SetFxEnvMapLightMult, 0x5D5BA0);
}

// 0x5D5B00
bool CCarFXRenderer::RegisterPlugins() {
    return CCustomCarEnvMapPipeline::RegisterPlugin() != 0;
}

// 0x5D5AC0
bool CCarFXRenderer::Initialise() {
    return CCustomCarEnvMapPipeline::CreatePipe() != 0;
}

// 0x5D5BC0
void CCarFXRenderer::InitialiseDirtTexture() {
    const auto txd = CTxdStore::FindTxdSlot("vehicle");
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(txd);

    auto* dict = CTxdStore::GetTxd(txd);
    auto* grungeTex = RwTexDictionaryFindNamedTexture(dict, "vehiclegrunge256");
    if (!grungeTex) {
        CTxdStore::PopCurrentTxd();
        return;
    }

    auto* grungeRaster = RwTextureGetRaster(grungeTex);
    const auto width = RwRasterGetWidth(grungeRaster);
    const auto height = RwRasterGetHeight(grungeRaster);

    int32 ebx = 0xFF0;
    for (auto dirtLevel = 0; dirtLevel < NUM_DIRT_TEXTURES; dirtLevel++) {
        auto* tex = CClothesBuilder::CopyTexture(grungeTex);
        ms_aDirtTextures[dirtLevel] = tex;
        RwTextureSetName(tex, "vehiclegrunge256");

        auto* raster = RwTextureGetRaster(tex);
        auto* pixels = RwRasterLock(raster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKREAD);
        if (pixels) {
            const auto factor = static_cast<uint8>(ebx >> 4);
            const auto stride = RwRasterGetStride(raster);

            for (auto y = 0; y < height; y++) {
                auto* row = reinterpret_cast<uint8*>(pixels) + y * stride;
                for (auto x = 0; x < width; x++) {
                    auto* pixel = &row[x * 4];
                    pixel[0] = static_cast<uint8>((pixel[0] * dirtLevel) / 16 + factor);
                    pixel[1] = static_cast<uint8>((pixel[1] * dirtLevel) / 16 + factor);
                    pixel[2] = static_cast<uint8>((pixel[2] * dirtLevel) / 16 + factor);
                }
            }
            RwRasterUnlock(raster);
        }

        ebx -= 0xFF;
    }

    CTxdStore::PopCurrentTxd();
}

// 0x5D5AD0
void CCarFXRenderer::Shutdown() {
    for (auto& texture : ms_aDirtTextures) {
        RwTextureDestroy(texture);
    }
    CCustomCarEnvMapPipeline::DestroyPipe();
}

// 0x5D5B10
void CCarFXRenderer::PreRenderUpdate() {
    CCustomCarEnvMapPipeline::PreRenderUpdate();
}

// 0x5D5B80
bool CCarFXRenderer::IsCCPCPipelineAttached(RpAtomic* atomic) {
    return GetPipelineID(atomic) == 0x53F2009A;
}

// 0x5D5B20
void CCarFXRenderer::CustomCarPipeAtomicSetup(RpAtomic* atomic) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
}

// 0x5D5B40
void CCarFXRenderer::CustomCarPipeClumpSetup(RpClump* clump) {
    RpClumpForAllAtomics(clump, SetCustomFXAtomicRenderPipelinesVMICB, nullptr);
}

// 0x5D5B60
RpAtomic* CCarFXRenderer::SetCustomFXAtomicRenderPipelinesVMICB(RpAtomic* atomic, void* data) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
    return atomic;
}

// 0x5D5BA0
void CCarFXRenderer::SetFxEnvMapLightMult(float multiplier) {
    gSpecIntensity = multiplier;
}
