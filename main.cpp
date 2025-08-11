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

    //
    // Vulkan WSI (Window System Integration)
    //
    
    // Create surface for the window
    VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = hInstance,
        .hwnd = hwnd,
    };
    VkSurfaceKHR vkSurface;
    vkCreateWin32SurfaceKHR(vkInstance, &win32SurfaceCreateInfoKHR, pAllocator, &vkSurface);

    // Create swapchain
    VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;
    assertVkSuccess(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities));
    // TODO: ensure the capabilities we are using are indeed available in vkSurfaceCapabilities.

    //TODO: we should compare to a tutorial, there is so much here
    VkSwapchainCreateInfoKHR swapchainCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vkSurface,
        .minImageCount = 2, // Double-buffering, I guess requires 2
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB, // Really, I am guessing at this point
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, // This is where we would HDR
        .imageExtent = vkSurfaceCapabilities.currentExtent,
        .imageArrayLayers = 1,
        // Depth stencil attachment is not a valid usage (thank you validation layer)
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT /*| VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT*/,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // treate the image as opaque when compositing
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // TODO: can we do better? This is the only required mode
        .clipped = VK_FALSE, // let's be safe ATM
        .oldSwapchain = VK_NULL_HANDLE,
    };
    VkSwapchainKHR vkSwapchain;
    assertVkSuccess(vkCreateSwapchainKHR(vkDevice, &swapchainCreateInfoKHR, pAllocator, &vkSwapchain));

    // Get swapchain images. They are fully backed by memory.
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    assertVkSuccess(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImageCount, swapchainImages.data()));
    std::cout << "Swapchain has " << swapchainImageCount << " images.\n\n";

    VkSemaphore acquireSemaphore =VK_NULL_HANDLE;
    VkFence acquireFence;
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    vkCreateFence(vkDevice, &fenceCreateInfo, pAllocator, &acquireFence);

    uint32_t nextImageIndex;
    // TODO: handle window resize (VK_ERROR_OUT_OF_DATE_KHR?)
    assertVkSuccess(vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX/*treated as infinite timeout, 0 would mean not wait allowed*/,
                                          acquireSemaphore, acquireFence, &nextImageIndex));

    // Use synchronization to ensure presentation engine reads have completed on the next image.

    // TODO: Implement the recommended idiom via semaphore instead (from WSI Swapchain):
    // > When the presentable image will be accessed by some stage S, the recommended idiom for ensuring correct synchronization is:
    // TODO: can probably me moved to queue submission
    assertVkSuccess(vkWaitForFences(vkDevice, 1, &acquireFence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(vkDevice, acquireFence, pAllocator);

    VkCommandPool vkCommandPool;
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueSelection.mQueueFamilyIndex,
    };
    vkCreateCommandPool(vkDevice, &commandPoolCreateInfo, pAllocator, &vkCommandPool);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer vkCommandBuffer;
    assertVkSuccess(vkAllocateCommandBuffers(vkDevice, &commandBufferAllocateInfo, &vkCommandBuffer));

    // Move CB to recording state
    VkCommandBufferBeginInfo commandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    assertVkSuccess(vkBeginCommandBuffer(vkCommandBuffer, &commandBufferBeginInfo));

    // TODO: transition to a valid layout
    // > All presentable images are initially in the VK_IMAGE_LAYOUT_UNDEFINED layout, thus before using presentable images, 
    // > the application must transition them to a valid layout for the intended use.

    //vkCmd*

    // Move CB to executable state
    assertVkSuccess(vkEndCommandBuffer(vkCommandBuffer));

    //submit queue
    VkCommandBufferSubmitInfo commandBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = vkCommandBuffer,
    };
    VkSemaphore signalSubmitSemaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    assertVkSuccess(vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, pAllocator, &signalSubmitSemaphore));
    VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = signalSubmitSemaphore,
    };

    VkFence submitFence = VK_NULL_HANDLE;
    VkSubmitInfo2 submitInfo2{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo,
    };
    assertVkSuccess(vkQueueSubmit2(vkQueue, 1, &submitInfo2, submitFence));

    // Present the next image 

    // TODO: use sync to ensure all commands in the specified queue completed before presentation begins
    // >  semaphores must be used to ensure that prior rendering and other commands in the specified queue complete before the presentation begins.
    // TODO: transition to present layout 
    // > Before an application can present an image, the image’s layout must be transitioned to the VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &signalSubmitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &vkSwapchain,
        .pImageIndices = &nextImageIndex,
        .pResults = NULL, // TODO: would it provide more info in the single swapchain situation?
    };
    assertVkSuccess(vkQueuePresentKHR(vkQueue, &presentInfo));

    //
    // Show window and enter main event loop
    //
    {
        ShowWindow(hwnd, nCmdShow);
        //ShowWindow(hwnd, SW_SHOWDEFAULT);

        // Run the message loop.
        MSG msg = { };
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // TODO: move to the window destroy, it is time to make a user ptr
    //
    // Vulkan clean-up
    //

    // semaphore
    vkDestroySemaphore(vkDevice, signalSubmitSemaphore, pAllocator);

    // Command pool and command buffer
    vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer);
    vkDestroyCommandPool(vkDevice, vkCommandPool, pAllocator);

    // Swapchain and surface
    vkDestroySwapchainKHR(vkDevice, vkSwapchain, pAllocator);
    vkDestroySurfaceKHR(vkInstance, vkSurface, pAllocator);
    // Device
    assertVkSuccess(vkDeviceWaitIdle(vkDevice));
    vkDestroyDevice(vkDevice, pAllocator);
    // Instance
    vkDestroyInstance(vkInstance, pAllocator);

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