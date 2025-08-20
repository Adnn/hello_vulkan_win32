#pragma once


// VK_NO_PROTOTYPES prevent vulkan headers to declare function
// (that would be statisfied by linking against the Vulkan SDK loader .lib)
// This way we can declare all functions as variable that we will assign ourselves
#define VK_NO_PROTOTYPES
// Required to control vulkan.h as to include win32 specific header 
// (notably declaring VK_KHR_win32_surface symbols) 
// see: https://docs.vulkan.org/spec/latest/appendices/boilerplate.html#boilerplate-wsi-header-table
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "WindowsHelpers.h"

#include <windows.h>


// Functions prototypes, as function pointers
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define D(functionName) PFN_ ## functionName functionName
    D(vkGetDeviceProcAddr);
    D(vkEnumerateInstanceVersion);
    D(vkCreateInstance);
    D(vkEnumerateInstanceLayerProperties);

    D(vkDestroyInstance);
    D(vkEnumeratePhysicalDevices);
    D(vkGetPhysicalDeviceProperties2);
    D(vkGetPhysicalDeviceFeatures2);
    D(vkGetPhysicalDeviceMemoryProperties2);
    D(vkGetPhysicalDeviceQueueFamilyProperties2);
    D(vkCreateDevice);
    D(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    // VK_KHR_surface
    D(vkDestroySurfaceKHR);
    // VK_KHR_win32_surface
    D(vkCreateWin32SurfaceKHR);

    //
    // Device functions
    //
    D(vkDestroyDevice);
    D(vkDeviceWaitIdle);
    D(vkGetDeviceQueue2);
    D(vkCreateFence);
    D(vkDestroyFence);
    D(vkWaitForFences);
    D(vkCreateSemaphore);
    D(vkDestroySemaphore);
    D(vkCreateCommandPool);
    D(vkDestroyCommandPool);
    D(vkAllocateCommandBuffers);
    D(vkFreeCommandBuffers);
    D(vkBeginCommandBuffer);
    D(vkEndCommandBuffer);
    D(vkCmdPipelineBarrier2);
    D(vkCmdClearColorImage);
    D(vkQueueWaitIdle);
    D(vkCmdBeginRendering);
    D(vkCmdEndRendering);
    D(vkCreateImageView);
    D(vkDestroyImageView);
    D(vkCmdDraw);
    D(vkCmdSetViewportWithCount);
    D(vkCmdSetScissorWithCount);
    D(vkCmdSetRasterizerDiscardEnable);
    D(vkCmdSetPrimitiveTopology);
    D(vkCmdSetPrimitiveRestartEnable);
    D(vkCmdSetRasterizationSamplesEXT);
    D(vkCmdSetSampleMaskEXT);
    D(vkCmdSetAlphaToCoverageEnableEXT);
    D(vkCmdSetAlphaToOneEnableEXT);
    D(vkCmdSetPolygonModeEXT);
    D(vkCmdSetLineWidth);
    D(vkCmdSetCullMode);
    D(vkCmdSetFrontFace);
    D(vkCmdSetDepthTestEnable);
    D(vkCmdSetDepthWriteEnable);
    D(vkCmdSetDepthCompareOp);
    D(vkCmdSetDepthBoundsTestEnable);
    D(vkCmdSetDepthBounds);
    D(vkCmdSetDepthBiasEnable);
    D(vkCmdSetDepthBias);
    D(vkCmdSetDepthClampEnableEXT);
    D(vkCmdSetStencilTestEnable);
    D(vkCmdSetStencilOp);
    D(vkCmdSetStencilCompareMask);
    D(vkCmdSetStencilWriteMask);
    D(vkCmdSetStencilReference);
    D(vkCmdSetLogicOpEnableEXT);
    D(vkCmdSetLogicOpEXT);
    D(vkCmdSetColorWriteMaskEXT);
    D(vkCmdSetColorBlendEnableEXT);
    D(vkCmdSetColorBlendEquationEXT);
    D(vkCmdSetBlendConstants);
    D(vkCreateBuffer);
    D(vkDestroyBuffer);
    D(vkAllocateMemory);
    D(vkFreeMemory);
    D(vkGetBufferMemoryRequirements);
    D(vkBindBufferMemory);
    D(vkMapMemory);
    D(vkUnmapMemory);

    // VK_KHR_swapchain
    D(vkCreateSwapchainKHR);
    D(vkDestroySwapchainKHR);
    D(vkGetSwapchainImagesKHR);
    D(vkAcquireNextImageKHR);
    D(vkQueuePresentKHR);
    D(vkQueueSubmit2);
    // VK_EXT_debug_utils
    D(vkSetDebugUtilsObjectNameEXT);
    // VK_EXT_shader_object
    D(vkCreateShadersEXT);
    D(vkDestroyShaderEXT);
    D(vkCmdBindShadersEXT);
    D(vkCmdSetVertexInputEXT);
    D(vkCmdBindVertexBuffers);
#undef D


void initializeVulkan()
{

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

#define D(functionName) \
    functionName = reinterpret_cast<PFN_ ## functionName>( \
        vkGetInstanceProcAddr(nullptr, #functionName))

    D(vkEnumerateInstanceLayerProperties);
#undef D
}


void initializeForInstance(VkInstance aInstance)
{
    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        vkGetInstanceProcAddr(aInstance, "vkDestroyInstance"));

#define D(functionName) \
    functionName = reinterpret_cast<PFN_ ## functionName>( \
        vkGetInstanceProcAddr(aInstance, #functionName))

    // TODO: should vkGetDeviceProcAddr be scoped to the device somehow? (like re-assigning itself)
    D(vkGetDeviceProcAddr);
    D(vkEnumeratePhysicalDevices);
    D(vkGetPhysicalDeviceProperties2);
    D(vkGetPhysicalDeviceFeatures2);
    D(vkGetPhysicalDeviceMemoryProperties2);
    D(vkGetPhysicalDeviceQueueFamilyProperties2);
    D(vkCreateDevice);
    D(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

    D(vkDestroySurfaceKHR);

    D(vkCreateWin32SurfaceKHR);

#undef D
}


void initializeForDevice(VkDevice aDevice)
{
    vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        vkGetDeviceProcAddr(aDevice, "vkDestroyDevice"));

#define D(functionName) \
    functionName = reinterpret_cast<PFN_ ## functionName>( \
        vkGetDeviceProcAddr(aDevice, #functionName))

    D(vkDeviceWaitIdle);
    D(vkGetDeviceQueue2);
    D(vkCreateFence);
    D(vkDestroyFence);
    D(vkWaitForFences);
    D(vkCreateSemaphore);
    D(vkDestroySemaphore);
    D(vkCreateCommandPool);
    D(vkDestroyCommandPool);
    D(vkAllocateCommandBuffers);
    D(vkFreeCommandBuffers);
    D(vkBeginCommandBuffer);
    D(vkEndCommandBuffer);
    D(vkCmdPipelineBarrier2);
    D(vkCmdClearColorImage);
    D(vkQueueWaitIdle);
    D(vkCmdBeginRendering);
    D(vkCmdEndRendering);
    D(vkCreateImageView);
    D(vkDestroyImageView);
    D(vkCmdDraw);
    D(vkCmdSetViewportWithCount);
    D(vkCmdSetScissorWithCount);
    D(vkCmdSetRasterizerDiscardEnable);
    D(vkCmdSetPrimitiveTopology);
    D(vkCmdSetPrimitiveRestartEnable);
    D(vkCmdSetRasterizationSamplesEXT);
    D(vkCmdSetSampleMaskEXT);
    D(vkCmdSetAlphaToCoverageEnableEXT);
    D(vkCmdSetAlphaToOneEnableEXT);
    D(vkCmdSetPolygonModeEXT);
    D(vkCmdSetLineWidth);
    D(vkCmdSetCullMode);
    D(vkCmdSetFrontFace);
    D(vkCmdSetDepthTestEnable);
    D(vkCmdSetDepthWriteEnable);
    D(vkCmdSetDepthCompareOp);
    D(vkCmdSetDepthBoundsTestEnable);
    D(vkCmdSetDepthBounds);
    D(vkCmdSetDepthBiasEnable);
    D(vkCmdSetDepthBias);
    D(vkCmdSetDepthClampEnableEXT);
    D(vkCmdSetStencilTestEnable);
    D(vkCmdSetStencilOp);
    D(vkCmdSetStencilCompareMask);
    D(vkCmdSetStencilWriteMask);
    D(vkCmdSetStencilReference);
    D(vkCmdSetLogicOpEnableEXT);
    D(vkCmdSetLogicOpEXT);
    D(vkCmdSetColorWriteMaskEXT);
    D(vkCmdSetColorBlendEnableEXT);
    D(vkCmdSetColorBlendEquationEXT);
    D(vkCmdSetBlendConstants);
    D(vkCreateBuffer);
    D(vkDestroyBuffer);
    D(vkAllocateMemory);
    D(vkFreeMemory);
    D(vkGetBufferMemoryRequirements);
    D(vkBindBufferMemory);
    D(vkMapMemory);
    D(vkUnmapMemory);

    D(vkCreateSwapchainKHR);
    D(vkDestroySwapchainKHR);
    D(vkGetSwapchainImagesKHR);
    D(vkAcquireNextImageKHR);
    D(vkQueuePresentKHR);
    D(vkQueueSubmit2);

    D(vkSetDebugUtilsObjectNameEXT);

    D(vkCreateShadersEXT);
    D(vkDestroyShaderEXT);
    D(vkCmdBindShadersEXT);
    D(vkCmdSetVertexInputEXT);
    D(vkCmdBindVertexBuffers);
#undef D
}