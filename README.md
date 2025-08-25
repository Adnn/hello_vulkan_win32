# Hello Vulkan Win32

A _Hello Vulkan_ on Win32 API, without external dependencies (appart from your IHV installable client driver).
Provides custom loader and Window management.

Note: The [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) is an convenient way to obtain the validation layer (not required otherwise).

## Accessing Vulkan API from Windows

For the record, the simple steps taken to access the Vulkan API from Windows:
* Grab Vulkan C headers from https://github.com/KhronosGroup/Vulkan-Headers
* From the code, include `<vulkan/vulkan.h>`, after defining:
  * `VK_NO_PROTOTYPES`, to prevent the headers from declaring functions (allowing to load function ourselve into function pointers)
  * `VK_USE_PLATFORM_WIN32_KHR` to request `vulkan.h` inclusion of win32 specific headers.
* Load `vulkan-1.dll` library (provided by the ICD), via `LoadLibraryEx()`, giving access to `vkGetInstanceProcAddr` entry point via `GetProcAddress()`.


## VS code

Provided with a simple `.vscode` folder for convenient development with Microsoft compiler.

## SPIR-V compilation

    glslang -V -e main -o Forward.vert.spv ../Forward.vert
