#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
    #ifndef GLFW_EXPOSE_NATIVE_WIN32
        #define GLFW_EXPOSE_NATIVE_WIN32
    #endif
    #include <GLFW/glfw3native.h>
    #include "DiligentCore/Platforms/Win32/interface/Win32NativeWindow.h"
    #include <Windows.h>
    #include "main.h"
    #include "DiligentSamples/SampleBase/include/Win32/InputControllerWin32.hpp"

#elif defined(__APPLE__)
    #ifndef GLFW_EXPOSE_NATIVE_COCOA
        #define GLFW_EXPOSE_NATIVE_COCOA
    #endif
    #include <GLFW/glfw3native.h>
    #include "DiligentCore/Platforms/Apple/interface/MacOSNativeWindow.h"
    #include <Cocoa/Cocoa.h>
    #include <QuartzCore/CAMetalLayer.h>
    #include "main.h"
    #include "DiligentSamples/SampleBase/include/MacOS/InputControllerMacOS.hpp"

#elif defined(__linux__)
    #ifndef GLFW_EXPOSE_NATIVE_X11
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
    #include <GLFW/glfw3native.h>
    #include "DiligentCore/Platforms/Linux/interface/LinuxNativeWindow.h"
#endif

// Diligent common interfaces
#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h"

// Engine factories
#include "DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "DiligentCore/Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "DiligentCore/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#ifdef __APPLE__
#include "DiligentCore/Graphics/GraphicsEngineMetal/interface/EngineFactoryMtl.h"
#endif

// Your includes
#include "main.h"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "backends/imgui_impl_glfw.h"
#include "ImGuiImplDiligent.hpp"
#include "DiligentTools/ThirdParty/imGuIZMO.quat/imGuIZMO.h"
#include "Align.hpp"
#include "FirstPersonCamera.hpp"
#include "FastRand.hpp"
#include "Bloom.hpp"
#include "SuperResolution.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "PostFXContext.hpp"
#include "ScopedDebugGroup.hpp"
#include "TemporalAntiAliasing.hpp"
#include "ScreenSpaceReflection.hpp"
#include "ScreenSpaceAmbientOcclusion.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "CommonlyUsedStates.h"
#include "RenderStateCache.hpp"
#include "GraphicsTypesX.hpp"
#include "GBuffer.hpp"
#include "EnvMapRenderer.hpp"


using namespace Diligent;

// Windows-only GLFW callbacks that forward events into Diligent InputControllerWin32
#if defined(_WIN32)
static UINT MapGLFWKeyToVK(int key)
{
    switch (key)
    {
        case GLFW_KEY_LEFT: return VK_LEFT;
        case GLFW_KEY_RIGHT: return VK_RIGHT;
        case GLFW_KEY_UP: return VK_UP;
        case GLFW_KEY_DOWN: return VK_DOWN;
        case GLFW_KEY_A: return 'A';
        case GLFW_KEY_D: return 'D';
        case GLFW_KEY_W: return 'W';
        case GLFW_KEY_S: return 'S';
        case GLFW_KEY_Q: return 'Q';
        case GLFW_KEY_E: return 'E';
        case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return VK_SHIFT;
        case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: return VK_CONTROL;
        case GLFW_KEY_LEFT_ALT: case GLFW_KEY_RIGHT_ALT: return VK_MENU;
        case GLFW_KEY_HOME: return VK_HOME;
        case GLFW_KEY_KP_ADD: case GLFW_KEY_EQUAL: return VK_ADD;
        case GLFW_KEY_KP_SUBTRACT: case GLFW_KEY_MINUS: return VK_SUBTRACT;
        default:
            return 0;
    }
}

static void GLFWKeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    // Forward to ImGui backend first
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);

    // If ImGui wants keyboard, don't forward to the app
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    UINT vk = MapGLFWKeyToVK(key);
    if (vk == 0)
        return;

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    MsgData.wParam = (WPARAM)vk;
    MsgData.lParam = 0;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        MsgData.message = WM_KEYDOWN;
    else if (action == GLFW_RELEASE)
        MsgData.message = WM_KEYUP;
    else
        return;

    // Cast controller to Win32 implementation and call HandleNativeMessage
    auto& winCtrl = reinterpret_cast<Diligent::InputControllerWin32&>(samplePtr->GetInputController());
    winCtrl.HandleNativeMessage(&MsgData);
}

static void GLFWMouseButtonCallback(GLFWwindow* w, int button, int action, int mods)
{
    // Forward to ImGui backend first
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    double xd, yd;
    glfwGetCursorPos(w, &xd, &yd);
    int x = static_cast<int>(xd);
    int y = static_cast<int>(yd);

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    MsgData.lParam = (LPARAM)((y << 16) | (x & 0xFFFF));

    // If ImGui didn't capture, forward the event to app's input controller.
    // Trick: map RIGHT button to LEFT for camera rotation behavior.
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        MsgData.message = (action == GLFW_PRESS) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        MsgData.message = (action == GLFW_PRESS) ? WM_MBUTTONDOWN : WM_MBUTTONUP;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        MsgData.message = (action == GLFW_PRESS) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    else
        return;

    auto& winCtrl2 = reinterpret_cast<Diligent::InputControllerWin32&>(samplePtr->GetInputController());
    winCtrl2.HandleNativeMessage(&MsgData);
}

static void GLFWCursorPosCallback(GLFWwindow* w, double xpos, double ypos)
{
    // Forward to ImGui
    ImGui_ImplGlfw_CursorPosCallback(w, xpos, ypos);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;
    // Update cached mouse position inside controller (Win32 implementation reads real cursor)
    samplePtr->GetInputController().GetMouseState();
}

static void GLFWScrollCallback(GLFWwindow* w, double xoffset, double yoffset)
{
    // Forward to ImGui
    ImGui_ImplGlfw_ScrollCallback(w, xoffset, yoffset);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    struct WindowMessageData { HWND hWnd; UINT message; WPARAM wParam; LPARAM lParam; } MsgData{};
    MsgData.hWnd = glfwGetWin32Window(w);
    short delta = static_cast<short>(yoffset * WHEEL_DELTA);
    MsgData.wParam = (WPARAM)((UINT)delta << 16);
    MsgData.message = WM_MOUSEWHEEL;
    MsgData.lParam = 0;
    auto& winCtrl3 = reinterpret_cast<Diligent::InputControllerWin32&>(samplePtr->GetInputController());
    winCtrl3.HandleNativeMessage(&MsgData);
}
#endif

#if defined(__APPLE__)
// macOS-specific GLFW callbacks that forward events into Diligent InputControllerMacOS
static void GLFWKeyCallbackMac(GLFWwindow* w, int key, int scancode, int action, int mods)
{
    // Forward to ImGui backend first
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);

    // If ImGui wants keyboard, don't forward to the app
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
        macCtrl.OnKeyPressed(key);
    else if (action == GLFW_RELEASE)
        macCtrl.OnKeyReleased(key);

    // Forward modifier state as flags change
    macCtrl.OnFlagsChanged((mods & GLFW_MOD_SHIFT) != 0,
                           (mods & GLFW_MOD_CONTROL) != 0,
                           (mods & GLFW_MOD_ALT) != 0);
}

static void GLFWMouseButtonCallbackMac(GLFWwindow* w, int button, int action, int mods)
{
    // Forward to ImGui backend first
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::LMB_Pressed);
        else if (action == GLFW_RELEASE)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::LMB_Released);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::RMB_Pressed);
        else if (action == GLFW_RELEASE)
            macCtrl.OnMouseButtonEvent(Diligent::InputControllerMacOS::MouseButtonEvent::RMB_Released);
    }
}

static void GLFWCursorPosCallbackMac(GLFWwindow* w, double xpos, double ypos)
{
    // Forward to ImGui
    ImGui_ImplGlfw_CursorPosCallback(w, xpos, ypos);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());
    macCtrl.OnMouseMove(static_cast<int>(xpos), static_cast<int>(ypos));
}

static void GLFWScrollCallbackMac(GLFWwindow* w, double xoffset, double yoffset)
{
    // Forward to ImGui
    ImGui_ImplGlfw_ScrollCallback(w, xoffset, yoffset);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    auto* samplePtr = static_cast<SampleBase*>(glfwGetWindowUserPointer(w));
    if (!samplePtr)
        return;

    auto& macCtrl = reinterpret_cast<Diligent::InputControllerMacOS&>(samplePtr->GetInputController());
    macCtrl.OnMouseWheel(static_cast<float>(yoffset));
}
#endif

