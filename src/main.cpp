#include <iostream>
#include <thread>
#include <chrono>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "DiligentCore/Graphics/GraphicsEngine/interface/EngineFactory.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/Shader.h"
#include "DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h"
#include "DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "DiligentCore/Platforms/Win32/interface/Win32NativeWindow.h"

#include "main.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

using namespace Diligent;

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial01_HelloTriangle();
}

// Vertex shader source
static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

// Pixel shader source
static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

void Tutorial01_HelloTriangle::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Pipeline state object configuration
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Create vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Triangle vertex shader";
        ShaderCI.Source = VSSource;
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "Triangle pixel shader";
        ShaderCI.Source = PSSource;
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
}

void Tutorial01_HelloTriangle::Render()
{
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // Bind render targets to the device context
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Now clear the render target and depth-stencil buffer
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set pipeline state and draw
    m_pImmediateContext->SetPipelineState(m_pPSO);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    m_pImmediateContext->Draw(drawAttrs);
}

void Tutorial01_HelloTriangle::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);
}

} // namespace Diligent

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "Diligent Engine - Hello Triangle", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark(); // optional, for dark theme

    // Initialize Diligent Engine
    SampleBase* sample = CreateSample();
    SampleInitInfo initInfo;

    // Dynamically load the D3D12 engine factory
    RefCntAutoPtr<IEngineFactoryD3D12> factory;
    auto GetFactoryFunc = LoadGraphicsEngineD3D12();
    if (GetFactoryFunc == nullptr)
    {
        std::cerr << "Failed to load D3D12 engine factory function" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    factory = GetFactoryFunc();
    if (!factory)
    {
        std::cerr << "Failed to initialize D3D12 engine factory" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    RefCntAutoPtr<ISwapChain> swapChain;

    // Create device and contexts
    IDeviceContext* ppContexts[1] = {nullptr};
    EngineD3D12CreateInfo engineCI;
    factory->CreateDeviceAndContextsD3D12(engineCI, &device, ppContexts);
    immediateContext = ppContexts[0];

    // Create swap chain
    SwapChainDesc swapChainDesc;
    swapChainDesc.Width = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

    Win32NativeWindow nativeWindow{glfwGetWin32Window(window)};
    factory->CreateSwapChainD3D12(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow, &swapChain);

    // Set up SampleInitInfo
    initInfo.pDevice = device;
    initInfo.pSwapChain = swapChain;
    initInfo.ppContexts = ppContexts;

    // Initialize the sample
    sample->Initialize(initInfo);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        sample->Update(glfwGetTime(), 0.016, true); // Assuming ~60 FPS
        sample->Render();
        swapChain->Present();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Simulate frame time
    }

    // Cleanup
    delete sample;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}