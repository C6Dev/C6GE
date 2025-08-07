#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec3 color;
in vec2 texCoord;

uniform sampler2D tex0;
uniform sampler2D specularMap;
uniform vec3 viewPos;

#define MAX_LIGHTS 3

struct Light {
    int type; // 0: Point, 1: Directional, 2: Spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float cutoff;
};

uniform Light lights[MAX_LIGHTS];
uniform int numLights;

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 result = vec3(0.0);
    vec3 texColor = texture(tex0, texCoord).rgb * color;
    float specMap = texture(specularMap, texCoord).r;

    for (int i = 0; i < numLights; i++) {
        Light light = lights[i];
        vec3 lightDir;
        float attenuation = 1.0;

        if (light.type == 1) { // Directional
            lightDir = normalize(-light.direction);
        } else { // Point or Spot
            lightDir = normalize(light.position - FragPos);
            float distance = length(light.position - FragPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        }

        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * light.color * texColor;

        // Diffuse
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * light.color * texColor;

        // Specular
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        float specularStrength = specMap;
        vec3 specular = specularStrength * spec * light.color;

        // Spot light
        float spotFactor = 1.0;
        if (light.type == 2) { // Spot
            float theta = dot(lightDir, normalize(-light.direction));
            if (theta > light.cutoff) {
                spotFactor = 1.0;
            } else {
                spotFactor = 0.0;
            }
        }

        result += (ambient + (diffuse + specular) * spotFactor) * attenuation * light.intensity;
    }

    FragColor = vec4(result, 1.0);
}