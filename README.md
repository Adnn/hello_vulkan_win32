# Hello Vulkan Win32

A _Hello Vulkan_ on Win32 API, without external dependencies (appart from your IHV installable client driver).
Provides custom loader and Window management.

Note: The [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) is an convenient way to obtain the validation layer (not required otherwise).

## VS code

Provided with a simple `.vscode` folder for convenient development with Microsoft compiler.

## SPIR-V compilation

    glslang -V -e main -o Forward.vert.spv ../Forward.vert
