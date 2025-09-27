#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

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

// Include engine factory headers for all supported backends
#include "DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "DiligentCore/Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "DiligentCore/Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#ifdef __APPLE__
#include "DiligentCore/Graphics/GraphicsEngineMetal/interface/EngineFactoryMtl.h"
#include "DiligentCore/Platforms/MacOS/interface/MacOSNativeWindow.h"
#endif
#ifdef _WIN32
#include "DiligentCore/Platforms/Win32/interface/Win32NativeWindow.h"
#endif
#ifdef __linux__
#include "DiligentCore/Platforms/Linux/interface/LinuxNativeWindow.h"
#endif

#include "main.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "ImGuiImplDiligent.hpp"

#include <random>

#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "TexturedCube.hpp"

using namespace Diligent;

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial05_TextureArray();
}

namespace
{

struct InstanceData
{
    float4x4 Matrix;
    float    TextureInd = 0;
};

} // namespace

void Tutorial05_TextureArray::CreatePipelineState()
{
    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False},
            
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 6 - texture array index
        LayoutElement{6, 1, 1, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
    };
    // clang-format on

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice                = m_pDevice;
    CubePsoCI.RTVFormat              = m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat              = m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory   = pShaderSourceFactory;
    CubePsoCI.VSFilePath             = "cube_inst.vsh";
    CubePsoCI.PSFilePath             = "cube_inst.psh";
    CubePsoCI.ExtraLayoutElements    = LayoutElems;
    CubePsoCI.NumExtraLayoutElements = _countof(LayoutElems);

    m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

    // Create dynamic uniform buffer that will store our transformation matrix
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial05_TextureArray::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(InstanceData) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void Tutorial05_TextureArray::LoadTextures()
{
    std::vector<RefCntAutoPtr<ITextureLoader>> TexLoaders(NumTextures);
    // Load textures
    for (int tex = 0; tex < NumTextures; ++tex)
    {
        // Create loader for the current texture
        std::stringstream FileNameSS;
        FileNameSS << "C6GELogo" << tex << ".png";
        const auto      FileName = FileNameSS.str();
        TextureLoadInfo LoadInfo;
        LoadInfo.IsSRGB = true;

        CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &TexLoaders[tex]);
        VERIFY_EXPR(TexLoaders[tex]);
        VERIFY(tex == 0 || TexLoaders[tex]->GetTextureDesc() == TexLoaders[0]->GetTextureDesc(), "All textures must be same size");
    }

    TextureDesc TexArrDesc = TexLoaders[0]->GetTextureDesc();
    TexArrDesc.ArraySize   = NumTextures;
    TexArrDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
    TexArrDesc.Usage       = USAGE_DEFAULT;
    TexArrDesc.BindFlags   = BIND_SHADER_RESOURCE;

    // Prepare initialization data
    std::vector<TextureSubResData> SubresData(TexArrDesc.ArraySize * TexArrDesc.MipLevels);
    for (Uint32 slice = 0; slice < TexArrDesc.ArraySize; ++slice)
    {
        for (Uint32 mip = 0; mip < TexArrDesc.MipLevels; ++mip)
        {
            SubresData[slice * TexArrDesc.MipLevels + mip] = TexLoaders[slice]->GetSubresourceData(mip, 0);
        }
    }
    TextureData InitData{SubresData.data(), TexArrDesc.MipLevels * TexArrDesc.ArraySize};

    // Create the texture array
    RefCntAutoPtr<ITexture> pTexArray;
    m_pDevice->CreateTexture(TexArrDesc, &InitData, &pTexArray);

    // Get shader resource view from the texture array
    m_TextureSRV = pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}

void Tutorial05_TextureArray::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
        {
            PopulateInstanceBuffer();
        }
    }
    ImGui::End();

    ImGui::Begin("Demo Window", nullptr);
    ImGui::Text("This window is created by the Dear ImGui library.");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void Tutorial05_TextureArray::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();

    // Load cube vertex and index buffers
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);

    CreateInstanceBuffer();
    LoadTextures();
}

