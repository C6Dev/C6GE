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
    float cutoff;  // Cosine of cutoff angle for spot light
};

uniform Light lights[MAX_LIGHTS];
uniform int numLights;

// Optional fallback color if your model doesn't have vertex colors
uniform vec3 objectColor = vec3(1.0);

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // Use vertex color only if it is non-zero; otherwise use objectColor uniform
    vec3 baseColor = (color == vec3(0.0)) ? objectColor : color;

    vec4 texColor = texture(tex0, texCoord) * vec4(baseColor, 1.0);
    float specMap = texture(specularMap, texCoord).r;

    vec3 result = vec3(0.0);

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
        vec3 ambient = ambientStrength * light.color * texColor.rgb;

        // Diffuse
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * light.color * texColor.rgb;

        // --- Blinn–Phong Specular ---
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
        float specularStrength = specMap;
        vec3 specular = specularStrength * spec * light.color;

        // Spotlight factor
        float spotFactor = 1.0;
        if (light.type == 2) { // Spot
            float theta = dot(lightDir, normalize(-light.direction));
            float epsilon = 0.05; // Soft edge width
            spotFactor = clamp((theta - light.cutoff) / epsilon, 0.0, 1.0);
        }

        result += (ambient + (diffuse + specular) * spotFactor) * attenuation * light.intensity;
    }

    FragColor = vec4(result, texColor.a);
}
