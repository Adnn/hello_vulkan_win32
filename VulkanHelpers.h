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
    std::cerr << "Vulkan API version: " << toString_version(apiVersion) << "\n\n";

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

    std::vector<const char *> enabledInstanceExtensionNames{
        "VK_EXT_debug_report", // For VkDebugReportCallbackCreateInfoEXT 
        "VK_EXT_debug_utils",
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
    };
    // TODO: we should rely on more data-oriented approaches, such as configurator
    std::vector<const char *> enabledLayerNames{
        // Note: The validation layer is not distributed with the ICD
        // It can notably be installed via Vulkan SDK
        "VK_LAYER_KHRONOS_validation",
    };
    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugReportCallbackCreateInfoEXT,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = (uint32_t)enabledLayerNames.size(),
        .ppEnabledLayerNames = enabledLayerNames.data(),
        .enabledExtensionCount = (uint32_t)enabledInstanceExtensionNames.size(),
        .ppEnabledExtensionNames = enabledInstanceExtensionNames.data(),
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
        std::vector<VkQueueFamilyProperties2> queueFamilyPropertiesVector(
            queueFamilyPropertyCount,
            VkQueueFamilyProperties2{
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
            }
        );
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

            std::cout << "\n\n";
    }
}


void printEnumeratedLayers()
{
    uint32_t layerPropertiesCount;
    vkEnumerateInstanceLayerProperties(&layerPropertiesCount, NULL);
    std::vector<VkLayerProperties> layerPropertiesVector(layerPropertiesCount);
    assertVkSuccess(vkEnumerateInstanceLayerProperties(&layerPropertiesCount, layerPropertiesVector.data()));
    for(const auto & layerProperties : layerPropertiesVector)
    {
        std::cout << "- Layer " << layerProperties.layerName
            << "(" << layerProperties.implementationVersion << ")"
            << ": " << layerProperties.description
            << "\n"
            ;
    }
    std::cout << "\n";
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

    VkPhysicalDeviceShaderObjectFeaturesEXT physicalDeviceShaderObjectFeaturesEXT{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
        .shaderObject = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &physicalDeviceShaderObjectFeaturesEXT,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceQueueCreateInfo deviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = aQueueSelection.mQueueFamilyIndex,
        .queueCount = queueCount,
        .pQueuePriorities = queuePriorities.data(),
    };

    std::vector<const char *>enabledDeviceExtensionNames{
        "VK_EXT_shader_object",
        "VK_KHR_swapchain",
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceVulkan13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = (uint32_t)enabledDeviceExtensionNames.size(),
        .ppEnabledExtensionNames = enabledDeviceExtensionNames.data(),
    };

    VkDevice vkDevice;
    assertVkSuccess(vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, pAllocator, &vkDevice));
    return vkDevice;
}


VkFence createFence(VkDevice vkDevice)
{
    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    assertVkSuccess(vkCreateFence(vkDevice, &fenceCreateInfo, pAllocator, &fence));
    return fence;
}


