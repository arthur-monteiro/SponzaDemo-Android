layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in uint inSubMeshIdx;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoords;
layout(location = 2) out uint outMaterialID;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = getProjectionMatrix() * getViewMatrix() * vec4(inPosition, 1.0);

    outColor = inColor;
    outTexCoords = inTexCoord;
    outMaterialID = inSubMeshIdx + 1;
} 
