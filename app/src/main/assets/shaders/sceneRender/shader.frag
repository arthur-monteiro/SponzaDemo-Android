layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoords;
layout(location = 2) flat in uint inMaterialID;

layout(location = 0) out vec4 outColor;

void main() 
{
	mat3 tbn = mat3(1.0);
	vec3 worldPos = vec3(0.0);
	MaterialInfo materialInfo = fetchMaterial(inTexCoords, inMaterialID, tbn, worldPos);

	outColor = vec4(materialInfo.albedo.rgb, 1.0);

	//outColor = vec4(inColor, 1.0);
}