void setDynamicPipelineState(VkCommandBuffer vkCommandBuffer, VkExtent2D aSurfaceExtent)
{
    // Note: the viewport coordinate system is top-left origin (Y going down),
    // which is opposite to OpenGL.
    // (but VK_KHR_maintenance1 allows to flip that)
    #define VIEWPORT_ORIGIN_BOTTOMLEFT
    #if defined(VIEWPORT_ORIGIN_BOTTOMLEFT)
    // Uses VK_KHR_maintenance1, which allow negative heights to result in Y going up in the viewport
    VkViewport vkViewport{
        .y = (float)aSurfaceExtent.height,
        .width = (float)aSurfaceExtent.width,
        .height = -(float)aSurfaceExtent.height,
        .minDepth = 0,
        .maxDepth = 1,
    };
    #else
    VkViewport vkViewport{
        .width = (float)aSurfaceExtent.width,
        .height = (float)aSurfaceExtent.height,
        .minDepth = 0,
        .maxDepth = 1,
    };
    #endif
    vkCmdSetViewportWithCount(vkCommandBuffer, 1, &vkViewport);

    VkRect2D scissor{
        .extent = aSurfaceExtent,
    };
    vkCmdSetScissorWithCount(vkCommandBuffer, 1, &scissor);

    vkCmdSetRasterizerDiscardEnable(vkCommandBuffer, VK_FALSE);

    vkCmdSetVertexInputEXT(vkCommandBuffer, 0, nullptr, 0, nullptr);

    vkCmdSetPrimitiveTopology(vkCommandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vkCmdSetPrimitiveRestartEnable(vkCommandBuffer, VK_FALSE);

    vkCmdSetRasterizationSamplesEXT(vkCommandBuffer, VK_SAMPLE_COUNT_1_BIT);

    // I assume the mask to be "do I enable this sample"
    // TODO: read about sample mask
    VkSampleMask sampleMask = VK_SAMPLE_COUNT_1_BIT;
    vkCmdSetSampleMaskEXT(vkCommandBuffer, VK_SAMPLE_COUNT_1_BIT, &sampleMask);

    vkCmdSetAlphaToCoverageEnableEXT(vkCommandBuffer, VK_FALSE);

    vkCmdSetPolygonModeEXT(vkCommandBuffer, VK_POLYGON_MODE_FILL);

    vkCmdSetCullMode(vkCommandBuffer, VK_CULL_MODE_BACK_BIT);

    vkCmdSetFrontFace(vkCommandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    vkCmdSetDepthTestEnable(vkCommandBuffer, VK_TRUE);

    vkCmdSetDepthWriteEnable(vkCommandBuffer, VK_TRUE);

    vkCmdSetDepthCompareOp(vkCommandBuffer, VK_COMPARE_OP_LESS);

    vkCmdSetDepthBiasEnable(vkCommandBuffer, VK_FALSE);

    vkCmdSetStencilTestEnable(vkCommandBuffer, VK_FALSE);

    VkColorComponentFlags colorWriteMasks[]{
        {VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
    };
    vkCmdSetColorWriteMaskEXT(vkCommandBuffer, 0, std::size(colorWriteMasks), colorWriteMasks);

    VkBool32 colorBlendEnables[]{
        VK_TRUE,
    };
    vkCmdSetColorBlendEnableEXT(vkCommandBuffer, 0, std::size(colorBlendEnables), colorBlendEnables);

    // TODO: look-up default OpenGL blend parameters
    VkColorBlendEquationEXT colorBlendEquations[]{
        VkColorBlendEquationEXT{
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
        },
    };
    vkCmdSetColorBlendEquationEXT(vkCommandBuffer, 0, std::size(colorBlendEquations), colorBlendEquations);

}

template <class T_handle>
constexpr VkObjectType getObjectType()
{
#define MAP(objectType, handleType) \
    else if(std::is_same_v<T_handle, handleType>) return objectType

    if constexpr(std::is_same_v<T_handle, VkInstance>) return VK_OBJECT_TYPE_INSTANCE;
    MAP(VK_OBJECT_TYPE_INSTANCE, VkInstance);
    MAP(VK_OBJECT_TYPE_PHYSICAL_DEVICE, VkPhysicalDevice);
    MAP(VK_OBJECT_TYPE_DEVICE, VkDevice);
    MAP(VK_OBJECT_TYPE_QUEUE, VkQueue);
    MAP(VK_OBJECT_TYPE_SEMAPHORE, VkSemaphore);
    MAP(VK_OBJECT_TYPE_COMMAND_BUFFER, VkCommandBuffer);
    MAP(VK_OBJECT_TYPE_FENCE, VkFence);
    MAP(VK_OBJECT_TYPE_DEVICE_MEMORY, VkDeviceMemory);
    MAP(VK_OBJECT_TYPE_BUFFER, VkBuffer);
    MAP(VK_OBJECT_TYPE_IMAGE, VkImage);
    MAP(VK_OBJECT_TYPE_EVENT, VkEvent);
    MAP(VK_OBJECT_TYPE_QUERY_POOL, VkQueryPool);
    MAP(VK_OBJECT_TYPE_BUFFER_VIEW, VkBufferView);
    MAP(VK_OBJECT_TYPE_IMAGE_VIEW, VkImageView);
    MAP(VK_OBJECT_TYPE_SHADER_MODULE, VkShaderModule);
    MAP(VK_OBJECT_TYPE_PIPELINE_CACHE, VkPipelineCache);
    MAP(VK_OBJECT_TYPE_PIPELINE_LAYOUT, VkPipelineLayout);
    MAP(VK_OBJECT_TYPE_RENDER_PASS, VkRenderPass);
    MAP(VK_OBJECT_TYPE_PIPELINE, VkPipeline);
    MAP(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, VkDescriptorSetLayout);
    MAP(VK_OBJECT_TYPE_SAMPLER, VkSampler);
    MAP(VK_OBJECT_TYPE_DESCRIPTOR_POOL, VkDescriptorPool);
    MAP(VK_OBJECT_TYPE_DESCRIPTOR_SET, VkDescriptorSet);
    MAP(VK_OBJECT_TYPE_FRAMEBUFFER, VkFramebuffer);
    MAP(VK_OBJECT_TYPE_COMMAND_POOL, VkCommandPool);
    MAP(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION, VkSamplerYcbcrConversion);
    MAP(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, VkDescriptorUpdateTemplate);
    MAP(VK_OBJECT_TYPE_PRIVATE_DATA_SLOT, VkPrivateDataSlot);
    MAP(VK_OBJECT_TYPE_SURFACE_KHR, VkSurfaceKHR);
    MAP(VK_OBJECT_TYPE_SWAPCHAIN_KHR, VkSwapchainKHR);
    MAP(VK_OBJECT_TYPE_DISPLAY_KHR, VkDisplayKHR);
    MAP(VK_OBJECT_TYPE_DISPLAY_MODE_KHR, VkDisplayModeKHR);
    MAP(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT, VkDebugReportCallbackEXT);
    MAP(VK_OBJECT_TYPE_VIDEO_SESSION_KHR, VkVideoSessionKHR);
    MAP(VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR, VkVideoSessionParametersKHR);
    MAP(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT, VkDebugUtilsMessengerEXT);
    MAP(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, VkAccelerationStructureKHR);
    MAP(VK_OBJECT_TYPE_VALIDATION_CACHE_EXT, VkValidationCacheEXT);
    MAP(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV, VkAccelerationStructureNV);
    MAP(VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL, VkPerformanceConfigurationINTEL);
    MAP(VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR, VkDeferredOperationKHR);
    MAP(VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV, VkIndirectCommandsLayoutNV);
    MAP(VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT, VkIndirectCommandsLayoutEXT);
    MAP(VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT, VkIndirectExecutionSetEXT);
    //MAP(VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA, VkBufferCollectionFUCHSIA);
    MAP(VK_OBJECT_TYPE_MICROMAP_EXT, VkMicromapEXT);
    MAP(VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV, VkOpticalFlowSessionNV);
    MAP(VK_OBJECT_TYPE_SHADER_EXT, VkShaderEXT);
    MAP(VK_OBJECT_TYPE_TENSOR_ARM, VkTensorARM);
    MAP(VK_OBJECT_TYPE_TENSOR_VIEW_ARM, VkTensorViewARM);
    MAP(VK_OBJECT_TYPE_DATA_GRAPH_PIPELINE_SESSION_ARM, VkDataGraphPipelineSessionARM);
    else return VK_OBJECT_TYPE_UNKNOWN;

#undef MAP
}

template <class T_handle>
void nameObject(VkDevice vkDevice, T_handle aHandle, const char * aName)
{
    VkDebugUtilsObjectNameInfoEXT  nameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = getObjectType<T_handle>(),
        .objectHandle = reinterpret_cast<uint64_t>(aHandle),
        .pObjectName = aName,
    };
    vkSetDebugUtilsObjectNameEXT(vkDevice, &nameInfo);
}

#define NAME_VKOBJECT(object) nameObject(vkDevice, object, #object);
#define NAME_VKOBJECT_IDX(object, index) nameObject(vkDevice, object, (#object + std::to_string(index)).c_str());