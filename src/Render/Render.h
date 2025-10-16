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

#include "SampleBase.hpp"
#include "FirstPersonCamera.hpp"
#include "DiligentTools/AssetLoader/interface/DXSDKMeshLoader.hpp"
#include "AdvancedMath.hpp"

namespace Diligent
{

class C6GERender final : public SampleBase
{
    // Play/pause icon textures
    RefCntAutoPtr<ITextureView> m_PlayIconSRV;
    RefCntAutoPtr<ITextureView> m_PauseIconSRV;
public:
    // Play/pause state for editor runtime
    enum class PlayState { Paused, Playing };
    static PlayState playState;

    static void TogglePlayState();
    static bool IsPlaying() { return playState == PlayState::Playing; }
    static bool IsPaused() { return playState == PlayState::Paused; }
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    virtual ~C6GERender();

    void UpdateUI();

    void UpdateViewportUI();

    void DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT* VertexElement,
                                               Uint32 Stride,
                                               InputLayoutDesc& Layout,
                                               std::vector<LayoutElement>& Elements);

    void CreatePipelineStates();

    void CreateInstanceBuffer();

    void LoadTextures();

    void PopulateInstanceBuffer();

    void CreateFramebuffer();

    void ResizeFramebuffer(Uint32 Width, Uint32 Height);

    void WindowResize(Uint32 Width, Uint32 Height) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

    virtual const Char* GetSampleName() const override final { return "C6GERender"; }

    // Get framebuffer texture for ImGui viewport
    ITextureView* GetFramebufferSRV() const { return m_pFramebufferSRV; }
    Uint32 GetFramebufferWidth() const { return m_FramebufferWidth; }
    Uint32 GetFramebufferHeight() const { return m_FramebufferHeight; }

    static bool IsRuntime;

private:
    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<IBuffer> m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_InstanceBuffer;
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    float4x4 m_WorldViewProjMatrix = float4x4::Identity();
    FirstPersonCamera m_Camera;
    RefCntAutoPtr<IBuffer> m_CubeIndexBuffer;

    // Framebuffer for off-screen rendering
    RefCntAutoPtr<ITexture>     m_pFramebufferTexture;
    RefCntAutoPtr<ITextureView> m_pFramebufferRTV;
    RefCntAutoPtr<ITextureView> m_pFramebufferSRV;
    RefCntAutoPtr<ITexture>     m_pFramebufferDepth;
    RefCntAutoPtr<ITextureView> m_pFramebufferDSV;
    Uint32                      m_FramebufferWidth = 800;
    Uint32                      m_FramebufferHeight = 600;

    float4x4             m_ViewProjMatrix;
    float4x4             m_RotationMatrix;
    int                  m_GridSize   = 5;
    static constexpr int MaxGridSize  = 32;
    static constexpr int MaxInstances = MaxGridSize * MaxGridSize * MaxGridSize;
    static constexpr int NumTextures  = 4;
};

} // namespace Diligent