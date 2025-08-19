#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;   // optional vertex color
layout (location = 3) in vec2 aTex;
layout (location = 4) in vec4 instanceMatrixRow0;
layout (location = 5) in vec4 instanceMatrixRow1;
layout (location = 6) in vec4 instanceMatrixRow2;
layout (location = 7) in vec4 instanceMatrixRow3;

out vec3 Normal;
out vec3 FragPos;
out vec3 color;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform bool isInstanced = false;

out vec3 LocalPos;

void main() {
    mat4 effectiveModel;
    if (isInstanced) {
        effectiveModel = mat4(instanceMatrixRow0, instanceMatrixRow1, instanceMatrixRow2, instanceMatrixRow3);
    } else {
        effectiveModel = model;
    }
    vec4 worldPos = effectiveModel * vec4(aPos, 1.0);
    gl_Position = proj * view * worldPos;

    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(effectiveModel))) * aNormal;
    color = aColor;
    texCoord = aTex;
    LocalPos = aPos;
}
