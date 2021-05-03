#version 460

#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform Data { 
  vec4 cameraPos; 
  vec4 lightPosition;
  vec4 lightColor;
  }
inData;

layout(location = 0) out vec4 outColor;

layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec3 inPosition;

void main() {
  const float ambientStrength = 0.4f;
  const float specularStrength = 0.4f;

  vec3 ambient = ambientStrength * inData.lightColor.xyz;

  vec3 norm = normalize(inNormal);
  vec3 lightDirection = normalize(inData.lightPosition.xyz - inPosition);
  float diff = max(dot(norm, lightDirection), 0.0);
  vec3 diffuse = 0.3 * diff * inData.lightColor.xyz;

  vec3 viewDir = normalize(inData.cameraPos.xyz - inPosition);
  vec3 reflectDir = reflect(-lightDirection, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular = specularStrength * spec * inData.lightColor.xyz;

  vec3 result = (ambient + diffuse /* + specular */) * fragColor.xyz;

  outColor = vec4(result, fragColor.w);
}