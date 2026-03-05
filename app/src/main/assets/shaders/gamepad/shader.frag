layout(location = 0) in vec2 inPosition;

layout(binding = 0, set = 0, std140) uniform readonly UniformBuffer
{
    vec2 offset;
    vec2 scale;

    vec4 color;
} ub;

layout(location = 0) out vec4 outColor;

void main() 
{
    float alpha = inPosition.x * inPosition.x + inPosition.y * inPosition.y < 1.0 ? 1.0 : 0.0;

	outColor = vec4(ub.color.rgb, alpha * ub.color.a);
}