bool enableVsync = false; // toggle this from your editor later
bool enableRayTracing = true; // toggle ray tracing on/off (will run fallback when disabled)

namespace Diligent
{

static RefCntAutoPtr<IShader> CreateShader(IRenderDevice*          pDevice,
                                           IRenderStateCache*      pStateCache,
                                           const Char*             FileName,
                                           const Char*             EntryPoint,
                                           SHADER_TYPE             Type,
                                           const ShaderMacroArray& Macros = {})
{

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    // Create a compound shader source factory that will be able to load DiligentFX shaders.
    auto pCompoundShaderSourceFactory = CreateCompoundShaderSourceFactory({&DiligentFXShaderSourceStreamFactory::GetInstance(), pShaderSourceFactory});

    ShaderCreateInfo ShaderCI;
    ShaderCI.EntryPoint                      = EntryPoint;
    ShaderCI.FilePath                        = FileName;
    ShaderCI.Macros                          = Macros;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.ShaderType                 = Type;
    ShaderCI.Desc.Name                       = EntryPoint;
    ShaderCI.pShaderSourceStreamFactory      = pCompoundShaderSourceFactory;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
    return RenderDeviceWithCache<false>{pDevice, pStateCache}.CreateShader(ShaderCI);
}

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
#include "Shaders/PostProcess/TemporalAntiAliasing/public/TemporalAntiAliasingStructures.fxh"
#include "Shaders/PostProcess/ScreenSpaceReflection/public/ScreenSpaceReflectionStructures.fxh"
#include "Shaders/PostProcess/ScreenSpaceAmbientOcclusion/public/ScreenSpaceAmbientOcclusionStructures.fxh"
#include "Shaders/PostProcess/Bloom/public/BloomStructures.fxh"
#include "Shaders/PostProcess/SuperResolution/public/SuperResolutionStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
    #include "Assets/shaders/GeometryStructures.fxh"

} // namespace HLSL

enum GBUFFER_RT : Uint32
{
    GBUFFER_RT_BASE_COLOR,
    GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_NORMAL,
    GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_COUNT
};

enum GBUFFER_RT_FLAG : Uint32
{
    GBUFFER_RT_FLAG_BASE_COLOR     = 1u << GBUFFER_RT_BASE_COLOR,
    GBUFFER_RT_FLAG_MATERIAL_DATA  = 1u << GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_FLAG_NORMAL         = 1u << GBUFFER_RT_NORMAL,
    GBUFFER_RT_FLAG_MOTION_VECTORS = 1u << GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_FLAG_LAST           = GBUFFER_RT_FLAG_MOTION_VECTORS,
    GBUFFER_RT_FLAG_ALL            = (GBUFFER_RT_FLAG_LAST << 1u) - 1u,
};
DEFINE_FLAG_ENUM_OPERATORS(GBUFFER_RT_FLAG);


enum UPSAMPLING_MODE : Int32
{
    UPSAMPLING_MODE_BILINEAR = 0,
    UPSAMPLING_MODE_FSR
};

SampleBase* CreateSample()
{
    return new Tutorial27_PostProcessing();
}

struct Tutorial27_PostProcessing::ShaderSettings
{
    HLSL::PBRRendererShaderParameters        PBRRenderParams = {};
    HLSL::ScreenSpaceReflectionAttribs       SSRSettings     = {};
    HLSL::ScreenSpaceAmbientOcclusionAttribs SSAOSettings    = {};
    HLSL::TemporalAntiAliasingAttribs        TAASettings     = {};
    HLSL::BloomAttribs                       BloomSettings   = {};
    HLSL::SuperResolutionAttribs             FSRSettings     = {};

    bool  TAAEnabled   = true;
    bool  BloomEnabled = true;
    float SSAOStrength = 1.0;
    float SSRStrength  = 1.0;

    UPSAMPLING_MODE                            UpsamplingMode       = UPSAMPLING_MODE_FSR;
    PostFXContext::FEATURE_FLAGS               PostFXFeatureFlags   = PostFXContext::FEATURE_FLAG_NONE;
    ScreenSpaceAmbientOcclusion::FEATURE_FLAGS SSAOFeatureFlags     = ScreenSpaceAmbientOcclusion::FEATURE_FLAG_NONE;
    ScreenSpaceReflection::FEATURE_FLAGS       SSRFeatureFlags      = ScreenSpaceReflection::FEATURE_FLAG_PREVIOUS_FRAME;
    TemporalAntiAliasing::FEATURE_FLAGS        TAAFeatureFlags      = TemporalAntiAliasing::FEATURE_FLAG_BICUBIC_FILTER;
    Bloom::FEATURE_FLAGS                       BloomFeatureFlags    = Bloom::FEATURE_FLAG_NONE;
    SuperResolution::FEATURE_FLAGS             SuperResolutionFlags = SuperResolution::FEATURE_FLAG_NONE;
};

Tutorial27_PostProcessing::Tutorial27_PostProcessing() :
    m_Resources{RESOURCE_IDENTIFIER_COUNT},
    m_CameraAttribs{std::make_unique<HLSL::CameraAttribs[]>(2)},
    m_ObjectAttribs{std::make_unique<HLSL::ObjectAttribs[]>(m_MaxObjectCount)},
    m_MaterialAttribs{std::make_unique<HLSL::MaterialAttribs[]>(m_MaxMaterialCount)}
{
    m_Camera.SetMoveSpeed(4.0f);
    m_Camera.SetPos(float3{-8.75f, 1.25f, 6.5f});
    m_Camera.SetReferenceAxes(float3{1.0f, 0.0f, 0.0f}, float3{0.0f, 1.0f, 0.0f}, false);
    m_Camera.SetLookAt(float3{1.0f, 0.0f, 1.0f});

    m_ObjectTransforms[0].resize(m_MaxObjectCount, float4x4::Identity());
    m_ObjectTransforms[1].resize(m_MaxObjectCount, float4x4::Identity());
}

