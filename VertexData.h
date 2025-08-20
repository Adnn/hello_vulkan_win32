#include <array>

struct Vertex
{
    std::array<float, 3> mPosition;
};

const float f = 0.75f;

constexpr std::array<Vertex, 3> gTriangle{{
    {.mPosition = {-0.866f * f, -0.5f * f, 0.0f}},
    {.mPosition = { 0.866f * f, -0.5f * f, 0.0f}},
    {.mPosition = { 0.0f,        1.0f * f, 0.0f}},
}};