void Tutorial05_TextureArray::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    const size_t              zGridSize = static_cast<size_t>(m_GridSize);
    std::vector<InstanceData> Instances(zGridSize * zGridSize * zGridSize);

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);
    std::uniform_int_distribution<Int32>  tex_distr(0, NumTextures - 1);

    float BaseScale = 0.6f / fGridSize;
    int   instId    = 0;
    for (int x = 0; x < m_GridSize; ++x)
    {
        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int z = 0; z < m_GridSize; ++z)
            {
                // Add random offset from central position in the grid
                float xOffset = 2.f * (x + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float yOffset = 2.f * (y + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float zOffset = 2.f * (z + 0.5f + offset_distr(gen)) / fGridSize - 1.f;
                // Random scale
                float scale = BaseScale * scale_distr(gen);
                // Random rotation
                float4x4 rotation = float4x4::RotationX(rot_distr(gen));
                rotation *= float4x4::RotationY(rot_distr(gen));
                rotation *= float4x4::RotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix        = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                InstanceData& CurrInst = Instances[instId++];
                CurrInst.Matrix        = matrix;
                // Texture array index
                CurrInst.TextureInd = static_cast<float>(tex_distr(gen));
            }
        }
    }
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(Instances[0]) * Instances.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, Instances.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

// Render a frame
void Tutorial05_TextureArray::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // Bind the render target and depth-stencil view
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }

    // Bind vertex, instance and index buffers
    const Uint64 offsets[] = {0, 0};
    IBuffer*    pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize; // The number of instances
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial05_TextureArray::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    // Set cube view matrix
    float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    float4x4 Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute view-projection matrix
    m_ViewProjMatrix = View * SrfPreTransform * Proj;

    // Global rotation matrix
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
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
    SwapChainDesc swapChainDesc;
    swapChainDesc.Width = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;

#if defined(_WIN32)
    // Windows: Try D3D12, D3D11, Vulkan, OpenGL
    {
        Win32NativeWindow nativeWindow{ glfwGetWin32Window(window) };

        // Try Direct3D12
#if D3D12_SUPPORTED
        {
            auto* pFactoryD3D12 = LoadAndGetEngineFactoryD3D12();
            if (pFactoryD3D12)
            {
                RefCntAutoPtr<IEngineFactoryD3D12> factoryD3D12(pFactoryD3D12);
                EngineD3D12CreateInfo engineCI;
                factoryD3D12->CreateDeviceAndContextsD3D12(engineCI, &device, ppContexts);
                if (device)
                {
                    immediateContext = ppContexts[0];
                    factoryD3D12->CreateSwapChainD3D12(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow, &swapChain);
                    if (swapChain)
                    {
                        std::cout << "Render backend: Direct3D12" << std::endl;
                        factory = factoryD3D12;
                        return true;
                    }
                    device.Release();
                    immediateContext.Release();
                    ppContexts[0] = nullptr;
                }
            }
        }
#endif

        // Try Direct3D11
#if D3D11_SUPPORTED
        {
            auto* pFactoryD3D11 = LoadAndGetEngineFactoryD3D11();
            if (pFactoryD3D11)
            {
                RefCntAutoPtr<IEngineFactoryD3D11> factoryD3D11(pFactoryD3D11);
                EngineD3D11CreateInfo engineCI;
                factoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &device, ppContexts);
                if (device)
                {
                    immediateContext = ppContexts[0];
                    factoryD3D11->CreateSwapChainD3D11(device, immediateContext, swapChainDesc, FullScreenModeDesc{}, nativeWindow, &swapChain);
                    if (swapChain)
                    {
                        std::cout << "Render backend: Direct3D11" << std::endl;
                        factory = factoryD3D11;
                        return true;
                    }
                    device.Release();
                    immediateContext.Release();
                    ppContexts[0] = nullptr;
                }
            }
        }
#endif
    }
#endif

    // Try Vulkan (Windows, Linux, or macOS fallback)
#if VULKAN_SUPPORTED
    {
        auto* pFactoryVk = LoadAndGetEngineFactoryVk();
        if (pFactoryVk)
        {
            RefCntAutoPtr<IEngineFactoryVk> factoryVk(pFactoryVk);
            EngineVkCreateInfo engineCI;
            factoryVk->CreateDeviceAndContextsVk(engineCI, &device, ppContexts);
            if (device)
            {
                immediateContext = ppContexts[0];
#ifdef _WIN32
                Win32NativeWindow nativeWindow{ glfwGetWin32Window(window) };
                factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow, &swapChain);
#elif defined(__linux__)
                LinuxNativeWindow nativeWindow{ glfwGetX11Window(window), glfwGetX11Display() };
                factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow, &swapChain);
#else // __APPLE__
                MacOSNativeWindow nativeWindow{ glfwGetCocoaWindow(window) };
                factoryVk->CreateSwapChainVk(device, immediateContext, swapChainDesc, nativeWindow, &swapChain);
#endif
                if (swapChain)
                {
                    std::cout << "Render backend: Vulkan" << std::endl;
                    factory = factoryVk;
                    return true;
                }
                device.Release();
                immediateContext.Release();
                ppContexts[0] = nullptr;
            }
        }
    }
