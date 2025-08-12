#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;   // optional vertex color
layout (location = 3) in vec2 aTex;

out vec3 Normal;
out vec3 FragPos;
out vec3 color;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = proj * view * worldPos;

    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    color = aColor;
    texCoord = aTex;
}
