layout(location = 0) in vec2 inPosition;

layout(binding = 0, set = 0, std140) uniform readonly UniformBuffer
{
    vec2 offset;
    vec2 scale;

    vec4 color;
} ub;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 outPos;

void main() 
{
    gl_Position = vec4(inPosition * ub.scale + ub.offset, 0.0, 1.0);

    outPos = inPosition;
} 
