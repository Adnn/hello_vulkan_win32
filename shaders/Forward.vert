#version 460


layout(location = 1) in vec3 ve_Position;
//layout(location = 2) in vec2 ve_Uv;

layout(location = 1) out vec2 ex_Uv;

void main() 
{
    ex_Uv = vec2(0);
    gl_Position = vec4(ve_Position, 1.0);
}