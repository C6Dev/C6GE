#pragma once

namespace C6GE {

	struct Texture {
    unsigned int id;          // OpenGL texture ID
    std::string type;         // diffuse, specular etc.
    std::string path;         // file path to avoid loading duplicates
	};

    struct TextureComponent {
	unsigned int Texture;
	TextureComponent(unsigned int id) : Texture(id) {}
	};

}