#endif

    // Try OpenGL (Windows, Linux, or macOS fallback)
#if GL_SUPPORTED || GLES_SUPPORTED
    {
        auto* pFactoryOpenGL = LoadAndGetEngineFactoryOpenGL();
        if (pFactoryOpenGL)
        {
            RefCntAutoPtr<IEngineFactoryOpenGL> factoryOpenGL(pFactoryOpenGL);
            EngineGLCreateInfo engineCI;
            engineCI.Window = {};
#ifdef _WIN32
            Win32NativeWindow nativeWindow{ glfwGetWin32Window(window) };
            engineCI.Window.hWnd = nativeWindow.hWnd;
#elif defined(__linux__)
            LinuxNativeWindow nativeWindow{ glfwGetX11Window(window), glfwGetX11Display() };
            engineCI.Window.WindowId = nativeWindow.WindowId;
            engineCI.Window.pDisplay = nativeWindow.pDisplay;
#elif defined(__APPLE__)
            MacOSNativeWindow nativeWindow{ glfwGetCocoaWindow(window) };
            engineCI.Window.pNSView = nativeWindow.pNSView;
#endif
            factoryOpenGL->CreateDeviceAndSwapChainGL(engineCI, &device, &immediateContext, swapChainDesc, &swapChain);
            if (device && swapChain)
            {
                std::cout << "Render backend: OpenGL" << std::endl;
                ppContexts[0] = immediateContext;
                factory = factoryOpenGL;
                return true;
            }
            device.Release();
            immediateContext.Release();
            ppContexts[0] = nullptr;
        }
    }
#endif

#if defined(__APPLE__) && METAL_SUPPORTED
    // Try Metal (macOS only)
    {
        auto* pFactoryMtl = LoadAndGetEngineFactoryMtl();
        if (pFactoryMtl)
        {
            RefCntAutoPtr<IEngineFactoryMtl> factoryMtl(pFactoryMtl);
            EngineMtlCreateInfo engineCI;
            factoryMtl->CreateDeviceAndContextsMtl(engineCI, &device, ppContexts);
            if (device)
            {
                immediateContext = ppContexts[0];
                MacOSNativeWindow nativeWindow{ glfwGetCocoaWindow(window) };
                factoryMtl->CreateSwapChainMtl(device, immediateContext, swapChainDesc, nativeWindow, &swapChain);
                if (swapChain)
                {
                    std::cout << "Render backend: Metal" << std::endl;
                    factory = factoryMtl;
                    return true;
                }
                device.Release();
                immediateContext.Release();
                ppContexts[0] = nullptr;
            }
        }
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

    // -------------------
    // Main loop
    // -------------------
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Query the current framebuffer size from GLFW
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        // Resize swap chain if window size changed
        if (fbW != static_cast<int>(swapChain->GetDesc().Width) || fbH != static_cast<int>(swapChain->GetDesc().Height))
        {
            swapChain->Resize(fbW, fbH);
            sample->WindowResize(fbW, fbH);
        }

        // Calculate delta time
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Start new ImGui frame
        ImGui_ImplGlfw_NewFrame();
        const SwapChainDesc& scDesc = swapChain->GetDesc();
        imGuiImpl->NewFrame(scDesc.Width, scDesc.Height, scDesc.PreTransform);

        // Update sample (calls UpdateUI to set up ImGui widgets)
        sample->Update(currentTime, deltaTime, true);

        // Render sample
        sample->Render();

        // Render ImGui
        ITextureView* pRTV = swapChain->GetCurrentBackBufferRTV();
        ITextureView* pDSV = swapChain->GetDepthBufferDSV();
        immediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        imGuiImpl->Render(immediateContext);

        // Present
        swapChain->Present();

        // Slight throttle to avoid pegging CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
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