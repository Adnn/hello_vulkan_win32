#ifndef UNICODE
#define UNICODE
#endif 

#include "WindowsHelpers.h"

// VK_NO_PROTOTYPES prevent vulkan headers to declare function
// (that would be statisfied by linking against the Vulkan SDK loader .lib)
// This way we can declare all functions as variable that we will assign ourselves
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <windows.h>

#include <iostream>

#include <cassert>

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define DECL(functionName) PFN_ ## functionName functionName
DECL(vkEnumerateInstanceVersion);
DECL(vkCreateInstance);
DECL(vkDestroyInstance);
#undef DECL

/// @brief Converts an enumerator from VkDebugReportFlagBitsEXT to c-string
const char * toString_debugReportFlagBits(VkDebugReportFlagsEXT aBit)
{
#define CASE(enumerator) \
    case VK_DEBUG_REPORT_ ## enumerator ## _BIT_EXT: return #enumerator

    switch(aBit)
    {
        CASE(INFORMATION);
        CASE(WARNING);
        CASE(PERFORMANCE_WARNING);
        CASE(ERROR);
        CASE(DEBUG);
        default: return "<unknown>";
    }
#undef CASE
}


/// @brief Callback for instance creation
VkBool32 VKAPI_PTR debugReportCallback(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char*                                 pLayerPrefix,
    const char*                                 pMessage,
    void*                                       pUserData)
{
    std::cerr << "(debug_report) " 
        << "[" << toString_debugReportFlagBits(flags) << "] "
        << pLayerPrefix << ": " << pMessage
        << "\n"
        ;
    return VK_FALSE; // Vulkan API requirement
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

//#define IS_CONSOLE
#if defined(IS_CONSOLE)
int WINAPI main(int argc, char * argv[])
#else
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
#endif
{
    WCHAR * programPtr;
    if(errno_t error = _get_wpgmptr(&programPtr); error != 0)
    {
        return error;
    }
    std::wcout << "Launching " << programPtr << std::endl;

#if defined(IS_CONSOLE)
    HINSTANCE hInstance = GetModuleHandle(NULL);
    STARTUPINFO si;
    GetStartupInfo(&si);
    // This is not equivalent to the nCmdShow passed to wWinMain
    int nCmdShow = si.wShowWindow;
#endif

    //
    // Vulkan dynamic loading
    //

    // Load the IHV provided Vulkan loader as a dynamic library
    HMODULE vulkanModule = LoadLibraryEx(TEXT("vulkan-1.dll"), NULL, 0);
    require(vulkanModule);
    // Load the Vulkan entry-point, the function to load other Vulkan functions.
    // see: https://docs.vulkan.org/spec/latest/chapters/initialization.html#initialization-functionpointers
    FARPROC entry = GetProcAddress(vulkanModule, "vkGetInstanceProcAddr");
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(entry);

    // Use vkGetInstanceProcAddr to load required Vulkan functions
    vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        vkGetInstanceProcAddr(nullptr, "vkDestroyInstance"));

    //#define GETPROC(functionName)
    //functionName = reinterpret_cast<PFN_ ## functionName>(
    //    vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));


    //
    // Vulkan instance initialization
    //

    uint32_t apiVersion;
    assert(vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS);
    // Variant is always 0 for Vulkan API
    // see: https://docs.vulkan.org/spec/latest/chapters/extensions.html#extendingvulkan-coreversions-versionnumbers
    assert(VK_API_VERSION_VARIANT(apiVersion) == 0);
    std::cerr << "Vulkan API version: " 
        << VK_API_VERSION_MAJOR(apiVersion) << "."
        << VK_API_VERSION_MINOR(apiVersion) << "."
        << VK_API_VERSION_PATCH(apiVersion)
        << "\n";

    const uint32_t requestApiVersion = VK_API_VERSION_1_4;
    assert(apiVersion >= requestApiVersion);

    VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags =
            VK_DEBUG_REPORT_INFORMATION_BIT_EXT
            | VK_DEBUG_REPORT_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
            | VK_DEBUG_REPORT_ERROR_BIT_EXT 
            | VK_DEBUG_REPORT_DEBUG_BIT_EXT,
        .pfnCallback = &debugReportCallback,
        .pUserData = nullptr,
    };

    VkApplicationInfo applicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "vulkan_sample",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = nullptr,
        .engineVersion = 0,
        .apiVersion = requestApiVersion,
    };

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugReportCallbackCreateInfoEXT,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        // TODO: Layers
    };

    VkAllocationCallbacks * pAllocator = nullptr;

    VkInstance instance;
    assert(vkCreateInstance(&instanceCreateInfo, pAllocator, &instance) == VK_SUCCESS);
    

    // Register the window class.
    const wchar_t CLASS_NAME[]  = L"ad_vulkan_window";
    
    WNDCLASS wc = { }; // All entries will be initialized to zero
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Vulkan sample",               // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
        );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    //ShowWindow(hwnd, SW_SHOWDEFAULT);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // All painting occurs here, between BeginPaint and EndPaint.
            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
            EndPaint(hwnd, &ps);
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}