void Tutorial27_PostProcessing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Initialize G-Buffer
    {
        GBuffer::ElementDesc GBufferElems[GBUFFER_RT_COUNT]{};
        GBufferElems[GBUFFER_RT_BASE_COLOR]     = {TEX_FORMAT_RGBA8_UNORM};
        GBufferElems[GBUFFER_RT_MATERIAL_DATA]  = {TEX_FORMAT_RG8_UNORM};
        GBufferElems[GBUFFER_RT_NORMAL]         = {TEX_FORMAT_RGBA16_FLOAT};
        GBufferElems[GBUFFER_RT_MOTION_VECTORS] = {TEX_FORMAT_RG16_FLOAT};
        static_assert(GBUFFER_RT_COUNT == 4, "Not all G-buffer elements are initialized");
        m_GBuffer = std::make_unique<GBuffer>(GBufferElems, _countof(GBufferElems));
    }

    // Create necessary constant buffers for rendering
    {
        RefCntAutoPtr<IBuffer> pFrameAttribsCB;
        CreateUniformBuffer(m_pDevice, 2 * sizeof(HLSL::CameraAttribs), "Tutorial27_PostProcessing::CameraConstantBuffer", &pFrameAttribsCB);
        m_Resources.Insert(RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER, pFrameAttribsCB);

        RefCntAutoPtr<IBuffer> pPBRRenderParametersCB;
        CreateUniformBuffer(m_pDevice, sizeof(HLSL::PBRRendererShaderParameters), "Tutorial27_PostProcessing::PBRRenderParameters", &pPBRRenderParametersCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER, pPBRRenderParametersCB);

        RefCntAutoPtr<IBuffer> pObjectAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxObjectCount * sizeof(HLSL::ObjectAttribs), "Tutorial27_PostProcessing::ObjectAttribs", &pObjectAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER, pObjectAttribsCB);

        RefCntAutoPtr<IBuffer> pMaterialAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxMaterialCount * sizeof(HLSL::MaterialAttribs), "Tutorial27_PostProcessing::MaterialAttribs", &pMaterialAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER, pMaterialAttribsCB);
    }

    // Create bounding box vertex and index buffers
    {
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER, TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION));
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER, TexturedCube::CreateIndexBuffer(m_pDevice));
    }

    // Create necessary textures for IBL
    {
        LoadEnvironmentMap("textures/papermill.ktx");
    }

    m_PostFXContext               = std::make_unique<PostFXContext>(m_pDevice, PostFXContext::CreateInfo{true, true});
    m_TemporalAntiAliasing        = std::make_unique<TemporalAntiAliasing>(m_pDevice, TemporalAntiAliasing::CreateInfo{true});
    m_ScreenSpaceReflection       = std::make_unique<ScreenSpaceReflection>(m_pDevice, ScreenSpaceReflection::CreateInfo{true});
    m_ScreenSpaceAmbientOcclusion = std::make_unique<ScreenSpaceAmbientOcclusion>(m_pDevice, ScreenSpaceAmbientOcclusion::CreateInfo{true});
    m_Bloom                       = std::make_unique<Bloom>(m_pDevice, Bloom::CreateInfo{true});
    m_SuperResolution             = std::make_unique<SuperResolution>(m_pDevice, SuperResolution::CreateInfo{true});
    m_ShaderSettings              = std::make_unique<ShaderSettings>();

    m_ShaderSettings->PBRRenderParams.OcclusionStrength      = 1.0f;
    m_ShaderSettings->PBRRenderParams.IBLScale               = float4{1.0f};
    m_ShaderSettings->PBRRenderParams.AverageLogLum          = 0.2f;
    m_ShaderSettings->PBRRenderParams.WhitePoint             = HLSL::ToneMappingAttribs{}.fWhitePoint;
    m_ShaderSettings->PBRRenderParams.MiddleGray             = HLSL::ToneMappingAttribs{}.fMiddleGray;
    m_ShaderSettings->PBRRenderParams.PrefilteredCubeLastMip = static_cast<float>(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].AsTexture()->GetDesc().MipLevels - 1);
    m_ShaderSettings->PBRRenderParams.MipBias                = 0;

    m_ShaderSettings->SSRSettings.MaxTraversalIntersections = 64;
    m_ShaderSettings->SSRSettings.RoughnessThreshold        = 1.0f;
    m_ShaderSettings->SSRSettings.IsRoughnessPerceptual     = true;
    m_ShaderSettings->SSRSettings.RoughnessChannel          = 0;

    m_ShaderSettings->FSRSettings.ResolutionScale = 0.75f;
}

// Render a frame
void Tutorial27_PostProcessing::Render()
{
    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

    const HLSL::CameraAttribs& CurrCamAttribs = m_CameraAttribs[CurrFrameIdx];
    const HLSL::CameraAttribs& PrevCamAttribs = m_CameraAttribs[PrevFrameIdx];
    {
        MapHelper<HLSL::CameraAttribs> FrameAttribs{m_pImmediateContext, m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer(), MAP_WRITE, MAP_FLAG_DISCARD};
        FrameAttribs[0] = CurrCamAttribs;
        FrameAttribs[1] = PrevCamAttribs;
    }

    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::PBRRendererShaderParameters), &m_ShaderSettings->PBRRenderParams, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::ObjectAttribs) * m_MaxObjectCount, m_ObjectAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::MaterialAttribs) * m_MaxMaterialCount, m_MaterialAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    PrepareResources();
    if (m_EnableGenerateGeometry)
        GenerateGeometry();
    // If lighting is disabled, clear radiance buffers so downstream passes sample black instead of stale content
    if (!m_EnableLighting)
    {
        for (Uint32 TexIdx = RESOURCE_IDENTIFIER_RADIANCE0; TexIdx <= RESOURCE_IDENTIFIER_RADIANCE1; ++TexIdx)
        {
            if (m_Resources[TexIdx])
            {
                ITextureView* pRTV = m_Resources[TexIdx].GetTextureRTV();
                if (pRTV)
                {
                    float4 ClearColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
                    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
                }
            }
        }
    }
    if (m_EnablePostFX)
        ComputePostFX();
    if (m_EnableSSR)
        ComputeSSR();
    if (m_EnableSSAO)
        ComputeSSAO();
    if (m_EnableLighting)
        ComputeLighting();
    if (m_EnableTAA)
        ComputeTAA();
    if (m_EnableBloom)
        ComputeBloom();
    if (m_EnableToneMapping)
        ComputeToneMapping();
    if (m_EnableFSR)
        ComputeFSR();
    if (m_EnableGammaCorrection)
        ComputeGammaCorrection();
}

