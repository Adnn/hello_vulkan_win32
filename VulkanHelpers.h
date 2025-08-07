#pragma once


#include "VulkanLoading.h"

#include <format>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <vector>

#include <cassert>


static VkAllocationCallbacks * const pAllocator = nullptr;

// To be extended with logging of specific errors
void assertVkSuccess(VkResult aResult)
{
    assert(aResult == VK_SUCCESS);
}

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


std::string toString_queueFlags(VkQueueFlags aMask)
{
#define CASE_EXPLICIT(family, flagbit) \
    if ((aMask & flagbit) == flagbit) {oss << #family << ", ";}

#define CASE(family) CASE_EXPLICIT(family, VK_QUEUE_ ## family ## _BIT)

    std::ostringstream oss;

    CASE(GRAPHICS);
    CASE(COMPUTE);
    CASE(TRANSFER);
    CASE(SPARSE_BINDING);
    CASE(PROTECTED);
    CASE_EXPLICIT(VIDEO_DECODE, VK_QUEUE_VIDEO_DECODE_BIT_KHR);
    CASE_EXPLICIT(VIDEO_ENCODE, VK_QUEUE_VIDEO_ENCODE_BIT_KHR);
    CASE_EXPLICIT(OPTICAL_FLOW, VK_QUEUE_OPTICAL_FLOW_BIT_NV);
    CASE_EXPLICIT(DATA_GRAPH,   VK_QUEUE_DATA_GRAPH_BIT_ARM);

    std::string result = oss.str();
    if(!result.empty())
    {
        result.resize(result.size() - 2);
    }
    return result;

#undef CASE
#undef CASE_EXPLICIT
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


std::string toString_version(uint32_t aVersion)
{
    std::ostringstream oss;
    oss << VK_API_VERSION_MAJOR(aVersion) << "."
        << VK_API_VERSION_MINOR(aVersion) << "."
        << VK_API_VERSION_PATCH(aVersion)
        ;
    return oss.str();
}


VkInstance createInstance(const char * aAppName, const uint32_t aRequestedApiVersion)
{
    uint32_t apiVersion;
    assertVkSuccess(vkEnumerateInstanceVersion(&apiVersion));
    // Variant is always 0 for Vulkan API
    // see: https://docs.vulkan.org/spec/latest/chapters/extensions.html#extendingvulkan-coreversions-versionnumbers
    assert(VK_API_VERSION_VARIANT(apiVersion) == 0);
    std::cerr << "Vulkan API version: " << toString_version(apiVersion) << "\n";

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
    assertVkSuccess(vkCreateInstance(&instanceCreateInfo, pAllocator, &instance));

    return instance;
}

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(VkInstance vkInstance)
{
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices{physicalDeviceCount};
    assertVkSuccess(vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices.data()));
    return physicalDevices;
}


void printPhysicalDeviceProperties(VkInstance vkInstance,
                                  std::span<VkPhysicalDevice> aPhysicalDevices)
{
    // Exist for each other version, 12, 13, ...
    VkPhysicalDeviceVulkan11Properties physicalDeviceVulkan11Properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
    };

    // There is a long list of poperties struct that can be passed in the pNext chain
    // to query extra information.
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &physicalDeviceVulkan11Properties,
    };
    for(uint32_t deviceIdx = 0; deviceIdx != aPhysicalDevices.size(); ++deviceIdx)
    {
        const VkPhysicalDevice physical = aPhysicalDevices[deviceIdx];

        vkGetPhysicalDeviceProperties2(physical, &properties);

        uint32_t queueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties2(physical, &queueFamilyPropertyCount, nullptr);
        //std::unique_ptr<VkQueueFamilyProperties2[]> queueFamilyPropertiesArray{
        //    new VkQueueFamilyProperties2[queueFamilyPropertyCount]{/*default construct, i.e. zero init*/}};
        std::vector<VkQueueFamilyProperties2> queueFamilyPropertiesVector{queueFamilyPropertyCount};
        vkGetPhysicalDeviceQueueFamilyProperties2(physical, &queueFamilyPropertyCount, queueFamilyPropertiesVector.data());

        std::cout << "Physical device #" << deviceIdx <<": "
             << properties.properties.deviceName
             << "(" << toString_version(properties.properties.driverVersion) << ")"
             << " supports Vulkan " << toString_version(properties.properties.apiVersion)
             << "\nsupported queues: "
             ;

            for(const VkQueueFamilyProperties2 & queueFamilyProperties2 : queueFamilyPropertiesVector)
                //std::span{queueFamilyPropertiesArray.get(), queueFamilyPropertyCount})
            {
                const VkQueueFamilyProperties & properties = queueFamilyProperties2.queueFamilyProperties;
                std::cout 
                    << "\n\t- " 
                    //<< "(0b" << std::format("{:b}", properties.queueFlags) << ") "
                    << toString_queueFlags(properties.queueFlags) 
                    << ": " << properties.queueCount;
            }

            std::cout << "\n";
    }
}

struct QueueSelection
{
    uint32_t mQueueFamilyIndex;
};

QueueSelection pickQueueFamily(VkInstance vkInstance, VkPhysicalDevice vkPhysicalDevice)
{
    // TODO: compute the queue family index from a list of required queues.
    return QueueSelection{
        .mQueueFamilyIndex = 0,
    };
}

VkDevice createDevice(VkInstance vkInstance,
                      VkPhysicalDevice vkPhysicalDevice,
                      const QueueSelection & aQueueSelection
                      )
{
    const uint32_t queueCount = 1;
    // TODO: assign priorities
    std::vector<float> queuePriorities(queueCount);

    VkDeviceQueueCreateInfo deviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = aQueueSelection.mQueueFamilyIndex,
        .queueCount = queueCount,
        .pQueuePriorities = queuePriorities.data(),
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
    };

    VkDevice vkDevice;
    vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, pAllocator, &vkDevice);
    return vkDevice;
}