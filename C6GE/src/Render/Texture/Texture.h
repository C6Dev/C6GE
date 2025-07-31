#include <string>
#include <iostream>

namespace C6GE {

    using GLuint = unsigned int;
    unsigned char* LoadTexture(const std::string& path, int& widthImg, int& heightImg, int& numColCh);
    GLuint CreateTexture(unsigned char* data, int width, int height, int channels);
}