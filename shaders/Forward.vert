#version 460


layout(location = 1) in vec3 ve_Position;
layout(location = 2) in vec3 ve_Color;

layout(location = 1) out vec3 ex_Color;

void main() 
{
    ex_Color = ve_Color;
    gl_Position = vec4(ve_Position, 1.0);
}