void Tutorial27_PostProcessing::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0) & 0x01;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 1) & 0x01;

    const SwapChainDesc& SCDesc    = m_pSwapChain->GetDesc();
    m_PostFXFrameDesc.Width        = static_cast<Uint32>(FastCeil(m_ShaderSettings->FSRSettings.ResolutionScale * static_cast<float>(SCDesc.Width)));
    m_PostFXFrameDesc.Height       = static_cast<Uint32>(FastCeil(m_ShaderSettings->FSRSettings.ResolutionScale * static_cast<float>(SCDesc.Height)));
    m_PostFXFrameDesc.OutputWidth  = SCDesc.Width;
    m_PostFXFrameDesc.OutputHeight = SCDesc.Height;
    m_PostFXFrameDesc.Index        = m_CurrentFrameNumber;

    constexpr float YFov  = PI_F / 4.0f;
    constexpr float ZNear = 0.1f;
    constexpr float ZFar  = 100.f;

    const float2 Jitter = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetJitterOffset() : float2{0.0f, 0.0f};

    const float4x4 CameraView     = m_Camera.GetViewMatrix();
    const float4x4 CameraProj     = TemporalAntiAliasing::GetJitteredProjMatrix(GetAdjustedProjectionMatrix(YFov, ZNear, ZFar), Jitter);
    const float4x4 CameraViewProj = CameraView * CameraProj;
    const float4x4 CameraWorld    = CameraView.Inverse();

    float2 Resolution = float2{static_cast<float>(m_PostFXFrameDesc.Width), static_cast<float>(m_PostFXFrameDesc.Height)};

    HLSL::CameraAttribs& CurrCamAttribs{m_CameraAttribs[CurrFrameIdx]};
    CurrCamAttribs.f4ViewportSize = float4{Resolution.x, Resolution.y, 1.f / Resolution.x, 1.f / Resolution.y};
    CurrCamAttribs.mView          = CameraView;
    CurrCamAttribs.mProj          = CameraProj;
    CurrCamAttribs.mViewProj      = CameraViewProj;
    CurrCamAttribs.mViewInv       = CameraView.Inverse();
    CurrCamAttribs.mProjInv       = CameraProj.Inverse();
    CurrCamAttribs.mViewProjInv   = CameraViewProj.Inverse();
    CurrCamAttribs.f4Position     = float4(float3::MakeVector(CameraWorld[3]), 1);

    CurrCamAttribs.f2Jitter.x       = Jitter.x;
    CurrCamAttribs.f2Jitter.y       = Jitter.y;
    CurrCamAttribs.f4ExtraData[0].x = m_ShaderSettings->SSRStrength;
    CurrCamAttribs.f4ExtraData[0].y = m_ShaderSettings->SSAOStrength;
    {
        m_ObjectCount   = 0;
        m_MaterialCount = 0;

        auto CreateMaterial = [&](const float3& BaseColor, float Roughness, float Metalness) -> Uint32 {
            HLSL::MaterialAttribs MaterialAttribs{};
            MaterialAttribs.BaseColor = float4{BaseColor, 1.0f};
            MaterialAttribs.Metalness = Metalness;
            MaterialAttribs.Roughness = Roughness;

            m_MaterialAttribs[m_MaterialCount] = MaterialAttribs;
            return m_MaterialCount++;
        };

        auto CreateGeometryObjectWithFrequency = [&](const float4x4& Transform,
                                                     Uint32          ObjectType,
                                                     Uint32          MaterialIdx0,
                                                     Uint32          MaterialIdx1,
                                                     Uint32          Dim0,
                                                     Uint32          Dim1,
                                                     float           Frequency0,
                                                     float           Frequency1) -> Uint32 {
            m_ObjectTransforms[CurrFrameIdx][m_ObjectCount] = Transform;

            const float4x4 CurrWorldMatrix = m_ObjectTransforms[CurrFrameIdx][m_ObjectCount];
            const float4x4 PrevWorldMatrix = m_ObjectTransforms[PrevFrameIdx][m_ObjectCount];

            HLSL::ObjectAttribs ObjectAttribs{};
            ObjectAttribs.ObjectType                 = ObjectType;
            ObjectAttribs.CurrInvWorldMatrix         = CurrWorldMatrix.Inverse();
            ObjectAttribs.PrevWorldTransform         = PrevWorldMatrix;
            ObjectAttribs.CurrWorldViewProjectMatrix = CurrWorldMatrix * CurrCamAttribs.mViewProj;
            ObjectAttribs.CurrNormalMatrix           = ObjectAttribs.CurrInvWorldMatrix.Transpose();

            ObjectAttribs.ObjectMaterialIdx0       = MaterialIdx0;
            ObjectAttribs.ObjectMaterialIdx1       = MaterialIdx1;
            ObjectAttribs.ObjectMaterialDim0       = Dim0;
            ObjectAttribs.ObjectMaterialDim1       = Dim1;
            ObjectAttribs.ObjectMaterialFrequency0 = Frequency0;
            ObjectAttribs.ObjectMaterialFrequency1 = Frequency1;

            m_ObjectAttribs[m_ObjectCount] = ObjectAttribs;
            return m_ObjectCount++;
        };

        auto CreateGeometryObject = [&](const float4x4& Transform, Uint32 ObjectType, Uint32 MaterialIdx) -> Uint32 {
            return CreateGeometryObjectWithFrequency(Transform, ObjectType, MaterialIdx, MaterialIdx, 0, 0, 0.0, 0.0);
        };

        constexpr Uint32 SphereCount = 5;

        for (Uint32 SphereIdx = 0; SphereIdx < SphereCount; SphereIdx++)
        {
            const float4x4 Transform   = float4x4::Scale(0.45f) * float4x4::Translation(3.0f - static_cast<float>(SphereIdx) * 0.75f, -0.5f, 1.5f + static_cast<float>(SphereIdx));
            const Uint32   MaterialIdx = CreateMaterial(float3(0.56f, 0.57f, 0.58f), static_cast<float>(SphereIdx) / static_cast<float>(SphereCount - 1), 1.0);
            CreateGeometryObject(Transform, GEOMETRY_TYPE_SPHERE, MaterialIdx);
        }

        for (Uint32 SphereIdx = 0; SphereIdx < SphereCount; SphereIdx++)
        {
            const float4x4 Transform   = float4x4::Scale(0.45f) * float4x4::Translation(3.5f - static_cast<float>(SphereIdx) * 0.75f, +0.5f, 1.5f + static_cast<float>(SphereIdx));
            const Uint32   MaterialIdx = CreateMaterial(float3(0.56f, 0.57f, 0.58f), static_cast<float>(SphereIdx) / static_cast<float>(SphereCount - 1), 0.0);
            CreateGeometryObject(Transform, GEOMETRY_TYPE_SPHERE, MaterialIdx);
        }

        Uint32 Material0 = CreateMaterial(float3(1.00f, 0.71f, 0.29f), 0.05f, 1.0f);
        Uint32 Material1 = CreateMaterial(float3(0.03f, 0.05f, 0.10f), 0.15f, 0.5f);
        Uint32 Material2 = CreateMaterial(float3(0.56f, 0.57f, 0.58f), 0.01f, 1.0f);
        Uint32 Material3 = CreateMaterial(float3(0.24f, 0.24f, 0.84f), 0.50f, 1.0f);
        Uint32 Material4 = CreateMaterial(float3(0.87f, 0.07f, 0.17f), 0.50f, 0.1f);
        Uint32 Material5 = CreateMaterial(float3(0.07f, 0.80f, 0.17f), 0.00f, 0.1f);

        float4x4 Transform0 = float4x4::Scale(20.0f, 0.01f, 20.0f) * float4x4::Translation(0.0f, -1.0f, 0.0f);
        float4x4 Transform1 = float4x4::Scale(1.0f, 1.0f, 0.1f) * float4x4::RotationX(m_AnimationTime) * float4x4::Translation(+3.0f, 0.0f, 0.0f);
        float4x4 Transform2 = float4x4::Scale(1.0f, 1.0f, 0.1f) * float4x4::RotationY(m_AnimationTime) * float4x4::Translation(-3.0f, 0.0f, 0.0f);
        float4x4 Transform3 = float4x4::Translation(0.0f, ::abs(sinf(m_AnimationTime)), 0.0f);
        float4x4 Transform4 = float4x4::Scale(0.3f, 0.3f, 0.3f) * float4x4::RotationZ(m_AnimationTime) * float4x4::Translation(1.0f, 0.5f, 1.0f) * float4x4::RotationY(m_AnimationTime);
        float4x4 Transform5 = float4x4::Scale(0.3f, 0.3f, 0.3f) * float4x4::RotationX(m_AnimationTime) * float4x4::Translation(1.0f, 0.5f, 1.0f) * float4x4::RotationY(m_AnimationTime + PI_F);

        CreateGeometryObjectWithFrequency(Transform0, GEOMETRY_TYPE_AABB, Material0, Material1, 0, 2, 2.0, 2.0);
        CreateGeometryObjectWithFrequency(Transform1, GEOMETRY_TYPE_AABB, Material2, Material3, 0, 2, 4.0, 4.0);
        CreateGeometryObjectWithFrequency(Transform2, GEOMETRY_TYPE_AABB, Material4, Material5, 0, 1, 4.0, 4.0);
        CreateGeometryObject(Transform3, GEOMETRY_TYPE_SPHERE, Material2);
        CreateGeometryObject(Transform4, GEOMETRY_TYPE_AABB, Material3);
        CreateGeometryObject(Transform5, GEOMETRY_TYPE_SPHERE, Material4);

        DEV_CHECK_ERR(m_ObjectCount < m_MaxObjectCount, "Object Count must be lower then Max Object Count");
        DEV_CHECK_ERR(m_MaterialCount < m_MaxMaterialCount, "Material Count must be lower then Max Material Count");

        if (m_IsAnimationActive)
            m_AnimationTime += static_cast<float>(ElapsedTime);
    }

    auto SetupUpsamplingSettings = [](HLSL::SuperResolutionAttribs& Attribs, const PostFXContext::FrameDesc& FrameDesc) {
        Attribs.OutputSize.x = static_cast<float>(FrameDesc.OutputWidth);
        Attribs.OutputSize.y = static_cast<float>(FrameDesc.OutputHeight);
        Attribs.OutputSize.z = 1.0f / static_cast<float>(FrameDesc.OutputWidth);
        Attribs.OutputSize.w = 1.0f / static_cast<float>(FrameDesc.OutputHeight);

        Attribs.SourceSize.x = static_cast<float>(FrameDesc.Width);
        Attribs.SourceSize.y = static_cast<float>(FrameDesc.Height);
        Attribs.SourceSize.z = 1.0f / static_cast<float>(FrameDesc.Width);
        Attribs.SourceSize.w = 1.0f / static_cast<float>(FrameDesc.Height);
    };

    SetupUpsamplingSettings(m_ShaderSettings->FSRSettings, m_PostFXFrameDesc);
}

void Tutorial27_PostProcessing::WindowResize(Uint32 Width, Uint32 Height)
{
    SampleBase::WindowResize(Width, Height);
}

