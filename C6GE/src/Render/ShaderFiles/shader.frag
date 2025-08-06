#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec3 color;
in vec2 texCoord;

uniform sampler2D tex0;
uniform sampler2D specularMap;
uniform vec3 lightPos[4];
uniform vec3 lightColor[4];
uniform float lightIntensity[4];
uniform int numLights;
uniform vec3 viewPos;

void main() {
    vec3 result = vec3(0.0);
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    float ambientStrength = 0.1;
    for(int i = 0; i < numLights; i++) {
        vec3 lightDir = normalize(lightPos[i] - FragPos);
        vec3 ambient = ambientStrength * lightColor[i];
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor[i];
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        float specularStrength = (1.0 - texture(specularMap, texCoord).r) * 0.5;
        vec3 specular = specularStrength * spec * lightColor[i];
        result += (ambient + diffuse + specular) * lightIntensity[i];
    }
    result *= color * texture(tex0, texCoord).rgb;
    FragColor = vec4(result, 1.0);
}