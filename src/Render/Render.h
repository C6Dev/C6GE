/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include "SampleBase.hpp"
#include "FirstPersonCamera.hpp"
#include "DiligentTools/AssetLoader/interface/DXSDKMeshLoader.hpp"
#include "AdvancedMath.hpp"
#include "BasicMath.hpp"
#include "ThreadSignal.hpp"
#include "DiligentTools/ThirdParty/imgui/imgui.h"

namespace Diligent
{

    class C6GERender final : public SampleBase
    {
        // Play/pause icon textures
        RefCntAutoPtr<ITextureView> m_PlayIconSRV;
        RefCntAutoPtr<ITextureView> m_PauseIconSRV;

    public:
        // Play/pause state for editor runtime
        enum class PlayState
        {
            Paused,
            Playing
        };
        static PlayState playState;

        static void TogglePlayState();
        static bool IsPlaying() { return playState == PlayState::Playing; }
        static bool IsPaused() { return playState == PlayState::Paused; }
        virtual void Initialize(const SampleInitInfo &InitInfo) override final;

        void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs &Attribs) override final;

        virtual ~C6GERender();

    void UpdateUI() override;

        void UpdateViewportUI();

        void DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT *VertexElement,
                                                       Uint32 Stride,
                                                       InputLayoutDesc &Layout,
                                                       std::vector<LayoutElement> &Elements);

        void CreateFramebuffer();

        void ResizeFramebuffer(Uint32 Width, Uint32 Height);

        void WindowResize(Uint32 Width, Uint32 Height) override final;

        virtual void Render() override final;
        virtual void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

        virtual const Char *GetSampleName() const override final { return "C6GERender"; }

        // Get framebuffer texture for ImGui viewport
        ITextureView *GetFramebufferSRV() const { return m_pFramebufferSRV; }
        Uint32 GetFramebufferWidth() const { return m_FramebufferWidth; }
        Uint32 GetFramebufferHeight() const { return m_FramebufferHeight; }

        static bool IsRuntime;

    private:
    void CreateCubePSO();
    void CreatePlanePSO();
    // Shadow map visualization creation removed
    void CreateShadowMap();
    void RenderShadowMap();
    void RenderCube(const float4x4& CameraViewProj, bool IsShadowPass);
    void RenderPlane();
    // Shadow map visualization removed

    RefCntAutoPtr<IPipelineState>         m_pCubePSO;
    RefCntAutoPtr<IPipelineState>         m_pCubeShadowPSO;
    RefCntAutoPtr<IPipelineState>         m_pPlanePSO;
    RefCntAutoPtr<IPipelineState>         m_pPlaneNoShadowPSO;
    // Shadow map visualization PSO removed
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    float                                  m_LineWidth = 1.0f;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_CubeShadowSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_PlaneSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_PlaneNoShadowSRB;
    // Visualization SRB removed
    RefCntAutoPtr<ITextureView>           m_ShadowMapDSV;
    RefCntAutoPtr<ITextureView>           m_ShadowMapSRV;
    FirstPersonCamera m_Camera;

    float4x4       m_CubeWorldMatrix;
    float4x4       m_CameraViewProjMatrix;
    float4x4       m_WorldToShadowMapUVDepthMatr;
    float3         m_LightDirection  = normalize(float3(-0.49f, -0.60f, 0.64f));
    Uint32         m_ShadowMapSize   = 2048;
    TEXTURE_FORMAT m_ShadowMapFormat = TEX_FORMAT_D16_UNORM;
    // Shadow visualization removed. Use RenderShadows to enable/disable shadowing.

        // Framebuffer for off-screen rendering
        RefCntAutoPtr<ITexture> m_pFramebufferTexture;
        RefCntAutoPtr<ITextureView> m_pFramebufferRTV;
        RefCntAutoPtr<ITextureView> m_pFramebufferSRV;
        RefCntAutoPtr<ITexture> m_pFramebufferDepth;
        RefCntAutoPtr<ITextureView> m_pFramebufferDSV;
        Uint32 m_FramebufferWidth = 800;
        Uint32 m_FramebufferHeight = 600;
        ImTextureID m_ViewportTextureID = 0;
    };

} // namespace Diligent