void Tutorial27_PostProcessing::PrepareResources()
{
    RenderDeviceX_N Device{m_pDevice};

    if (!m_Resources[RESOURCE_IDENTIFIER_RADIANCE0] ||
        m_PostFXFrameDesc.Width != m_Resources[RESOURCE_IDENTIFIER_RADIANCE0].AsTexture()->GetDesc().Width ||
        m_PostFXFrameDesc.Height != m_Resources[RESOURCE_IDENTIFIER_RADIANCE0].AsTexture()->GetDesc().Height)
    {
        for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_RADIANCE0; TextureIdx <= RESOURCE_IDENTIFIER_RADIANCE1; TextureIdx++)
        {
            TextureDesc Desc;
            Desc.Name      = "Tutorial27_PostProcessing::Radiance";
            Desc.Type      = RESOURCE_DIM_TEX_2D;
            Desc.Width     = m_PostFXFrameDesc.Width;
            Desc.Height    = m_PostFXFrameDesc.Height;
            Desc.Format    = TEX_FORMAT_R11G11B10_FLOAT;
            Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
            m_Resources.Insert(TextureIdx, Device.CreateTexture(Desc));

            float4        Color = float4(0.0, 0.0, 0.0, 1.0);
            ITextureView* pRTV  = m_Resources[TextureIdx].GetTextureRTV();
            m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pRTV, Color.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_DEPTH0; TextureIdx <= RESOURCE_IDENTIFIER_DEPTH1; TextureIdx++)
        {
            TextureDesc Desc;
            Desc.Name      = "Tutorial27_PostProcessing::Depth";
            Desc.Type      = RESOURCE_DIM_TEX_2D;
            Desc.Width     = m_PostFXFrameDesc.Width;
            Desc.Height    = m_PostFXFrameDesc.Height;
            Desc.Format    = TEX_FORMAT_D32_FLOAT;
            Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
            m_Resources.Insert(TextureIdx, Device.CreateTexture(Desc));

            ITextureView* pDSV = m_Resources[TextureIdx].GetTextureDSV();
            m_pImmediateContext->SetRenderTargets(0, nullptr, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0, 0xFF, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        TextureDesc Desc;
        Desc.Name      = "Tutorial27_PostProcessing::ToneMapping";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = m_PostFXFrameDesc.Width;
        Desc.Height    = m_PostFXFrameDesc.Height;
        Desc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
        Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        m_Resources.Insert(RESOURCE_IDENTIFIER_TONE_MAPPING, Device.CreateTexture(Desc));

        // Create a 1x1 clear texture used as a fallback when features are disabled
        TextureDesc ClearDesc;
        ClearDesc.Name   = "Tutorial27_PostProcessing::ClearTexture";
        ClearDesc.Type   = RESOURCE_DIM_TEX_2D;
        ClearDesc.Width  = 1;
        ClearDesc.Height = 1;
    ClearDesc.Format = TEX_FORMAT_RGBA8_UNORM;
        ClearDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        auto pClearTex = Device.CreateTexture(ClearDesc);
        // Fill with black
        if (pClearTex)
        {
            ITextureView* pRTV = pClearTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            float4 Color = float4(0,0,0,1);
            m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pRTV, Color.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_Resources.Insert(RESOURCE_IDENTIFIER_CLEAR_TEXTURE, pClearTex);
        }
    }

    m_PostFXContext->PrepareResources(m_pDevice, m_PostFXFrameDesc, m_ShaderSettings->PostFXFeatureFlags);

    if (m_ShaderSettings->SSRStrength > 0.0)
    {
        ScreenSpaceReflection::FEATURE_FLAGS ActiveFeatures = m_ShaderSettings->SSRFeatureFlags;
        m_ScreenSpaceReflection->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->SSAOStrength > 0.0)
    {
        ScreenSpaceAmbientOcclusion::FEATURE_FLAGS ActiveFeatures = m_ShaderSettings->SSAOFeatureFlags;
        m_ScreenSpaceAmbientOcclusion->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->TAAEnabled)
    {
        TemporalAntiAliasing::FEATURE_FLAGS ActiveFeatures = m_ShaderSettings->TAAFeatureFlags;
        m_TemporalAntiAliasing->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->BloomEnabled)
    {
        Bloom::FEATURE_FLAGS ActiveFeatures = m_ShaderSettings->BloomFeatureFlags;
        m_Bloom->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->UpsamplingMode == UPSAMPLING_MODE_FSR)
    {
        SuperResolution::FEATURE_FLAGS ActiveFeatures = m_ShaderSettings->SuperResolutionFlags;
        m_SuperResolution->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }
}

void Tutorial27_PostProcessing::GenerateGeometry()
{
    m_GBuffer->Resize(m_pDevice, m_PostFXFrameDesc.Width, m_PostFXFrameDesc.Height);

    RenderTechnique& RenderTech = m_RenderTech[RENDER_TECH_GENERATE_GEOMETRY];
    if (!RenderTech.IsInitializedPSO())
    {
        ShaderMacroHelper Macros;
        Macros.Add("MAX_MATERIAL_COUNT", m_MaxMaterialCount);

        RefCntAutoPtr<IShader> VS = CreateShader(m_pDevice, nullptr, "GenerateGeometry.vsh", "GenerateGeometryVS", SHADER_TYPE_VERTEX);
        RefCntAutoPtr<IShader> PS = CreateShader(m_pDevice, nullptr, "GenerateGeometry.psh", "GenerateGeometryPS", SHADER_TYPE_PIXEL, Macros);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "cbObjectMaterial", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "cbObjectAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

        InputLayoutDescX InputLayout;
        InputLayout.Add(0u, 0u, 3u, VT_FLOAT32, false);

        GraphicsPipelineStateCreateInfoX PipelineCI;
        PipelineCI
            .SetName("Tutorial27_PostProcessing::GenerateGeometry")
            .AddShader(VS)
            .AddShader(PS)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_BASE_COLOR)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDesc().Format)
            .SetDepthFormat(m_Resources[RESOURCE_IDENTIFIER_DEPTH0].AsTexture()->GetDesc().Format)
            .SetResourceLayout(ResourceLayout)
            .SetInputLayout(InputLayout)
            .SetBlendDesc(BS_Default)
            .SetDepthStencilDesc(DSS_Default)
            .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerDesc(RS_SolidFillCullFront);

        RenderTech.PSO = RenderDeviceWithCache<false>{m_pDevice, nullptr}.CreateGraphicsPipelineState(PipelineCI);
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbObjectMaterial"}.Set(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

    ShaderResourceVariableX ObjectAttribVariable{RenderTech.SRB, SHADER_TYPE_PIXEL, "cbObjectAttribs"};
    ObjectAttribVariable.Set(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "GenerateGeometry"};

    Uint64   Offsets[]  = {0};
    IBuffer* pBuffers[] = {m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER].AsBuffer()};

    m_GBuffer->Bind(m_pImmediateContext,
                    GBUFFER_RT_FLAG_ALL, // Bind all render targets
                    m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureDSV(),
                    GBUFFER_RT_FLAG_ALL // Clear all render targets
    );
    m_pImmediateContext->ClearDepthStencil(m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureDSV(), CLEAR_DEPTH_FLAG, 1.0, 0xFF, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetIndexBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER].AsBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    for (Uint32 ObjectIdx = 0; ObjectIdx < m_ObjectCount; ObjectIdx++)
    {
        ObjectAttribVariable.SetBufferRange(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), ObjectIdx * sizeof(HLSL::ObjectAttribs), sizeof(HLSL::ObjectAttribs));
        m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DrawIndexed({36, VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    }
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::ComputePostFX()
{
    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

    {
        PostFXContext::RenderAttributes PostFXAttibs;
        PostFXAttibs.pDevice             = m_pDevice;
        PostFXAttibs.pDeviceContext      = m_pImmediateContext;
        PostFXAttibs.pCameraAttribsCB    = m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer();
        PostFXAttibs.pCurrDepthBufferSRV = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        PostFXAttibs.pPrevDepthBufferSRV = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + PrevFrameIdx].GetTextureSRV();
        PostFXAttibs.pMotionVectorsSRV   = m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_PostFXContext->Execute(PostFXAttibs);
    }
}

