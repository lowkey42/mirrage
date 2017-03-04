#include <stdio.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

void vulkan_main(SDL_Window *window) {
    VkResult err;

    VkInstance inst;
    {
        uint32_t extension_count = 0;
        const char *extension_names[64];
        extension_names[extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;

        const VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "SDL_vulkan example",
            .apiVersion = VK_MAKE_VERSION(1, 0, 3),
        };

        {
            unsigned c = 64 - extension_count;
            if (!SDL_GetVulkanInstanceExtensions(&c, &extension_names[extension_count])) {
                fprintf(stderr, "SDL_GetVulkanInstanceExtensions failed: %s\n", SDL_GetError());
                exit(1);
            }
            extension_count += c;
        }

        VkInstanceCreateInfo inst_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = extension_names,
        };

        err = vkCreateInstance(&inst_info, NULL, &inst);
        assert(!err);
    }

    VkSurfaceKHR surface;
    if (!SDL_CreateVulkanSurface(window, inst, &surface)) {
        fprintf(stderr, "SDL_CreateVulkanSurface failed: %s\n", SDL_GetError());
        exit(1);
    }

    VkPhysicalDevice gpu;
    {
        uint32_t gpu_count;
        err = vkEnumeratePhysicalDevices(inst, &gpu_count, NULL);
        assert(!err && gpu_count > 0);

        if (gpu_count > 0) {
            VkPhysicalDevice gpus[gpu_count];
            err = vkEnumeratePhysicalDevices(inst, &gpu_count, gpus);
            assert(!err);
            gpu = gpus[0];
        } else {
            gpu = VK_NULL_HANDLE;
        }
    }

    VkDevice device;
    VkQueue queue;
    VkCommandPool cmd_pool;
    {
        uint32_t queue_family_index = UINT32_MAX;
        {
            uint32_t queue_count;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, NULL);
            VkQueueFamilyProperties queue_props[queue_count];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, queue_props);
            assert(queue_count >= 1);

            for (uint32_t i = 0; i < queue_count; i++) {
                VkBool32 supports_present;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supports_present);
                if (supports_present && (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                    queue_family_index = i;
                    break;
                }
            }
            assert(queue_family_index != UINT32_MAX);
        }

        uint32_t extension_count = 0;
        const char *extension_names[64];
        extension_count = 0;
        extension_names[extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

        float queue_priorities[1] = {0.0};
        const VkDeviceQueueCreateInfo queueInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = queue_priorities};

        VkDeviceCreateInfo deviceInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueInfo,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = (const char *const *)extension_names,
        };

        err = vkCreateDevice(gpu, &deviceInfo, NULL, &device);
        assert(!err);

        vkGetDeviceQueue(device, queue_family_index, 0, &queue);

        const VkCommandPoolCreateInfo cmd_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue_family_index,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        err = vkCreateCommandPool(device, &cmd_pool_info, NULL, &cmd_pool);
        assert(!err);
    }

    VkFormat           format;
    VkColorSpaceKHR    color_space;
    {
        uint32_t format_count;
        err = vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, NULL);
        assert(!err);

        VkSurfaceFormatKHR formats[format_count];
        err = vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count, formats);
        assert(!err);

        if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            format = VK_FORMAT_B8G8R8A8_SRGB;
        } else {
            assert(format_count >= 1);
            format = formats[0].format;
        }
        color_space = formats[0].colorSpace;
    }

    VkCommandBuffer draw_cmd;
    {
        const VkCommandBufferAllocateInfo cmd = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        err = vkAllocateCommandBuffers(device, &cmd, &draw_cmd);
        assert(!err);
    }

    VkSwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    VkExtent2D swapchain_extent;
    {
        VkSurfaceCapabilitiesKHR surf_cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surf_cap);
        assert(!err);

        if (surf_cap.currentExtent.width == (uint32_t)-1) {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            swapchain_extent.width = (uint32_t)w;
            swapchain_extent.height = (uint32_t)h;
        } else {
            swapchain_extent = surf_cap.currentExtent;
        }

        swapchain_image_count = surf_cap.minImageCount + 1;
        if ((surf_cap.maxImageCount > 0) && (swapchain_image_count > surf_cap.maxImageCount)) {
            swapchain_image_count = surf_cap.maxImageCount;
        }

        const VkSwapchainCreateInfoKHR swapchainInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = swapchain_image_count,
            .imageFormat = format,
            .imageColorSpace = color_space,
            .imageExtent = swapchain_extent,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = surf_cap.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .imageArrayLayers = 1,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = 1,
        };

        err = vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain);
        assert(!err);
    }

    struct {
        VkImage image;
        VkCommandBuffer cmd;
        VkImageView view;
        VkFramebuffer fb;
    } buffers[swapchain_image_count];


    {
        err = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, 0);
        assert(!err);
        VkImage swapchain_images[swapchain_image_count];
        err = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images);
        assert(!err);
        for (uint32_t i = 0; i < swapchain_image_count; i++)
            buffers[i].image = swapchain_images[i];
    }

    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo color_attachment_view = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .format = format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = buffers[i].image,
        };

        err = vkCreateImageView(device, &color_attachment_view, NULL, &buffers[i].view);
        assert(!err);
    }

    const VkAttachmentDescription attachments[1] = {
        [0] = {
            .format = format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
    };
    const VkAttachmentReference color_reference = {
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
    };
    const VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    VkRenderPass render_pass;
    err = vkCreateRenderPass(device, &rp_info, NULL, &render_pass);
    assert(!err);

    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageView attachments[1] = {
            [0] = buffers[i].view,
        };

        const VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchain_extent.width,
            .height = swapchain_extent.height,
            .layers = 1,
        };

        err = vkCreateFramebuffer(device, &fb_info, NULL, &buffers[i].fb);
        assert(!err);
    }

    for (;;) {
        {
            SDL_Event event;
            SDL_bool done = SDL_FALSE;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT:
                    done = SDL_TRUE;
                    break;
                }
            }
            if (done) break;
        }

        VkSemaphore present_complete_semaphore;
        {
            VkSemaphoreCreateInfo semaphore_create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            err = vkCreateSemaphore(device, &semaphore_create_info, NULL, &present_complete_semaphore);
            assert(!err);
        }

        uint32_t current_buffer;
        err = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, present_complete_semaphore, (VkFence)0, &current_buffer);
        assert(!err);

        const VkCommandBufferBeginInfo cmd_buf_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        const VkClearValue clear_values[1] = {
            [0] = { .color.float32 = { current_buffer * 1.f, .2f, .2f, .2f } },
        };

        const VkRenderPassBeginInfo rp_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = buffers[current_buffer].fb,
            .renderArea.extent = swapchain_extent,
            .clearValueCount = 1,
            .pClearValues = clear_values,
        };

        err = vkBeginCommandBuffer(draw_cmd, &cmd_buf_info);
        assert(!err);

        VkImageMemoryBarrier image_memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .image = buffers[current_buffer].image,
        };

        vkCmdPipelineBarrier(
            draw_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, NULL, 0, NULL, 1, &image_memory_barrier);

        vkCmdBeginRenderPass(draw_cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(draw_cmd);

        VkImageMemoryBarrier present_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .image = buffers[current_buffer].image,
        };

        vkCmdPipelineBarrier(
            draw_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, NULL, 0, NULL, 1, &present_barrier);

        err = vkEndCommandBuffer(draw_cmd);
        assert(!err);

        VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &draw_cmd,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &present_complete_semaphore,
            .pWaitDstStageMask = &pipe_stage_flags,
        };

        err = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        assert(!err);

        VkPresentInfoKHR present = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &current_buffer,
        };
        err = vkQueuePresentKHR(queue, &present);
        if (err == VK_SUBOPTIMAL_KHR)
            fprintf(stderr, "warning: suboptimal present\n");
        else
            assert(!err);

        err = vkQueueWaitIdle(queue);
        assert(err == VK_SUCCESS);

        vkDestroySemaphore(device, present_complete_semaphore, NULL);
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Vulkan Example", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 1024, 0);

    vulkan_main(window);
    return 0;
}
