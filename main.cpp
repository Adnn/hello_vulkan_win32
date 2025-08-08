#ifndef UNICODE
#define UNICODE
#endif 

#include "VulkanLoading.h"
#include "VulkanHelpers.h"
#include "WindowsHelpers.h"

#include <windows.h>

#include <iostream>
#include <vector>

#include <cassert>

VkInstance vkInstance;
VkDevice vkDevice;

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
    // Vulkan dynamic loading and instance initialization
    //
    initializeVulkan();
    vkInstance = createInstance("vulkan_sample", VK_API_VERSION_1_4);
    initializeForInstance(vkInstance);
    
    // TODO: Vulkan stuff
    std::vector<VkPhysicalDevice> physicalDevices = enumeratePhysicalDevices(vkInstance);
    printPhysicalDeviceProperties(vkInstance, physicalDevices);

    // We hardcode using the first physical device and create a logical device
    VkPhysicalDevice vkPhysicalDevice = physicalDevices.front();
    QueueSelection queueSelection = pickQueueFamily(vkInstance, vkPhysicalDevice);
    vkDevice = createDevice(vkInstance, vkPhysicalDevice, queueSelection);
    initializeForDevice(vkDevice);

    // Retrieve the handle to a graphics queue
    VkDeviceQueueInfo2 deviceQueueInfo2{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
        // TODO: handle that family index cleanly
        .queueFamilyIndex = queueSelection.mQueueFamilyIndex,
        // Only 1 queue requested at device creation
        .queueIndex = 0,
    };
    VkQueue vkQueue;
    vkGetDeviceQueue2(vkDevice, &deviceQueueInfo2, &vkQueue);

    // Enumerate layers
    printEnumeratedLayers();

    //
    // Win32: setup a window
    //

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
        //
        // Vulkan clean-up
        //

        // Device
        assertVkSuccess(vkDeviceWaitIdle(vkDevice));
        vkDestroyDevice(vkDevice, pAllocator);

        // Instance
        vkDestroyInstance(vkInstance, pAllocator);

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