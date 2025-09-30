#include "Render/Features/Shadows/Shadows.h"
#include "Render/Render.h"
#include "AdvancedMath.hpp"
#include "GraphicsAccessories.hpp"
#include "MapHelper.hpp"

namespace Diligent
{

void ShadowsFeature::InitializeResourceBindings()
{
    auto& self = *m_Owner;
    self.m_SRBs.clear();
    self.m_ShadowSRBs.clear();
    self.m_SRBs.resize(self.m_Mesh.GetNumMaterials());
    self.m_ShadowSRBs.resize(self.m_Mesh.GetNumMaterials());
    for (Uint32 mat = 0; mat < self.m_Mesh.GetNumMaterials(); ++mat)
    {
        {
            const DXSDKMESH_MATERIAL& Mat = self.m_Mesh.GetMaterial(mat);

            RefCntAutoPtr<IShaderResourceBinding> pSRB;
            self.m_RenderMeshPSO[0]->CreateShaderResourceBinding(&pSRB, true);
            VERIFY(Mat.pDiffuseRV != nullptr, "Material must have diffuse color texture");
            pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DDiffuse")->Set(Mat.pDiffuseRV);

            if (self.m_ShadowMapMgr.GetSRV() != nullptr)
            {
                if (self.m_ShadowSettings.iShadowMode == SHADOW_MODE_PCF)
                {
                    pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DShadowMap")->Set(self.m_ShadowMapMgr.GetSRV());
                }
                else
                {
                    pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DFilterableShadowMap")->Set(self.m_ShadowMapMgr.GetFilterableSRV());
                }
            }
            else
            {
                // Bind dummy shadow resources when shadows are disabled
                if (self.m_ShadowSettings.iShadowMode == SHADOW_MODE_PCF)
                {
                    if (!self.m_pDummyShadowTex)
                    {
                        TextureDesc TexDesc;
                        TexDesc.Name         = "Dummy shadow depth array";
                        TexDesc.Type         = RESOURCE_DIM_TEX_2D_ARRAY;
                        TexDesc.Width        = 1;
                        TexDesc.Height       = 1;
                        TexDesc.ArraySize    = 1;
                        TexDesc.MipLevels    = 1;
                        TexDesc.Format       = self.m_ShadowSettings.Format;
                        TexDesc.Usage        = USAGE_DEFAULT;
                        TexDesc.BindFlags    = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
                        TexDesc.SampleCount  = 1;
                        TexDesc.ClearValue.Format             = TexDesc.Format;
                        TexDesc.ClearValue.DepthStencil.Depth = 1.0f;
                        self.m_pDevice->CreateTexture(TexDesc, nullptr, &self.m_pDummyShadowTex);
                        self.m_pDummyShadowMapSRV = self.m_pDummyShadowTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                        if (!self.m_pComparisonSampler)
                        {
                            SamplerDesc ComparsionSampler;
                            ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS;
                            ComparsionSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
                            ComparsionSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
                            ComparsionSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;
                            self.m_pDevice->CreateSampler(ComparsionSampler, &self.m_pComparisonSampler);
                        }
                        self.m_pDummyShadowMapSRV->SetSampler(self.m_pComparisonSampler);
                    }
                    pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DShadowMap")->Set(self.m_pDummyShadowMapSRV);
                }
                else
                {
                    if (!self.m_pDummyFilterableShadowTex)
                    {
                        TextureDesc TexDesc;
                        TexDesc.Name      = "Dummy filterable shadow array";
                        TexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
                        TexDesc.Width     = 1;
                        TexDesc.Height    = 1;
                        TexDesc.ArraySize = 1;
                        TexDesc.MipLevels = 1;
                        TexDesc.Format    = self.m_ShadowSettings.Is32BitFilterableFmt ? TEX_FORMAT_RGBA32_FLOAT : TEX_FORMAT_RGBA8_UNORM;
                        TexDesc.Usage     = USAGE_DEFAULT;
                        TexDesc.BindFlags = BIND_SHADER_RESOURCE;
                        TexDesc.SampleCount = 1;
                        self.m_pDevice->CreateTexture(TexDesc, nullptr, &self.m_pDummyFilterableShadowTex);
                        self.m_pDummyFilterableShadowMapSRV = self.m_pDummyFilterableShadowTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                        if (!self.m_pFilterableShadowMapSampler)
                        {
                            SamplerDesc SD;
                            SD.MinFilter     = FILTER_TYPE_ANISOTROPIC;
                            SD.MagFilter     = FILTER_TYPE_ANISOTROPIC;
                            SD.MipFilter     = FILTER_TYPE_ANISOTROPIC;
                            SD.MaxAnisotropy = self.m_LightAttribs.ShadowAttribs.iMaxAnisotropy;
                            self.m_pDevice->CreateSampler(SD, &self.m_pFilterableShadowMapSampler);
                        }
                        self.m_pDummyFilterableShadowMapSRV->SetSampler(self.m_pFilterableShadowMapSampler);
                    }
                    pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DFilterableShadowMap")->Set(self.m_pDummyFilterableShadowMapSRV);
                }
            }
            self.m_SRBs[mat] = std::move(pSRB);
        }

        {
            RefCntAutoPtr<IShaderResourceBinding> pShadowSRB;
            self.m_RenderMeshShadowPSO[0]->CreateShaderResourceBinding(&pShadowSRB, true);
            self.m_ShadowSRBs[mat] = std::move(pShadowSRB);
        }
    }
}

void ShadowsFeature::InitShadows()
{
    auto& self = *m_Owner;


    if (self.m_ShadowSettings.Resolution >= 2048)
        self.m_LightAttribs.ShadowAttribs.fFixedDepthBias = 0.0025f;
    else if (self.m_ShadowSettings.Resolution >= 1024)
        self.m_LightAttribs.ShadowAttribs.fFixedDepthBias = 0.005f;
    else
        self.m_LightAttribs.ShadowAttribs.fFixedDepthBias = 0.0075f;

    ShadowMapManager::InitInfo SMMgrInitInfo;
    SMMgrInitInfo.Format               = self.m_ShadowSettings.Format;
    SMMgrInitInfo.Resolution           = self.m_ShadowSettings.Resolution;
    SMMgrInitInfo.NumCascades          = static_cast<Uint32>(self.m_LightAttribs.ShadowAttribs.iNumCascades);
    SMMgrInitInfo.ShadowMode           = self.m_ShadowSettings.iShadowMode;
    SMMgrInitInfo.Is32BitFilterableFmt = self.m_ShadowSettings.Is32BitFilterableFmt;

    if (!self.m_pComparisonSampler)
    {
        SamplerDesc ComparsionSampler;
        ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS;
        ComparsionSampler.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;
        self.m_pDevice->CreateSampler(ComparsionSampler, &self.m_pComparisonSampler);
    }
    SMMgrInitInfo.pComparisonSampler = self.m_pComparisonSampler;

    if (!self.m_pFilterableShadowMapSampler)
    {
        SamplerDesc SD;
        SD.MinFilter     = FILTER_TYPE_ANISOTROPIC;
        SD.MagFilter     = FILTER_TYPE_ANISOTROPIC;
        SD.MipFilter     = FILTER_TYPE_ANISOTROPIC;
        SD.MaxAnisotropy = self.m_LightAttribs.ShadowAttribs.iMaxAnisotropy;
        self.m_pDevice->CreateSampler(SD, &self.m_pFilterableShadowMapSampler);
    }
    SMMgrInitInfo.pFilterableShadowMapSampler = self.m_pFilterableShadowMapSampler;

    self.m_ShadowMapMgr.Initialize(self.m_pDevice, nullptr, SMMgrInitInfo);

    InitializeResourceBindings();
}

void ShadowsFeature::RenderShadows()
{
    auto& self = *m_Owner;
    if (self.m_ShadowMapMgr.GetSRV() == nullptr)
        return;

    int iNumShadowCascades = self.m_LightAttribs.ShadowAttribs.iNumCascades;
    for (int iCascade = 0; iCascade < iNumShadowCascades; ++iCascade)
    {
        const float4x4& CascadeProjMatr = self.m_ShadowMapMgr.GetCascadeTransform(iCascade).Proj;

        const float4x4& WorldToLightViewSpaceMatr = self.m_PackMatrixRowMajor ?
            self.m_LightAttribs.ShadowAttribs.mWorldToLightView :
            self.m_LightAttribs.ShadowAttribs.mWorldToLightView.Transpose();

        const float4x4 WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * CascadeProjMatr;

        CameraAttribs ShadowCameraAttribs = {};
        WriteShaderMatrix(&ShadowCameraAttribs.mViewProj, WorldToLightProjSpaceMatr, !self.m_PackMatrixRowMajor);
        WriteShaderMatrix(&ShadowCameraAttribs.mProj, CascadeProjMatr, !self.m_PackMatrixRowMajor);
        {
            MapHelper<CameraAttribs> ShadowCamAttribs(self.m_pImmediateContext, self.m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            *ShadowCamAttribs = ShadowCameraAttribs;
        }

        ITextureView* pCascadeDSV = self.m_ShadowMapMgr.GetCascadeDSV(iCascade);
        self.m_pImmediateContext->SetRenderTargets(0, nullptr, pCascadeDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        self.m_pImmediateContext->ClearDepthStencil(pCascadeDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        ViewFrustumExt ShadowFrustum;
        ExtractViewFrustumPlanesFromMatrix(WorldToLightProjSpaceMatr, ShadowFrustum, self.m_pDevice->GetDeviceInfo().IsGLDevice());
        self.DrawMesh(self.m_pImmediateContext, true, ShadowFrustum);
    }

    if (self.m_ShadowSettings.iShadowMode > SHADOW_MODE_PCF)
        self.m_ShadowMapMgr.ConvertToFilterable(self.m_pImmediateContext, self.m_LightAttribs.ShadowAttribs);
}

void ShadowsFeature::UpdateShadows()
{
    auto& self = *m_Owner;
    ShadowMapManager::DistributeCascadeInfo DistrInfo;
    DistrInfo.pCameraView   = &self.m_Camera.GetViewMatrix();
    DistrInfo.pCameraProj   = &self.m_Camera.GetProjMatrix();
    float3 f3LightDirection = float3(self.m_LightAttribs.f4Direction.x, self.m_LightAttribs.f4Direction.y, self.m_LightAttribs.f4Direction.z);
    DistrInfo.pLightDir     = &f3LightDirection;

    DistrInfo.fPartitioningFactor = self.m_ShadowSettings.PartitioningFactor;
    DistrInfo.SnapCascades        = self.m_ShadowSettings.SnapCascades;
    DistrInfo.EqualizeExtents     = self.m_ShadowSettings.EqualizeExtents;
    DistrInfo.StabilizeExtents    = self.m_ShadowSettings.StabilizeExtents;
    DistrInfo.PackMatrixRowMajor  = self.m_PackMatrixRowMajor;

    if (self.m_ShadowMapMgr.GetSRV() != nullptr)
    {
        self.m_ShadowMapMgr.DistributeCascades(DistrInfo, self.m_LightAttribs.ShadowAttribs);
    }
}

} // namespace Diligent