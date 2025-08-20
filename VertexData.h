#include <array>

struct Vertex
{
    std::array<float, 3> mPosition;
    std::array<float, 3> mColor;
};

const float f = 0.75f;

constexpr std::array<Vertex, 3> gTriangle{{
    {.mPosition = {-0.866f * f, -0.5f * f, 0.0f}, .mColor = {0.0f, 1.0f, 0.0f}},
    {.mPosition = { 0.866f * f, -0.5f * f, 0.0f}, .mColor = {0.0f, 0.0f, 1.0f}},
    {.mPosition = { 0.0f,        1.0f * f, 0.0f}, .mColor = {1.0f, 0.0f, 0.0f}},
}};