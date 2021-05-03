#version 460

#extension GL_EXT_debug_printf : enable

layout(binding = 8) uniform Data { 
  vec4 cameraPos; 
  vec4 lightPosition;
  vec4 lightColor;
  }
inData;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inPosition;
layout(location = 3) flat in uint inGridID;


void main() {
  //debugPrintfEXT("gridID: %u", inGridID);

  const float ambientStrength = 0.4f;
  const float specularStrength = 0.4f;

  vec3 ambient = ambientStrength * inData.lightColor.xyz;

  vec3 norm = normalize(inNormal);
  vec3 lightDirection = normalize(inData.lightPosition.xyz - inPosition);
  float diff = max(dot(norm, lightDirection), 0.0);
  vec3 diffuse = 0.7 * diff * inData.lightColor.xyz;

  vec3 viewDir = normalize(inData.cameraPos.xyz - inPosition);
  vec3 reflectDir = reflect(-lightDirection, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular = specularStrength * spec * inData.lightColor.xyz;

  vec3 result = (ambient + diffuse + specular) * fragColor.xyz;

  outColor = vec4(result, fragColor.w);
}
