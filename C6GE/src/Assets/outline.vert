#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;  // Optional
layout (location = 3) in vec2 aTex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform float outlineScale = 1.05; // Scale factor for the outline
uniform vec3 viewPos; // Camera position for distance-based scaling

void main() {
    // Transform the vertex position to world space
    vec4 worldPos = model * vec4(aPos, 1.0);
    
    // Calculate distance from camera to vertex
    float distanceToCamera = length(viewPos - worldPos.xyz);
    
    // Scale the outline based on distance (farther = larger outline)
    // Base scale of 0.01 at close range, increasing with distance
    float distanceScale = 0.01 * (1.0 + distanceToCamera * 0.05);
    
    // Scale the vertex position along its normal for the outline effect
    vec3 scaledPos = aPos + aNormal * outlineScale * distanceScale;
    
    gl_Position = proj * view * model * vec4(scaledPos, 1.0);
}