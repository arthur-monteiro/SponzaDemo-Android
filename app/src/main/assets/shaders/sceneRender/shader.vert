layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoords;
layout(location = 1) out uint outMaterialID;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = getProjectionMatrix() * getViewMatrix()  * getInstanceTransform() * vec4(inPosition, 1.0);

    outTexCoords = inTexCoord;
    outMaterialID = getMaterialIdx();
} 
