#include "Platform/FileDialog.h"

#if defined(PLATFORM_WIN32)
#    define NOMINMAX
#    include <windows.h>
#    include <shobjidl.h> // IFileOpenDialog
#    include <combaseapi.h>
#    include <vector>
#    include <codecvt>
#    include <locale>
#endif

namespace Diligent {
namespace Platform {

#if defined(PLATFORM_WIN32)

static std::wstring ToWide(const std::string& s)
{
    if (s.empty())
        return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w;
    w.resize(needed);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], needed);
    return w;
}

static std::string ToUTF8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s;
    s.resize(needed);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], needed, nullptr, nullptr);
    return s;
}

bool OpenFileDialogGLTF(std::string& outPath)
{
    outPath.clear();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT h = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(h) || pFileOpen == nullptr)
    {
        if (needUninit) CoUninitialize();
        return false;
    }

    // Filters: glTF, glb, all files
    COMDLG_FILTERSPEC fileTypes[] = {
        {L"glTF Files (*.gltf; *.glb)", L"*.gltf;*.glb"},
        {L"glTF Text (*.gltf)", L"*.gltf"},
        {L"glTF Binary (*.glb)", L"*.glb"},
        {L"All Files (*.*)", L"*.*"}
    };
    pFileOpen->SetFileTypes(static_cast<UINT>(std::size(fileTypes)), fileTypes);
    pFileOpen->SetFileTypeIndex(1); // default to glTF/glb

    // Options: force file must exist
    DWORD dwFlags = 0;
    if (SUCCEEDED(pFileOpen->GetOptions(&dwFlags)))
    {
        pFileOpen->SetOptions(dwFlags | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    }

    h = pFileOpen->Show(nullptr);
    if (FAILED(h))
    {
        pFileOpen->Release();
        if (needUninit) CoUninitialize();
        return false; // canceled or failed
    }

    IShellItem* pItem = nullptr;
    h = pFileOpen->GetResult(&pItem);
    if (FAILED(h) || pItem == nullptr)
    {
        pFileOpen->Release();
        if (needUninit) CoUninitialize();
        return false;
    }

    PWSTR pszFilePath = nullptr;
    h = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(h) && pszFilePath)
    {
        std::wstring pathW(pszFilePath);
        CoTaskMemFree(pszFilePath);
        outPath = ToUTF8(pathW);
    }
    pItem->Release();
    pFileOpen->Release();

    if (needUninit) CoUninitialize();

    return !outPath.empty();
}

bool OpenFileDialogEnvironmentMap(std::string& outPath)
{
    outPath.clear();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT h = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(h) || pFileOpen == nullptr)
    {
        if (needUninit) CoUninitialize();
        return false;
    }

    COMDLG_FILTERSPEC fileTypes[] = {
        {L"Environment Maps (*.ktx; *.ktx2; *.hdr; *.dds; *.exr)", L"*.ktx;*.ktx2;*.hdr;*.dds;*.exr"},
        {L"KTX (*.ktx; *.ktx2)", L"*.ktx;*.ktx2"},
        {L"Radiance HDR (*.hdr)", L"*.hdr"},
        {L"DirectDraw Surface (*.dds)", L"*.dds"},
        {L"OpenEXR (*.exr)", L"*.exr"},
        {L"All Files (*.*)", L"*.*"}
    };
    pFileOpen->SetFileTypes(static_cast<UINT>(std::size(fileTypes)), fileTypes);
    pFileOpen->SetFileTypeIndex(1);

    DWORD dwFlags = 0;
    if (SUCCEEDED(pFileOpen->GetOptions(&dwFlags)))
    {
        pFileOpen->SetOptions(dwFlags | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    }

    h = pFileOpen->Show(nullptr);
    if (FAILED(h))
    {
        pFileOpen->Release();
        if (needUninit) CoUninitialize();
        return false;
    }

    IShellItem* pItem = nullptr;
    h = pFileOpen->GetResult(&pItem);
    if (FAILED(h) || pItem == nullptr)
    {
        pFileOpen->Release();
        if (needUninit) CoUninitialize();
        return false;
    }

    PWSTR pszFilePath = nullptr;
    h = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
    if (SUCCEEDED(h) && pszFilePath)
    {
        std::wstring pathW(pszFilePath);
        CoTaskMemFree(pszFilePath);
        outPath = ToUTF8(pathW);
    }
    if (pItem)
        pItem->Release();
    if (pFileOpen)
        pFileOpen->Release();

    if (needUninit) CoUninitialize();

    return !outPath.empty();
}

#else

bool OpenFileDialogGLTF(std::string& outPath)
{
    (void)outPath;
    return false; // Not implemented on this platform yet
}

bool OpenFileDialogEnvironmentMap(std::string& outPath)
{
    (void)outPath;
    return false;
}

#endif

} // namespace Platform
} // namespace Diligent
