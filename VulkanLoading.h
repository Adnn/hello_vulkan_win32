#pragma once


// VK_NO_PROTOTYPES prevent vulkan headers to declare function
// (that would be statisfied by linking against the Vulkan SDK loader .lib)
// This way we can declare all functions as variable that we will assign ourselves
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "WindowsHelpers.h"

#include <windows.h>


// Functions prototypes, as function pointers
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define D(functionName) PFN_ ## functionName functionName
    D(vkGetDeviceProcAddr);
    D(vkEnumerateInstanceVersion);
    D(vkCreateInstance);
    D(vkDestroyInstance);
    D(vkEnumeratePhysicalDevices);
    D(vkGetPhysicalDeviceProperties2);
    D(vkGetPhysicalDeviceFeatures2);
    D(vkGetPhysicalDeviceQueueFamilyProperties2);
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
    D(vkGetPhysicalDeviceQueueFamilyProperties2);

#undef D
}