void Tutorial27_PostProcessing::ComputeSSR()
{
    if (m_ShaderSettings->SSRStrength > 0.0)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
        const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

        ScreenSpaceReflection::RenderAttributes SSRRenderAttribs{};
        SSRRenderAttribs.pDevice            = m_pDevice;
        SSRRenderAttribs.pDeviceContext     = m_pImmediateContext;
        SSRRenderAttribs.pPostFXContext     = m_PostFXContext.get();
        SSRRenderAttribs.pColorBufferSRV    = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetAccumulatedFrameSRV(true) : m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + PrevFrameIdx].GetTextureSRV();
        SSRRenderAttribs.pDepthBufferSRV    = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        SSRRenderAttribs.pNormalBufferSRV   = m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMaterialBufferSRV = m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMotionVectorsSRV  = m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pSSRAttribs        = &m_ShaderSettings->SSRSettings;
        m_ScreenSpaceReflection->Execute(SSRRenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeSSAO()
{
    if (m_ShaderSettings->SSAOStrength > 0.0)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        ScreenSpaceAmbientOcclusion::RenderAttributes SSAORenderAttribs{};
        SSAORenderAttribs.pDevice          = m_pDevice;
        SSAORenderAttribs.pDeviceContext   = m_pImmediateContext;
        SSAORenderAttribs.pPostFXContext   = m_PostFXContext.get();
        SSAORenderAttribs.pDepthBufferSRV  = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        SSAORenderAttribs.pNormalBufferSRV = m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSAORenderAttribs.pSSAOAttribs     = &m_ShaderSettings->SSAOSettings;
        m_ScreenSpaceAmbientOcclusion->Execute(SSAORenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeLighting()
{
    RenderTechnique& RenderTech = m_RenderTech[RENDER_TECH_COMPUTE_LIGHTING];
    if (!RenderTech.IsInitializedPSO())
    {
        RefCntAutoPtr<IShader> VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        RefCntAutoPtr<IShader> PS = CreateShader(m_pDevice, nullptr, "ComputeLighting.fx", "ComputeLightingPS", SHADER_TYPE_PIXEL);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "cbPBRRendererAttibs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureBaseColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureMaterialData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureNormal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, SHADER_VARIABLE_FLAG_UNFILTERABLE_FLOAT_TEXTURE_WEBGPU)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureSSR", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureSSAO", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureIrradianceMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

        ResourceLayout
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap", Sam_Aniso16xClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureIrradianceMap", Sam_LinearClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap", Sam_LinearClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap", Sam_LinearClamp);

        RenderTech.InitializePSO(m_pDevice,
                                 nullptr, "Tutorial27_PostProcessing::ComputeLighting",
                                 VS, PS, ResourceLayout,
                                 {
                                     m_Resources[RESOURCE_IDENTIFIER_RADIANCE0].AsTexture()->GetDesc().Format,
                                 },
                                 TEX_FORMAT_UNKNOWN,
                                 DSS_DisableDepth, BS_Default, false);

        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_ENVIRONMENT_MAP].GetTextureSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureIrradianceMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_IRRADIANCE_MAP].GetTextureSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].GetTextureSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP].GetTextureSRV());

    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureBaseColor"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_BASE_COLOR)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureMaterialData"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureNormal"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureDepth"}.Set(m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSR"}.Set(m_ScreenSpaceReflection->GetSSRRadianceSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSAO"}.Set(m_ScreenSpaceAmbientOcclusion->GetAmbientOcclusionSRV());
    // Respect runtime toggles: if SSR/SSAO are disabled, bind null so they don't affect lighting
    // If a feature is disabled, use a 1x1 clear texture so the shader reads black and the effect disappears
    ITextureView* pClearSRV = m_Resources[RESOURCE_IDENTIFIER_CLEAR_TEXTURE].GetTextureSRV();
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSR"}.Set(m_EnableSSR ? m_ScreenSpaceReflection->GetSSRRadianceSRV() : pClearSRV);
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSAO"}.Set(m_EnableSSAO ? m_ScreenSpaceAmbientOcclusion->GetAmbientOcclusionSRV() : pClearSRV);

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeLighting"};

    float4 ClearColor = float4(0.0, 0.0, 0.0, 1.0);

    ITextureView* pRTV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureRTV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::ComputeTAA()
{
    if (m_ShaderSettings->TAAEnabled)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        TemporalAntiAliasing::RenderAttributes TAARenderAttribs{};
        TAARenderAttribs.pDevice         = m_pDevice;
        TAARenderAttribs.pDeviceContext  = m_pImmediateContext;
        TAARenderAttribs.pPostFXContext  = m_PostFXContext.get();
        TAARenderAttribs.pColorBufferSRV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
        TAARenderAttribs.pTAAAttribs     = &m_ShaderSettings->TAASettings;
        m_TemporalAntiAliasing->Execute(TAARenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeBloom()
{
    if (m_ShaderSettings->BloomEnabled)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        Bloom::RenderAttributes BloomRenderAttribs{};
        BloomRenderAttribs.pDevice         = m_pDevice;
        BloomRenderAttribs.pDeviceContext  = m_pImmediateContext;
        BloomRenderAttribs.pPostFXContext  = m_PostFXContext.get();
        BloomRenderAttribs.pColorBufferSRV = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetAccumulatedFrameSRV() : m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
        BloomRenderAttribs.pBloomAttribs   = &m_ShaderSettings->BloomSettings;
        m_Bloom->Execute(BloomRenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeToneMapping()
{
    RenderTechnique& RenderTech = m_RenderTech[RENDER_TECH_COMPUTE_TONE_MAPPING];
    if (!RenderTech.IsInitializedPSO())
    {
        ShaderMacroHelper Macros;
        Macros.Add("TONE_MAPPING_MODE", TONE_MAPPING_MODE_UNCHARTED2);

        RefCntAutoPtr<IShader> VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        RefCntAutoPtr<IShader> PS = CreateShader(m_pDevice, nullptr, "ApplyToneMap.fx", "ApplyToneMapPS", SHADER_TYPE_PIXEL, Macros);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbPBRRendererAttibs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureHDR", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

        RenderTech.InitializePSO(m_pDevice,
                                 nullptr, "Tutorial27_PostProcessing::ComputeToneMapping",
                                 VS, PS, ResourceLayout,
                                 {m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].AsTexture()->GetDesc().Format},
                                 TEX_FORMAT_UNKNOWN,
                                 DSS_DisableDepth, BS_Default, false);

        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;


    ITextureView* HDRTextureSRV = nullptr;
    // Respect runtime toggles so disabling Bloom or TAA immediately removes their contribution
    if (m_EnableBloom && m_ShaderSettings->BloomEnabled)
        HDRTextureSRV = m_Bloom->GetBloomTextureSRV();
    else if (m_EnableTAA && m_ShaderSettings->TAAEnabled)
        HDRTextureSRV = m_TemporalAntiAliasing->GetAccumulatedFrameSRV();
    else
        HDRTextureSRV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureHDR"}.Set(HDRTextureSRV);

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeToneMapping"};

    ITextureView* pRTV = m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].GetTextureRTV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::ComputeFSR()
{
    if (m_ShaderSettings->UpsamplingMode == UPSAMPLING_MODE_FSR)
    {
        SuperResolution::RenderAttributes FSRRenderAttribs{};
        FSRRenderAttribs.pDevice         = m_pDevice;
        FSRRenderAttribs.pDeviceContext  = m_pImmediateContext;
        FSRRenderAttribs.pPostFXContext  = m_PostFXContext.get();
        FSRRenderAttribs.pFSRAttribs     = &m_ShaderSettings->FSRSettings;
        FSRRenderAttribs.pColorBufferSRV = m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].GetTextureSRV();
        m_SuperResolution->Execute(FSRRenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeGammaCorrection()
{
    const bool ConvertOutputToGamma = (m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_RGBA8_UNORM ||
                                       m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_BGRA8_UNORM);

    ITextureView* pSRV = (m_ShaderSettings->UpsamplingMode == UPSAMPLING_MODE_FSR && m_EnableFSR) ? m_SuperResolution->GetUpsampledTextureSRV() : m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].GetTextureSRV();
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

    if (ConvertOutputToGamma)
    {
        RenderTechnique& RenderTech = m_RenderTech[RENDER_TECH_COMPUTE_GAMMA_CORRECTION];
        if (!RenderTech.IsInitializedPSO())
        {
            RefCntAutoPtr<IShader> VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
            RefCntAutoPtr<IShader> PS = CreateShader(m_pDevice, nullptr, "GammaCorrection.fx", "GammaCorrectionPS", SHADER_TYPE_PIXEL);

            PipelineResourceLayoutDescX ResourceLayout;
            ResourceLayout
                .AddVariable(SHADER_TYPE_PIXEL, "g_TextureColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
                .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureColor", Sam_LinearClamp);

            RenderTech.InitializePSO(m_pDevice,
                                     nullptr, "Tutorial27_PostProcessing::GammaCorrection",
                                     VS, PS, ResourceLayout,
                                     {pRTV->GetDesc().Format},
                                     TEX_FORMAT_UNKNOWN,
                                     DSS_DisableDepth, BS_Default, false);
            RenderTech.InitializeSRB(false);
        }

        ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureColor"}.Set(pSRV);

        ScopedDebugGroup DebugGroup{m_pImmediateContext, "GammaCorrection"};

        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(RenderTech.PSO);
        m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }
    else
    {
        PostFXContext::TextureOperationAttribs CopyTextureAttribs;
        CopyTextureAttribs.pDevice        = m_pDevice;
        CopyTextureAttribs.pDeviceContext = m_pImmediateContext;
        m_PostFXContext->CopyTextureColor(CopyTextureAttribs, pSRV, pRTV);
    }
}

void Tutorial27_PostProcessing::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        ImGui::Text("FPS: %f", m_fSmoothFPS);

#ifdef PLATFORM_WIN32
        if (ImGui::Button("Load Environment Map"))
        {
            FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
            OpenDialogAttribs.Title  = "Select HDR file";
            OpenDialogAttribs.Filter = "HDR files (*.hdr)\0*.hdr;\0All files\0*.*\0\0";
            std::string FileName     = FileSystem::FileDialog(OpenDialogAttribs);
            if (!FileName.empty())
                LoadEnvironmentMap(FileName.data());
        }
#endif

        if (ImGui::TreeNode("Rendering"))
        {
            ImGui::SliderFloat("Screen Space Reflection Strength", &m_ShaderSettings->SSRStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Screen Space Ambient Occlusion Strength", &m_ShaderSettings->SSAOStrength, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Animation", &m_IsAnimationActive);
            ImGui::Separator();
            ImGui::Text("Feature Toggles:");
            ImGui::Checkbox("Generate Geometry", &m_EnableGenerateGeometry);
            ImGui::Checkbox("Motion Vectors", &m_EnableMotionVectors);
            ImGui::Checkbox("Lighting", &m_EnableLighting);
            ImGui::Checkbox("Post Processing", &m_EnablePostFX);
            ImGui::Checkbox("Screen Space Reflections (SSR)", &m_EnableSSR);
            ImGui::Checkbox("Screen Space Ambient Occlusion (SSAO)", &m_EnableSSAO);
            ImGui::Checkbox("Temporal AA (TAA)", &m_EnableTAA);
            ImGui::Checkbox("Bloom", &m_EnableBloom);
            ImGui::Checkbox("Tone Mapping", &m_EnableToneMapping);
            ImGui::Checkbox("FSR/Upsampling", &m_EnableFSR);
            ImGui::Checkbox("Gamma Correction", &m_EnableGammaCorrection);
            ImGui::Checkbox("Enable TAA", &m_ShaderSettings->TAAEnabled);
            ImGui::Checkbox("Enable Bloom", &m_ShaderSettings->BloomEnabled);
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Post Processing"))
        {
            if (ImGui::TreeNode("Screen Space Reflections"))
            {
                ScreenSpaceReflection::UpdateUI(m_ShaderSettings->SSRSettings, m_ShaderSettings->SSRFeatureFlags, m_SSRSettingsDisplayMode);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Screen Space Ambient Occlusion"))
            {
                ScreenSpaceAmbientOcclusion::UpdateUI(m_ShaderSettings->SSAOSettings, m_ShaderSettings->SSAOFeatureFlags);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Tone mapping"))
            {
                // clang-format off
                ImGui::SliderFloat("Average log lum", &m_ShaderSettings->PBRRenderParams.AverageLogLum, 0.01f, 10.0f);
                ImGui::SliderFloat("Middle gray", &m_ShaderSettings->PBRRenderParams.MiddleGray, 0.01f, 1.0f);
                ImGui::SliderFloat("White point", &m_ShaderSettings->PBRRenderParams.WhitePoint, 0.1f, 20.0f);
                // clang-format on
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Temporal Anti Aliasing"))
            {
                TemporalAntiAliasing::UpdateUI(m_ShaderSettings->TAASettings, m_ShaderSettings->TAAFeatureFlags);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Bloom"))
            {
                Bloom::UpdateUI(m_ShaderSettings->BloomSettings, m_ShaderSettings->BloomFeatureFlags);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Upsampling"))
            {
                const char* UpsamplingType[] = {"Bilinear", "FSR"};

                ImGui::Combo("Mode", reinterpret_cast<Int32*>(&m_ShaderSettings->UpsamplingMode), UpsamplingType, IM_ARRAYSIZE(UpsamplingType));
                switch (m_ShaderSettings->UpsamplingMode)
                {
                    case UPSAMPLING_MODE_FSR:
                        SuperResolution::UpdateUI(m_ShaderSettings->FSRSettings, m_ShaderSettings->SuperResolutionFlags);
                        break;
                    case UPSAMPLING_MODE_BILINEAR:
                        ImGui::SliderFloat("Resolution Scale", &m_ShaderSettings->FSRSettings.ResolutionScale, 0.5f, 1.0f);
                        break;
                    default:
                        UNEXPECTED("Unexpected filter type");
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void Tutorial27_PostProcessing::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_GL)
    {
#if GL_SUPPORTED
        EngineGLCreateInfo* pEngineCI   = static_cast<EngineGLCreateInfo*>(&Attribs.EngineCI);
        pEngineCI->PreferredAdapterType = ADAPTER_TYPE_DISCRETE;
#endif
    }
}

void Tutorial27_PostProcessing::LoadEnvironmentMap(const char* FileName)
{
    // We only need PBR renderer to precompute environment maps
    if (!m_IBLBacker)
        m_IBLBacker = std::make_unique<PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, PBR_Renderer::CreateInfo{});

    for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_ENVIRONMENT_MAP; TextureIdx <= RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP; TextureIdx++)
        m_Resources[TextureIdx].Release();

    RefCntAutoPtr<ITexture> pEnvironmentMap;
    CreateTextureFromFile(FileName, TextureLoadInfo{"Tutorial27_PostProcessing::EnvironmentMap"}, m_pDevice, &pEnvironmentMap);
    DEV_CHECK_ERR(pEnvironmentMap, "Failed to load environment map");
    m_IBLBacker->PrecomputeCubemaps(m_pImmediateContext, pEnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    m_Resources.Insert(RESOURCE_IDENTIFIER_ENVIRONMENT_MAP, m_IBLBacker->GetPrefilteredEnvMapSRV()->GetTexture());
    m_Resources.Insert(RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP, m_IBLBacker->GetPrefilteredEnvMapSRV()->GetTexture());
    m_Resources.Insert(RESOURCE_IDENTIFIER_IRRADIANCE_MAP, m_IBLBacker->GetIrradianceCubeSRV()->GetTexture());
    m_Resources.Insert(RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP, m_IBLBacker->GetPreintegratedGGX_SRV()->GetTexture());
}

} // namespace Diligent

// Function to initialize Diligent Engine with the specified backend
bool InitializeDiligentEngine(
    GLFWwindow* window,
    RefCntAutoPtr<IEngineFactory>& factory,
    RefCntAutoPtr<IRenderDevice>& device,
    RefCntAutoPtr<IDeviceContext>& immediateContext,
    RefCntAutoPtr<ISwapChain>& swapChain,
    IDeviceContext* ppContexts[1])
{
    SwapChainDesc swapChainDesc{};
    swapChainDesc.Width  = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;


    // Helper to get native window for current platform
    struct NativeWindow
{
#if defined(_WIN32)
        Win32NativeWindow win32{};
#elif defined(__APPLE__)
        MacOSNativeWindow mac{};
#elif defined(__linux__)
        LinuxNativeWindow x11{}; // Renamed to avoid potential macro conflict with 'linux'
#endif
    };

    auto GetNativeWindow = [&](GLFWwindow* w) -> NativeWindow {
        NativeWindow nw{};
#if defined(_WIN32)
        nw.win32.hWnd = glfwGetWin32Window(w);
#elif defined(__APPLE__)
        nw.mac.pNSView = [glfwGetCocoaWindow(w) contentView];
#elif defined(__linux__)
        nw.x11.WindowId = glfwGetX11Window(w);
        nw.x11.pDisplay = glfwGetX11Display();
#endif
        return nw;
    };

    NativeWindow nativeWindow = GetNativeWindow(window);

#if defined(_WIN32)
    // Try D3D12
#if D3D12_SUPPORTED
    if (auto* pFactoryD3D12 = LoadAndGetEngineFactoryD3D12())
    {
        RefCntAutoPtr<IEngineFactoryD3D12> factoryD3D12(pFactoryD3D12);
        EngineD3D12CreateInfo engineCI{};
        factoryD3D12->CreateDeviceAndContextsD3D12(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryD3D12->CreateSwapChainD3D12(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow.win32, &swapChain);
            if (swapChain) { factory = factoryD3D12; std::cout << "Render backend: D3D12\n"; return true; }
        }
    }
#endif

    // Try D3D11
#if D3D11_SUPPORTED
    if (auto* pFactoryD3D11 = LoadAndGetEngineFactoryD3D11())
    {
        RefCntAutoPtr<IEngineFactoryD3D11> factoryD3D11(pFactoryD3D11);
        EngineD3D11CreateInfo engineCI{};
        factoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryD3D11->CreateSwapChainD3D11(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow.win32, &swapChain);
            if (swapChain) { factory = factoryD3D11; std::cout << "Render backend: D3D11\n"; return true; }
        }
    }
#endif
#endif // _WIN32

#if defined(__APPLE__) && METAL_SUPPORTED
    if (auto* pFactoryMtl = LoadAndGetEngineFactoryMtl())
    {
        RefCntAutoPtr<IEngineFactoryMtl> factoryMtl(pFactoryMtl);
        EngineMtlCreateInfo engineCI{};
        factoryMtl->CreateDeviceAndContextsMtl(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
            factoryMtl->CreateSwapChainMtl(device, immediateContext, swapChainDesc, nativeWindow.mac, &swapChain);
            if (swapChain) { factory = factoryMtl; std::cout << "Render backend: Metal\n"; return true; }
        }
    }
#endif

#if VULKAN_SUPPORTED
    if (auto* pFactoryVk = LoadAndGetEngineFactoryVk())
    {
        RefCntAutoPtr<IEngineFactoryVk> factoryVk(pFactoryVk);
        EngineVkCreateInfo engineCI{};
        factoryVk->CreateDeviceAndContextsVk(engineCI, &device, ppContexts);
        if (device)
        {
            immediateContext = ppContexts[0];
#if defined(_WIN32)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.win32, &swapChain);
#elif defined(__APPLE__)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.mac, &swapChain);
#elif defined(__linux__)
            factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow.x11, &swapChain);
#endif
            if (swapChain) { factory = factoryVk; std::cout << "Render backend: Vulkan\n"; return true; }
        }
    }
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
    if (auto* pFactoryGL = LoadAndGetEngineFactoryOpenGL())
    {
        RefCntAutoPtr<IEngineFactoryOpenGL> factoryGL(pFactoryGL);
        EngineGLCreateInfo engineCI{};
#if defined(_WIN32)
        engineCI.Window.hWnd = nativeWindow.win32.hWnd;
#elif defined(__APPLE__)
        engineCI.Window.pNSView = nativeWindow.mac.pNSView;
#elif defined(__linux__)
        engineCI.Window.WindowId = nativeWindow.x11.WindowId;
        engineCI.Window.pDisplay = nativeWindow.x11.pDisplay;
#endif
        factoryGL->CreateDeviceAndSwapChainGL(engineCI, &device, &immediateContext, swapChainDesc, &swapChain);
        if (device && swapChain) { ppContexts[0] = immediateContext; factory = factoryGL; std::cout << "Render backend: OpenGL\n"; return true; }
    }
#endif

    return false;
}

int main()
{
    // -------------------
    // Init GLFW
    // -------------------
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "C6GE - TextureArray with ImGui", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    #ifdef __APPLE__
    NSWindow *ns_window = glfwGetCocoaWindow(window);
    NSView *view = [ns_window contentView];
    [view setWantsLayer:YES];
    [view setLayer:[CAMetalLayer layer]];
    #endif

    // -------------------
    // Create Diligent Engine
    // -------------------
    RefCntAutoPtr<IEngineFactory> factory;
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    RefCntAutoPtr<ISwapChain> swapChain;
    IDeviceContext* ppContexts[1] = {nullptr};

    if (!InitializeDiligentEngine(window, factory, device, immediateContext, swapChain, ppContexts))
    {
        std::cerr << "Failed to initialize any rendering backend" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // -------------------
    // Initialize ImGui
    // -------------------
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation
    ImGui::StyleColorsDark();

    // Initialize ImGui GLFW backend
    if (!ImGui_ImplGlfw_InitForOther(window, true))
    {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Initialize ImGui Diligent backend
    std::unique_ptr<ImGuiImplDiligent> imGuiImpl;
    try
    {
        ImGuiDiligentCreateInfo imGuiCI(device, swapChain->GetDesc());
        imGuiImpl = std::make_unique<ImGuiImplDiligent>(imGuiCI);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend: " << e.what() << std::endl;
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    if (!imGuiImpl)
    {
        std::cerr << "Failed to initialize ImGui Diligent backend" << std::endl;
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // -------------------
    // Initialize sample
    // -------------------
    SampleBase* sample = CreateSample();
    SampleInitInfo initInfo;
    initInfo.pDevice = device;
    initInfo.pSwapChain = swapChain;
    initInfo.ppContexts = ppContexts;
    initInfo.NumImmediateCtx = 1;
    initInfo.pEngineFactory = factory;
    initInfo.pImGui = imGuiImpl.get(); // Use raw pointer for SampleInitInfo
    sample->Initialize(initInfo);
    std::cout << "Initialized, forcing WindowResize..." << std::endl;
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    swapChain->Resize(fbW, fbH);
    sample->WindowResize(fbW, fbH);

    // Forward GLFW events to Diligent InputController (Windows). We set the sample as
    // the user pointer so callbacks can access the InputController instance.
    glfwSetWindowUserPointer(window, sample);
#if defined(_WIN32)
    // These callbacks are implemented above only for Windows and forward
    // events into the Win32-specific InputController implementation.
    glfwSetKeyCallback(window, GLFWKeyCallback);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallback);
    glfwSetScrollCallback(window, GLFWScrollCallback);
#elif defined(__APPLE__)
    // Register macOS callbacks that forward into InputControllerMacOS
    glfwSetKeyCallback(window, GLFWKeyCallbackMac);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallbackMac);
    glfwSetCursorPosCallback(window, GLFWCursorPosCallbackMac);
    glfwSetScrollCallback(window, GLFWScrollCallbackMac);
#endif

    
    // -------------------
    // Main loop
    // -------------------
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Window size (logical, used for input)
        int windowW = 0, windowH = 0;
        glfwGetWindowSize(window, &windowW, &windowH);

        // Framebuffer size (physical pixels)
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        // DPI scale
        float dpiScaleX = (float)fbW / (float)windowW;
        float dpiScaleY = (float)fbH / (float)windowH;

        // Resize swap chain if needed
        const auto& scDesc = swapChain->GetDesc();
        if (fbW != static_cast<int>(scDesc.Width) || fbH != static_cast<int>(scDesc.Height))
        {
            swapChain->Resize(fbW, fbH);
            sample->WindowResize(fbW, fbH);
        }

    // Compute elapsed time and start ImGui frame
    double currTime = glfwGetTime();
    double elapsed = currTime - lastTime;
    lastTime = currTime;
        ImGui_ImplGlfw_NewFrame();
        
        // Set DisplaySize and scale for Retina
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)fbW, (float)fbH);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // keep 1.0 because we’re already using pixels

        imGuiImpl->NewFrame(fbW, fbH, scDesc.PreTransform);

    // Build UI, pass elapsed time so camera receives proper dt
    sample->Update(currTime, elapsed, true);

        

        // Render
        ImGui::Render();
        sample->Render();

        ITextureView* pRTV = swapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = swapChain->GetDepthBufferDSV();
        immediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        imGuiImpl->Render(immediateContext);

        swapChain->Present(enableVsync ? 1 : 0);
    }

    // -------------------
    // Cleanup
    // -------------------
    delete sample;
    ImGui_ImplGlfw_Shutdown();
    imGuiImpl.reset();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
