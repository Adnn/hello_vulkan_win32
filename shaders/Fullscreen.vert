#version 460


layout(location = 1) out vec2 ex_Uv;


const vec2 pos_data[4] = vec2[] (
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

const vec2 tex_data[4] = vec2[] (
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);


void main() 
{
    // Apparently the name is not gl_VertexID when compiling to SPIR-V FOR Vulkan
    ex_Uv = tex_data[gl_VertexIndex];
    gl_Position = vec4(pos_data[gl_VertexIndex], 0.0, 1.0);
}