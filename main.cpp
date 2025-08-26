#ifndef UNICODE
#define UNICODE
#endif 

#include "FileHelper.h"
#include "VertexData.h"
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
    
    std::vector<VkPhysicalDevice> physicalDevices = enumeratePhysicalDevices(vkInstance);
    printPhysicalDeviceProperties(vkInstance, physicalDevices);

    // We hardcode using the first physical device and create a logical device
    assert(!physicalDevices.empty());
    VkPhysicalDevice vkPhysicalDevice = physicalDevices.front();
    QueueSelection queueSelection = pickQueueFamily(vkInstance, vkPhysicalDevice);
    vkDevice = createDevice(vkInstance, vkPhysicalDevice, queueSelection);
    initializeForDevice(vkDevice);

    // Get physical device properties
    VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };
    vkGetPhysicalDeviceFeatures2(vkPhysicalDevice, &vkPhysicalDeviceFeatures2);
    VkPhysicalDeviceMemoryProperties2 vkPhysicalDeviceMemoryProperties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    };
    // see: https://docs.vulkan.org/spec/latest/chapters/memory.html#vkGetPhysicalDeviceMemoryProperties2
    vkGetPhysicalDeviceMemoryProperties2(vkPhysicalDevice, &vkPhysicalDeviceMemoryProperties2);

    // Get the first memory type index for device local memory
    const VkMemoryPropertyFlags requiredMemoryTypeProperties = 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
        | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        ;
    const VkPhysicalDeviceMemoryProperties & memoryProperties = vkPhysicalDeviceMemoryProperties2.memoryProperties;
    uint32_t deviceLocalMemoryTypeIndex = 0;
    for(; deviceLocalMemoryTypeIndex != memoryProperties.memoryTypeCount; ++deviceLocalMemoryTypeIndex)
    {
        if((memoryProperties.memoryTypes[deviceLocalMemoryTypeIndex].propertyFlags & requiredMemoryTypeProperties)
            == requiredMemoryTypeProperties )
        {
            break; // Found a suitable memory type
        }
    }
    assert(deviceLocalMemoryTypeIndex < memoryProperties.memoryTypeCount);

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
    nameObject(vkDevice, vkQueue, "main_graphics");

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

    
    printSupportedSurfaceFormat(vkPhysicalDevice, vkSurface);

    const VkFormat queueImageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    // Create swapchain
    Swapchain swapchain = prepareSwapchain(vkPhysicalDevice, vkDevice, vkSurface, queueImageFormat);

    //
    // Prepare the commande buffer and all objects required by per-frame operations
    //
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

    // Create semaphores to signal queue completion to image presentation
    // Per-image, otherwise might infringe on VUID-vkQueueSubmit2-semaphore-03868
    std::vector<VkSemaphore> signalSubmitSemaphores(swapchain.swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    for(std::size_t semaphoreIdx= 0; semaphoreIdx != swapchain.swapchainImages.size(); ++semaphoreIdx)
    {
        assertVkSuccess(
            vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, pAllocator, &signalSubmitSemaphores[semaphoreIdx]));
        nameObject(vkDevice, signalSubmitSemaphores[semaphoreIdx],
                   ("signal_QueueSubmit_" + std::to_string(semaphoreIdx)).c_str());
    }

    // Create shader objects
    std::vector<char> vertexCode = readFile("shaders/spirv/Forward.vert.spv");
    std::vector<char> fragmentCode = readFile("shaders/spirv/Color.frag.spv");
    std::vector<VkShaderEXT> vkShaderEXTs = createShaderObjects(vkDevice, vertexCode, fragmentCode);

    // Vertex Attribute Data
    const std::size_t vertexDataSize = sizeof(gTriangle);
    auto [vkVertexBuffer, vkVertexDeviceMemory] = prepareVertexBuffer(vkDevice, vertexDataSize, deviceLocalMemoryTypeIndex);

    // Load the vertex attribute data
    // see: https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-device-hostaccess
    {
        void * vertexDataMap;
        vkMapMemory(vkDevice, vkVertexDeviceMemory, 0, vertexDataSize, 0, &vertexDataMap);
        std::memcpy(vertexDataMap, gTriangle.data(), vertexDataSize);
        vkUnmapMemory(vkDevice, vkVertexDeviceMemory);
        // No need to call vkFlushMappedMemoryRanges() since we picked coherent memory

        // Note: afaiu, host writes to mappable device memory are made available to all memory accesses performed by the device
        // in commands from a subsequent queue submission.
        // see access scope in: https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-submission-host-writes
    }

    //
    // Render Pass Object (used when not going through dynamic rendering)
    //
    VkRenderPass vkRenderPass;
    {
        VkAttachmentDescription attachmentDescription{
            .format = queueImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            // TODO: are those encompassing the layout transitions?
            // The transition should apparently occur before, due to: VUID-VkAttachmentReference-layout-03077
            .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkAttachmentReference colorAttachmentReference{
            .attachment = 0, // Idx of the attachment in renderPassCreateInfo
            // TODO: Does the first transition happen before the subpass?
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkAttachmentReference dephtStencilAttachmentReference{
            .attachment = VK_ATTACHMENT_UNUSED,
        };
        VkSubpassDescription subpassDescription{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, 
            .inputAttachmentCount = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentReference,
            .pResolveAttachments = NULL,
            .pDepthStencilAttachment = &dephtStencilAttachmentReference,
            .preserveAttachmentCount = 0,
        };

        VkRenderPassCreateInfo renderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 0,
            .pDependencies = NULL,
        };
        vkCreateRenderPass(vkDevice, &renderPassCreateInfo, pAllocator, &vkRenderPass);
    }

    // Framebuffer
    std::vector<VkFramebuffer> framebuffers;//(swapchain.renderImageViews.size());
    {
        VkFramebufferCreateInfo framebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vkRenderPass,
            .attachmentCount = 1,
            .width = swapchain.imageExtent.width,
            .height = swapchain.imageExtent.height,
            .layers = 1,
        };

        for(VkImageView imageView : swapchain.renderImageViews)
        {
            framebufferCreateInfo.pAttachments = &imageView;
            framebuffers.emplace_back();
            vkCreateFramebuffer(vkDevice, &framebufferCreateInfo, pAllocator, &framebuffers.back());
        }
    }


    // Graphics Pipeline
    VkPipeline vkPipeline = createStaticPipeline(vkDevice, swapchain, vkRenderPass, vertexCode, fragmentCode);
    

    //
    // Show window and enter main event loop
    //
    {
        ShowWindow(hwnd, nCmdShow);
        //ShowWindow(hwnd, SW_SHOWDEFAULT);

        // Run the message loop.
        MSG msg{};
        while(msg.message != WM_QUIT)
        {
            if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else 
            {
                // Notably handle window resizing
                if(swapchain.mOutOfDate)
                {
                    // Wait until all queue submission commands have completed,
                    // guaranteeing the submit-semaphore have been signaled
                    assertVkSuccess(vkQueueWaitIdle(vkQueue));
                    swapchain.destroy();
                    swapchain = prepareSwapchain(vkPhysicalDevice, vkDevice, vkSurface, queueImageFormat/*, swapchain.vkSwapchain*/);
                }

                // 
                // TICK
                //
                //VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
                VkSemaphore acquireSemaphore = createSemaphore(vkDevice, "acquire_image");
                VkFence acquireFence = VK_NULL_HANDLE;
                //VkFence acquireFence;
                //VkFenceCreateInfo fenceCreateInfo{
                //    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                //};
                //assertVkSuccess(vkCreateFence(vkDevice, &fenceCreateInfo, pAllocator, &acquireFence));

                uint32_t nextImageIndex;
                // TODO: handle window resize (VK_ERROR_OUT_OF_DATE_KHR?)
                assertVkSuccess(vkAcquireNextImageKHR(vkDevice, swapchain.vkSwapchain, UINT64_MAX/*treated as infinite timeout, 0 would mean not wait allowed*/,
                                                      acquireSemaphore, acquireFence, &nextImageIndex));

                VkImage nextImage = swapchain.swapchainImages[nextImageIndex];

                if(acquireFence != VK_NULL_HANDLE)
                {
                    // Use synchronization to ensure presentation engine reads have completed on the next image.
                    // Note: Replaced with the recommended idiom via semaphore instead (from WSI Swapchain):
                    // > When the presentable image will be accessed by some stage S, the recommended idiom for ensuring correct synchronization is:
                    assertVkSuccess(vkWaitForFences(vkDevice, 1, &acquireFence, VK_TRUE, UINT64_MAX));
                    vkDestroyFence(vkDevice, acquireFence, pAllocator);
                }

                // Move CB to recording state
                VkCommandBufferBeginInfo commandBufferBeginInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };
                assertVkSuccess(vkBeginCommandBuffer(vkCommandBuffer, &commandBufferBeginInfo));

                // Transition to general layout (initializing from undefined layout)
                // > All presentable images are initially in the VK_IMAGE_LAYOUT_UNDEFINED layout, thus before using presentable images, 
                // > the application must transition them to a valid layout for the intended use.
                VkImageMemoryBarrier2 imageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                    // TODO: understand which stages should appear here
                    // I followed the note (but probably missunderstood it)
                    // > When the presentable image will be accessed by some stage S, the recommended idiom for ensuring correct synchronization is:
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = nextImage,
                    .subresourceRange = gSwapchainImageFullRange,
                };
                VkDependencyInfo dependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers  = &imageMemoryBarrier2,
                };
                vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

                // Clear color image
                VkClearColorValue clearColor{
                    .float32{0.1f, 0.1f, 0.1f, 1.0f},
                };
                vkCmdClearColorImage(vkCommandBuffer,
                                     nextImage,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     &clearColor,
                                     1,
                                     &gSwapchainImageFullRange);

                // Draw
                const VkRect2D renderArea{
                    .offset = VkOffset2D{0, 0},
                    .extent = swapchain.imageExtent,
                };
                bool dynamic = false;
                if(dynamic)
                {
                    VkRenderingAttachmentInfo renderingColorAttachmentInfo{
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = swapchain.renderImageViews[nextImageIndex],
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    };
                    VkRenderingInfo renderingInfo{
                        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                        .renderArea = renderArea,
                        .layerCount = 1,
                        .colorAttachmentCount = 1,
                        .pColorAttachments = &renderingColorAttachmentInfo,
                    };
                    vkCmdBeginRendering(vkCommandBuffer, &renderingInfo);

                    // Bind shader objects
                    const VkShaderStageFlagBits stageBits[]{
                        VK_SHADER_STAGE_VERTEX_BIT,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    };
                    vkCmdBindShadersEXT(vkCommandBuffer, vkShaderEXTs.size(), stageBits, vkShaderEXTs.data());

                    // Very explicit required state
                    setDynamicPipelineState(vkCommandBuffer, swapchain.imageExtent);

                    // Vertex Attributes

                    // Vertex shaders defines *input variables* which receive *vertex attribute* data.
                    // Binding model:
                    // shaders *input variables* are associated to a *vertex input attribute* number (per-shader)
                    //  - in GLSL, this number is given with `location` layout qualifier,
                    //    - `component` layout qualifier associate components of a shader *input variable* with component of a *vertex input attribute*
                    //  - *vertex input attribute* is given a number with `VkVertexInputAttributeDescription::location` (see below)
                    // *vertex input attributes* are associated to *vertex input bindings* (per-pipeline)
                    //  - `vkCmdSetVertexInputEXT()`
                    //    - `VkVertexInputBindingDescription2EXT` 
                    //      - defines the *binding* number
                    //      - stride between consecutive elements (within the buffer)
                    //    - `VkVertexInputAttributeDescription2EXT` 
                    //      - associates a shader *input variable* location to a *binding* number
                    //      - format of the *vertex attribute* data
                    //      - offset relative to the start of an element in the *vertex input binding* (e.g. interleaved, with several attribute from the same binding)
                    // *vertex input bindings* are associated with specific *buffers* (per-draw)
                    //  - vkCmdBindVertexBuffers()

                    // Vertex shader input from buffers
                    // Describe the single binding, used by both interleaved attributes
                    VkVertexInputBindingDescription2EXT vertexInputBindingDescriptions[]{
                        {
                            .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
                            .binding = 1,
                            .stride = sizeof(Vertex),
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                            .divisor = 1,
                        },
                    };
                    // Describes both attributes that will be pulled from the single binding
                    VkVertexInputAttributeDescription2EXT vertexInputAttributeDescription[]{
                        {
                            .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                            .location = 1,
                            .binding = 1,
                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                            .offset = 0,
                        },
                        {
                            .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                            .location = 2,
                            .binding = 1,
                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                            .offset = offsetof(Vertex, mColor),
                        },
                    };
                    vkCmdSetVertexInputEXT(vkCommandBuffer,
                                           std::size(vertexInputBindingDescriptions), vertexInputBindingDescriptions,
                                           std::size(vertexInputAttributeDescription), vertexInputAttributeDescription);

                    // Associate the vertex input bindings to buffers (per-draw)
                    VkDeviceSize vertexBufferOffset = 0;
                    vkCmdBindVertexBuffers(vkCommandBuffer, 1, 1, &vkVertexBuffer, &vertexBufferOffset);

                    // Non-indexed draw
                    vkCmdDraw(vkCommandBuffer, 3, 1, 0, 0);

                    vkCmdEndRendering(vkCommandBuffer);
                }
                else
                {
                    VkRenderPassBeginInfo renderPassBeginInfo{
                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        .renderPass = vkRenderPass,
                        .framebuffer = framebuffers[nextImageIndex],
                        .renderArea = renderArea,
                        .clearValueCount = 0,
                        .pClearValues = NULL,
                    };
                    vkCmdBeginRenderPass(vkCommandBuffer, &renderPassBeginInfo, 
                                         // The content of the first subpass will be recorded inline in the primary command buffer
                                         VK_SUBPASS_CONTENTS_INLINE);

                    vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

                    // Associate the vertex input bindings to buffers (per-draw)
                    VkDeviceSize vertexBufferOffset = 0;
                    vkCmdBindVertexBuffers(vkCommandBuffer, 1, 1, &vkVertexBuffer, &vertexBufferOffset);

                    // Non-indexed draw
                    vkCmdDraw(vkCommandBuffer, 3, 1, 0, 0);

                    vkCmdEndRenderPass(vkCommandBuffer);
                }

                // Transition to presentation layout
                // > Before an application can present an image, the image’s layout must be transitioned to the VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                imageMemoryBarrier2.oldLayout = imageMemoryBarrier2.newLayout;
                imageMemoryBarrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);

                // Move CB to executable state
                assertVkSuccess(vkEndCommandBuffer(vkCommandBuffer));

                //submit queue
                VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = acquireSemaphore,
                    .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                };

                VkCommandBufferSubmitInfo commandBufferSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = vkCommandBuffer,
                };

                VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = signalSubmitSemaphores[nextImageIndex],
                };

                //VkFence submitFence = VK_NULL_HANDLE;
                VkFence submitFence = createFence(vkDevice);

                VkSubmitInfo2 submitInfo2{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .waitSemaphoreInfoCount = 1,
                    .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
                    .commandBufferInfoCount = 1,
                    .pCommandBufferInfos = &commandBufferSubmitInfo,
                    .signalSemaphoreInfoCount = 1,
                    .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo,
                };
                assertVkSuccess(vkQueueSubmit2(vkQueue, 1, &submitInfo2, submitFence));

                // Present the next image 

                // Note: signalSubmitSemaphores address the requirement below:
                // >  semaphores must be used to ensure that prior rendering and other commands in the specified queue complete before the presentation begins.
                VkPresentInfoKHR presentInfo{
                    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &signalSubmitSemaphores[nextImageIndex],
                    .swapchainCount = 1,
                    .pSwapchains = &swapchain.vkSwapchain,
                    .pImageIndices = &nextImageIndex,
                    .pResults = NULL, // TODO: would it provide more info in the single swapchain situation?
                };
                VkResult result = vkQueuePresentKHR(vkQueue, &presentInfo);
                if(result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    swapchain.mOutOfDate = true;
                }
                else
                {
                    assertVkSuccess(result);
                }

                // Trivial sync, to ensure that the active command buffer has completed (not pending)
                // before we call Begin on next frame.
                assertVkSuccess(vkWaitForFences(vkDevice, 1, &submitFence, VK_TRUE, UINT64_MAX));
                vkDestroyFence(vkDevice, submitFence, pAllocator);

                vkDestroySemaphore(vkDevice, acquireSemaphore, pAllocator);
            }

        }
    }

    // TODO: move to the window destroy, it is time to make a user ptr
    //
    // Vulkan clean-up
    //

    // Pipeline
    vkDestroyPipeline(vkDevice, vkPipeline, pAllocator);

    // Render pass and framebuffers
    for(VkFramebuffer framebuffer : framebuffers)
    {
        vkDestroyFramebuffer(vkDevice, framebuffer, pAllocator);
    }
    vkDestroyRenderPass(vkDevice, vkRenderPass, pAllocator);

    // Buffers
    vkFreeMemory(vkDevice, vkVertexDeviceMemory, pAllocator);
    vkDestroyBuffer(vkDevice, vkVertexBuffer, pAllocator);

    // Shader objects
    for(VkShaderEXT shader : vkShaderEXTs)
    {
        vkDestroyShaderEXT(vkDevice, shader, pAllocator);
    }

    // Semaphores
    assertVkSuccess(vkQueueWaitIdle(vkQueue));
    for(VkSemaphore semaphore : signalSubmitSemaphores)
    {
        vkDestroySemaphore(vkDevice, semaphore, pAllocator);
    }

    // Command pool and command buffer
    vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer);
    vkDestroyCommandPool(vkDevice, vkCommandPool, pAllocator);

    // Swapchain and surface
    swapchain.destroy();
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