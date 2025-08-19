#pragma once

namespace C6GE {

    struct CubemapComponent {
        unsigned int Cubemap;
        CubemapComponent(unsigned int id) : Cubemap(id) {}
    };

    struct SkyboxComponent {
        unsigned int Cubemap;
        SkyboxComponent(unsigned int id) : Cubemap(id) {}
    };

} // namespace C6GE