#pragma once


#include "VulkanLoading.h"

#include <iostream>

#include <cassert>


static VkAllocationCallbacks * const pAllocator = nullptr;

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


VkInstance createInstance(const char * aAppName, const uint32_t aRequestedApiVersion)
{
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
        .pApplicationName = aAppName,
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = nullptr,
        .engineVersion = 0,
        .apiVersion = aRequestedApiVersion,
    };

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugReportCallbackCreateInfoEXT,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        // TODO: Layers
    };

    VkInstance instance;
    assert(vkCreateInstance(&instanceCreateInfo, pAllocator, &instance) == VK_SUCCESS);

    return instance;
}