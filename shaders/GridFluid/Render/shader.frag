#version 450


#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform Pos {vec3 cameraPos;} inCameraPos;

layout(location = 0) out vec4 outColor;

layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec3 inPosition;

void main() {
    const float ambientStrength = 0.4f;
    const vec3 lightColor = vec3(1.0f);
    const vec3 lightPosition = vec3(0.0f, 5.0f, 0.0f);
    const float specularStrength = 0.4f;

    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(inNormal);
    vec3 lightDirection = normalize(lightPosition - inPosition);
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = 0.3 * diff * lightColor;


    vec3 viewDir = normalize(inCameraPos.cameraPos - inPosition);
    vec3 reflectDir = reflect(-lightDirection, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse /* + specular */) * fragColor.xyz;

    outColor = vec4(result, fragColor.w);
}