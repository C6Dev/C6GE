#pragma once

#include <cstddef>

namespace Diligent
{

class ShadowsSample; // Forward declaration

// Shadows feature encapsulates all shadow-related logic
class ShadowsFeature
{
public:
    explicit ShadowsFeature(ShadowsSample* owner) : m_Owner(owner) {}

    // Initialize or reinitialize shadow resources and bindings
    void InitShadows();

    // Per-frame shadow updates (e.g., cascade distribution)
    void UpdateShadows();

    // Render shadow map(s)
    void RenderShadows();

    // Prepare SRBs and bind shadow resources or dummy textures depending on state
    void InitializeResourceBindings();

private:
    ShadowsSample* m_Owner = nullptr;
};

} // namespace Diligent