#include <algorithm>
#include <cstring>
#include <format>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "app.h"

import alib6;

#define VST(X) VK_STRUCTURE_TYPE_##X

using namespace alib6;
using namespace alib6::log;

namespace {
    inline std::string vulkan_api_to_string(uint32_t spec) {
        return std::format("{}.{}.{}.{}",
            VK_API_VERSION_VARIANT(spec),
            VK_API_VERSION_MAJOR(spec),
            VK_API_VERSION_MINOR(spec),
            VK_API_VERSION_PATCH(spec)
        );
    }

    inline auto color_cyan() { return alib6::log::color(alib6::log::Color::Cyan); }
    inline auto color_blue() { return alib6::log::color(alib6::log::Color::Blue); }
    inline auto color_none() { return alib6::log::color(alib6::log::Color::None); }
}

void App::_setup_vulkan() {
    /// 初始化 instance
    _vk_create_instance();
    /// 初始化 VulkanCallback
    if (!enable_validation_layer_steps) _vk_setup_debug_callback();
    /// 创建 surface 对象
    _vk_create_window_surface();

    /// 选择物理设备
    _vk_pick_physical_device();
    /// 选择逻辑设备
    _vk_create_logical_device();

    /// 选择 swap surface 的 format
    _vk_choose_swap_surface_format();
    /// 选择呈现模式
    _vk_choose_present_mode();
    /// 选择交换空间范围
    _vk_choose_swap_extent();
    /// 创建交换链
    _vk_create_swap_chain();
    /// 创建 image views
    _vk_create_image_views();

    /// 创建 pipeline
    // renderpass
    _vk_create_render_pass();
    // pipeline
    _vk_create_graphics_pipeline();
    // framebuffer
    _vk_create_framebuffers();

    // cmd pool
    _vk_create_command_pool();
    // cmd buffer
    _vk_create_command_buffer();
    // sync
    _vk_create_sync_objects();
}

void App::_vk_create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_ci {};
    semaphore_ci.sType = VST(SEMAPHORE_CREATE_INFO);
    
    VkFenceCreateInfo fence_ci {};
    fence_ci.sType = VST(FENCE_CREATE_INFO);
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    auto create_semaphore = [&, this](std::string_view desc, size_t index, VkSemaphore & s) {
        if (VkResult result = vkCreateSemaphore(device, &semaphore_ci, allocator, &s)) {
            lg(LogLevel::Error) << translator->translate("bad.create_semaphore", desc, index, static_cast<int>(result)) << endlog;
            throw "Failed to create semaphore!";
        }
        defer_mgr.defer([this, s] {
            vkDestroySemaphore(device, s, allocator);
        });
    };

    auto create_fence = [&, this](std::string_view desc, size_t index, VkFence & s) {
        if (VkResult result = vkCreateFence(device, &fence_ci, allocator, &s)) {
            lg(LogLevel::Error) << translator->translate("bad.create_fence", desc, index, static_cast<int>(result)) << endlog;
            throw "Failed to create fence!";
        }
        defer_mgr.defer([this, s] {
            vkDestroyFence(device, s, allocator);
        });
    };
    sync_object_count = static_cast<uint32_t>(swapchain_views.size());

    fen_in_flights.resize(sync_object_count, VK_NULL_HANDLE);
    sem_img_available.resize(sync_object_count, VK_NULL_HANDLE);
    sem_render_fin.resize(sync_object_count, VK_NULL_HANDLE);

    for (size_t i = 0; i < fen_in_flights.size(); ++i) {
        create_semaphore("image_available", i, sem_img_available[i]);
        create_semaphore("render_finished", i, sem_render_fin[i]);
        create_fence("in_flight", i, fen_in_flights[i]);
    }

    lg(LogLevel::Info) << translator->translate("ok.sync", sync_object_count) << endlog;
}

void App::vk_record_command_buffer(VkCommandBuffer buffer, uint32_t image_index) {
    VkCommandBufferBeginInfo beg_info {};
    beg_info.sType = VST(COMMAND_BUFFER_BEGIN_INFO);
    beg_info.flags = 0;
    beg_info.pInheritanceInfo = nullptr;

    if (VkResult result = vkBeginCommandBuffer(buffer, &beg_info)) {
        lg(LogLevel::Error) << translator->translate("bad.begin_cmd_buffer", static_cast<int>(result)) << endlog;
        throw "Failed to begin command buffer!";
    }

    VkRenderPassBeginInfo render_bi {};
    render_bi.sType = VST(RENDER_PASS_BEGIN_INFO);
    render_bi.renderPass = render_pass;
    render_bi.framebuffer = framebuffers[image_index];
    render_bi.renderArea.offset = {0, 0};
    render_bi.renderArea.extent = swap_extent;

    VkClearValue clear_color = {{{0.f, 0.f, 0.f, 1.f}}};
    render_bi.clearValueCount = 1;
    render_bi.pClearValues = &clear_color;
    
    vkCmdBeginRenderPass(buffer, &render_bi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swap_extent.width);
    viewport.height = static_cast<float>(swap_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = swap_extent;

    vkCmdSetScissor(buffer, 0, 1, &scissor);

    vkCmdDraw(buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(buffer);
    if (VkResult result = vkEndCommandBuffer(buffer)) {
        lg(LogLevel::Error) << translator->translate("bad.end_cmd_buffer", static_cast<int>(result)) << endlog;
    }
}

void App::_vk_create_command_buffer() {
    sync_object_count = static_cast<uint32_t>(swapchain_views.size());

    VkCommandBufferAllocateInfo allocate_ci {};
    allocate_ci.sType = VST(COMMAND_BUFFER_ALLOCATE_INFO);
    allocate_ci.commandBufferCount = sync_object_count;
    allocate_ci.commandPool = cmd_pool;
    allocate_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    cmd_buffers.resize(sync_object_count, VK_NULL_HANDLE);
    if (VkResult result = vkAllocateCommandBuffers(device, &allocate_ci, cmd_buffers.data())) {
        lg(LogLevel::Error) << translator->translate("bad.allocate_cmd_buffer", static_cast<int>(result)) << endlog;
        throw "Failed to allocate command buffers!";
    }

    lg(LogLevel::Info) << translator->translate("ok.create_cmd_buffer") << endlog;
}

void App::_vk_create_command_pool() {
    VkCommandPoolCreateInfo pool_ci {};
    pool_ci.sType = VST(COMMAND_POOL_CREATE_INFO);
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = *queue_family.graphics;

    if (VkResult result = vkCreateCommandPool(device, &pool_ci, allocator, &cmd_pool)) {
        lg(LogLevel::Error) << translator->translate("bad.create_cmd_pool", static_cast<int>(result)) << endlog;
        throw "Failed to create command pool!";
    }
    defer_mgr.defer([this] {
        vkDestroyCommandPool(device, cmd_pool, allocator);
    });

    lg(LogLevel::Info) << translator->translate("ok.create_cmd_pool") << endlog;
}

void App::_vk_create_framebuffers() {
    framebuffers.resize(swapchain_views.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < framebuffers.size(); ++i) {
        VkImageView attachments[] = {
            swapchain_views[i]
        };

        VkFramebufferCreateInfo create_info {};
        create_info.sType = VST(FRAMEBUFFER_CREATE_INFO);
        create_info.renderPass = render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = swap_extent.width;
        create_info.height = swap_extent.height;
        create_info.layers = 1;

        if (VkResult result = vkCreateFramebuffer(device, &create_info, allocator, &framebuffers[i])) {
            lg(LogLevel::Error) << translator->translate("bad.create_framebuffer", static_cast<int>(result)) << endlog;
            throw "Failed to create framebuffer!";
        }
        defer_mgr.defer([this, i] {
            vkDestroyFramebuffer(device, framebuffers[i], allocator);
        });
    }
    
    lg(LogLevel::Info) << translator->translate("ok.create_framebuffer", framebuffers.size()) << endlog;
}

void App::_vk_create_render_pass() {
    VkAttachmentDescription color_attachment {};
    color_attachment.format = surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_ci {};
    render_pass_ci.sType = VST(RENDER_PASS_CREATE_INFO);
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = &color_attachment;
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pSubpasses = &subpass;
    render_pass_ci.dependencyCount = 1;
    render_pass_ci.pDependencies = &dependency;

    if (VkResult result = vkCreateRenderPass(device, &render_pass_ci, allocator, &render_pass)) {
        lg(LogLevel::Error) << translator->translate("bad.create_render_pass", static_cast<int>(result)) << endlog;
        throw "Failed to create render pass!";
    }
    defer_mgr.defer([this] {
        vkDestroyRenderPass(device, render_pass, allocator);
    });

    lg(LogLevel::Info) << translator->translate("ok.create_render_pass") << endlog;
}

void App::_vk_create_graphics_pipeline() {
    std::string vertex_shader, fragment_shader;
    std::string vertex_shader_raw, fragment_shader_raw;
    uint32_t detail_count = app_cfg.vulkan.shader_detail_count;

    if (app_cfg.vulkan.shaders.empty()) {
        lg(LogLevel::Error) << "No shader configuration found!" << endlog;
        throw "No shader configuration found!";
    }

    const auto& shader_cfg = app_cfg.vulkan.shaders[0];

    auto read_shader_file = [this](std::string_view path, std::string_view type, std::string & out) {
        if (io::read_all(path, out) == std::numeric_limits<usize>::max()) {
            lg(LogLevel::Error) << translator->translate("bad.read_shader",
                translator->translate(type),
                path
            ) << endlog;
            throw "Failed to read shader file!";
        }
    };

    read_shader_file(shader_cfg.vert, "shader_type.vert", vertex_shader);
    read_shader_file(shader_cfg.vert_raw, "shader_type.vert_raw", vertex_shader_raw);
    read_shader_file(shader_cfg.frag, "shader_type.frag", fragment_shader);
    read_shader_file(shader_cfg.frag_raw, "shader_type.frag_raw", fragment_shader_raw);

    lg(LogLevel::Info) << translator->translate("ok.read_shader") << 
        "\nVertex  :\n" << color_blue() << log_omit(vertex_shader_raw, detail_count) << color_none() << 
        "\nFragment:\n" << color_blue() << log_omit(fragment_shader_raw, detail_count) << color_none() <<
        endlog;

    // 创建着色器模块
    auto create_shader_module = [this](std::string_view type, std::span<char> code, VkShaderModule& mod) {
        VkShaderModuleCreateInfo create_info {};
        create_info.sType = VST(SHADER_MODULE_CREATE_INFO);
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        if (VkResult result = vkCreateShaderModule(device, &create_info, allocator, &mod)) {
            lg(LogLevel::Error) << translator->translate("bad.create_shader_module",
                translator->translate(type),
                static_cast<int>(result)
            ) << endlog;
            throw "Failed to create shader module!";
        }
    };

    DeferManager module_defer;
    VkShaderModule vert_module {VK_NULL_HANDLE};
    VkShaderModule frag_module {VK_NULL_HANDLE};

    create_shader_module("shader_type.vert", vertex_shader, vert_module);
    module_defer.defer([this, vert_module] {
        vkDestroyShaderModule(device, vert_module, allocator);
    });
    create_shader_module("shader_type.frag", fragment_shader, frag_module);
    module_defer.defer([this, frag_module] {
        vkDestroyShaderModule(device, frag_module, allocator);
    });

    VkPipelineShaderStageCreateInfo vert_create_info {};
    vert_create_info.sType = VST(PIPELINE_SHADER_STAGE_CREATE_INFO);
    vert_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_create_info.module = vert_module;
    vert_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_create_info {};
    frag_create_info.sType = VST(PIPELINE_SHADER_STAGE_CREATE_INFO);
    frag_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_create_info.module = frag_module;
    frag_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vert_create_info, frag_create_info};

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_ci {};
    dynamic_state_ci.sType = VST(PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dynamic_state_ci.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_ci.pDynamicStates = dynamic_states.data();

    VkPipelineVertexInputStateCreateInfo vertex_in_ci {};
    vertex_in_ci.sType = VST(PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    vertex_in_ci.vertexBindingDescriptionCount = 0;
    vertex_in_ci.pVertexBindingDescriptions = nullptr;    
    vertex_in_ci.vertexAttributeDescriptionCount = 0;
    vertex_in_ci.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo assemby_ci {};
    assemby_ci.sType = VST(PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    assemby_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assemby_ci.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport {};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(swap_extent.width);
    viewport.height = static_cast<float>(swap_extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = swap_extent;

    VkPipelineViewportStateCreateInfo viewport_ci {};
    viewport_ci.sType = VST(PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    viewport_ci.viewportCount = 1;
    viewport_ci.pViewports = &viewport;
    viewport_ci.scissorCount = 1;
    viewport_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster_ci {};
    raster_ci.sType = VST(PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    raster_ci.depthClampEnable = VK_FALSE;
    raster_ci.rasterizerDiscardEnable = VK_FALSE;
    raster_ci.polygonMode = VK_POLYGON_MODE_FILL;
    raster_ci.lineWidth = 1.0f;
    raster_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster_ci.depthBiasEnable = VK_FALSE;
    raster_ci.depthBiasClamp = 0.f;
    raster_ci.depthBiasConstantFactor = 0.f;
    raster_ci.depthBiasSlopeFactor = 0.f;

    VkPipelineMultisampleStateCreateInfo multi_sample_ci {};
    multi_sample_ci.sType = VST(PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    multi_sample_ci.sampleShadingEnable = VK_FALSE;
    multi_sample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multi_sample_ci.minSampleShading = 1.0f;
    multi_sample_ci.pSampleMask = nullptr;
    multi_sample_ci.alphaToCoverageEnable = VK_FALSE;
    multi_sample_ci.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend {};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend.blendEnable = VK_TRUE;
    color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend_state_ci {};
    blend_state_ci.sType = VST(PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    blend_state_ci.logicOpEnable = VK_FALSE;
    blend_state_ci.logicOp = VK_LOGIC_OP_COPY;
    blend_state_ci.attachmentCount = 1;
    blend_state_ci.pAttachments = &color_blend;
    blend_state_ci.blendConstants[0] = 0;
    blend_state_ci.blendConstants[1] = 0;
    blend_state_ci.blendConstants[2] = 0;
    blend_state_ci.blendConstants[3] = 0;

    VkPipelineLayoutCreateInfo pipeline_layout_ci {};
    pipeline_layout_ci.sType = VST(PIPELINE_LAYOUT_CREATE_INFO);
    pipeline_layout_ci.setLayoutCount = 0;
    pipeline_layout_ci.pSetLayouts = nullptr;
    pipeline_layout_ci.pushConstantRangeCount = 0;
    pipeline_layout_ci.pPushConstantRanges = nullptr;

    if (VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_ci, allocator, &pipeline_layout)) {
        lg(LogLevel::Error) << translator->translate("bad.create_pipeline_layout", static_cast<int>(result)) << endlog;
        throw "Failed to create pipeline layout!";
    }
    defer_mgr.defer([this] {
        vkDestroyPipelineLayout(device, pipeline_layout, allocator);
    });

    VkGraphicsPipelineCreateInfo graphics_ci {};
    graphics_ci.sType = VST(GRAPHICS_PIPELINE_CREATE_INFO);
    graphics_ci.stageCount = 2;
    graphics_ci.pStages = stages;
    graphics_ci.pVertexInputState = &vertex_in_ci;
    graphics_ci.pInputAssemblyState = &assemby_ci;
    graphics_ci.pViewportState = &viewport_ci;
    graphics_ci.pRasterizationState = &raster_ci;
    graphics_ci.pMultisampleState = &multi_sample_ci;
    graphics_ci.pDepthStencilState = nullptr;
    graphics_ci.pColorBlendState = &blend_state_ci;
    graphics_ci.pDynamicState = &dynamic_state_ci;

    graphics_ci.layout = pipeline_layout;
    graphics_ci.renderPass = render_pass;
    graphics_ci.subpass = 0;
    graphics_ci.basePipelineHandle = VK_NULL_HANDLE;
    graphics_ci.basePipelineIndex = -1;

    if (VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_ci, allocator, &graphics_pipeline)) {
        lg(LogLevel::Error) << translator->translate("bad.create_pipeline", static_cast<int>(result)) << endlog;
        throw "Failed to create graphics pipeline!";
    }
    defer_mgr.defer([this] {
        vkDestroyPipeline(device, graphics_pipeline, allocator);
    });

    lg(LogLevel::Info) << translator->translate("ok.create_pipeline") << endlog;
}

void App::_vk_create_image_views() {
    swapchain_views.resize(swapchain_images.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < swapchain_views.size(); ++i) {
        VkImageViewCreateInfo create_info {};
        create_info.sType = VST(IMAGE_VIEW_CREATE_INFO);
        create_info.image = swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = surface_format.format;
        create_info.components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        create_info.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        if (VkResult result = vkCreateImageView(device, &create_info, allocator, &swapchain_views[i])) {
            lg(LogLevel::Error) << translator->translate("bad.create_view", i, static_cast<int>(result)) << endlog;
            throw "Failed to create image view!";
        }
        defer_mgr.defer([this, i] {
            vkDestroyImageView(device, swapchain_views[i], allocator);
        });
    }

    lg(LogLevel::Info) << translator->translate("ok.create_view", swapchain_views.size()) << endlog;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_call_back(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    App* papp = reinterpret_cast<App*>(pUserData);
    if (papp) [[likely]] {
        App& app = *papp;
        LogLevel sv;

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            sv = LogLevel::Error;
            if (!app.allow_posts[0]) return VK_FALSE;
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            sv = LogLevel::Warn;
            if (!app.allow_posts[1]) return VK_FALSE;
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            sv = LogLevel::Info;
            if (!app.allow_posts[2]) return VK_FALSE;
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            sv = LogLevel::Trace;
            if (!app.allow_posts[3]) return VK_FALSE;
        } else {
            sv = LogLevel::Debug;
            if (!app.allow_posts[4]) return VK_FALSE;
        }

        auto && ctx = app.vk_validation_lg(sv);

        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            std::move(ctx) << "[Performance] ";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            std::move(ctx) << "[Validation ] ";
        }

        std::move(ctx) << pCallbackData->pMessage << endlog; 
    } else [[unlikely]] {
        std::cerr << "[Early Vulkan Log]: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
    return VK_FALSE;
}

void App::_vk_create_swap_chain() {
    auto & cap = swap_chain_details.capabilities;
    uint32_t image_count = cap.minImageCount + 1;
    if (cap.maxImageCount > 0 && image_count > cap.maxImageCount) {
        image_count = cap.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info {};
    create_info.sType = VST(SWAPCHAIN_CREATE_INFO_KHR);
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swap_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t indices[] = {*queue_family.graphics, *queue_family.present};
    if (indices[0] != indices[1]) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }
    create_info.preTransform = cap.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (VkResult result = vkCreateSwapchainKHR(device, &create_info, allocator, &swapchain)) {
        lg(LogLevel::Error) << translator->translate("bad.create_swapchain", static_cast<int>(result)) << endlog;
        throw "Failed to create swapchain!";
    }
    defer_mgr.defer([this] {
        vkDestroySwapchainKHR(device, swapchain, allocator);
    });

    uint32_t swapchain_images_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, nullptr);
    swapchain_images.resize(swapchain_images_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, swapchain_images.data());

    lg(LogLevel::Info) << translator->translate("ok.create_swapchain", swapchain_images_count) << endlog;
}

void App::_vk_choose_swap_extent() {
    if (swap_chain_details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        swap_extent = swap_chain_details.capabilities.currentExtent;
    } else {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(
            actual_extent.width,
            swap_chain_details.capabilities.minImageExtent.width,
            swap_chain_details.capabilities.maxImageExtent.width
        );
        actual_extent.height = std::clamp(
            actual_extent.height,
            swap_chain_details.capabilities.minImageExtent.height,
            swap_chain_details.capabilities.maxImageExtent.height
        );

        swap_extent = actual_extent;
    }

    lg(LogLevel::Info) << translator->translate(
        "ok.select_extent",
        swap_extent.width,
        swap_extent.height
    ) << endlog;
}

void App::_vk_choose_present_mode() {
    present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto & p : swap_chain_details.present_modes) {
        if (p == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = p;
        }
    }

    lg(LogLevel::Info) << translator->translate("ok.select_present", static_cast<int>(present_mode)) << endlog;
}

void App::_vk_choose_swap_surface_format() {
    bool selected = false;
    for (const auto & fmt : swap_chain_details.formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            surface_format = fmt;
            selected = true;
            break;
        } else if (fmt.format == VK_FORMAT_B8G8R8_SRGB &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            surface_format = fmt;
            selected = true;
            break;
        }
    }
    if (!selected) surface_format = swap_chain_details.formats[0];

    lg(LogLevel::Info) << translator->translate(
        "ok.select_format",
        static_cast<int>(surface_format.format),
        static_cast<int>(surface_format.colorSpace)
    ) << endlog;
}

static SwapChainSupportDetails query_swap_chain_details(
    VkPhysicalDevice dev,
    VkSurfaceKHR surface
) {
    SwapChainSupportDetails det;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &det.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &format_count, nullptr);
    if (format_count) {
        det.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &format_count, det.formats.data());
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &present_mode_count, nullptr);
    if (present_mode_count) {
        det.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &present_mode_count, det.present_modes.data());
    }

    return det;
} 

static bool check_required_extensions(
    VkPhysicalDevice physical_device,
    std::vector<const char*> & fill_in,
    const std::vector<const char *>& exts,
    std::vector<std::string> & verbose_names,
    bool verbose = false
) {
    uint32_t dev_ext_c;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &dev_ext_c, nullptr);
    std::vector<VkExtensionProperties> vk_extensions(dev_ext_c);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &dev_ext_c, vk_extensions.data());
    std::unordered_set<std::string_view> vk_existing_extensions;
    
    for (auto & p : vk_extensions) {
        vk_existing_extensions.emplace(p.extensionName);
        if (verbose) {
            auto& str = verbose_names.emplace_back(p.extensionName);
            str += "(v";
            str += vulkan_api_to_string(p.specVersion);
            str += ")";
        }
    }

    fill_in.clear();
    for (auto & data : exts) {
        auto it = vk_existing_extensions.find(data);
        if (it != vk_existing_extensions.end()) {
            fill_in.emplace_back(data);
        } else {
            return false;
        }
    }
    return true;
}

static QueueFamilyIndices find_queue_families(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    QueueFamilyIndices q;
    uint32_t qf_count;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, properties.data());

    VkBool32 support_present = VK_FALSE;

    size_t i = 0;
    for (auto & p : properties) {
        support_present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, static_cast<uint32_t>(i), surface, &support_present);
        bool support_graphics = (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        if (!q.graphics && support_graphics) {
            q.graphics.emplace(static_cast<uint32_t>(i));
        }
        if (!q.present && support_present) {
            q.present.emplace(static_cast<uint32_t>(i));
        }

        if (support_graphics && support_present) {
            q.graphics.emplace(static_cast<uint32_t>(i));
            q.present.emplace(static_cast<uint32_t>(i));
            break;
        }
        ++i;
    }
    return q;
}

void App::_vk_create_window_surface() {
    if (VkResult result = glfwCreateWindowSurface(instance, window, allocator, &surface)) {
        lg(LogLevel::Error) << translator->translate("bad.create_surface", static_cast<int>(result)) << endlog;
        throw "Failed to create window surface!";
    }
    defer_mgr.defer([this] {
        vkDestroySurfaceKHR(instance, surface, allocator);
    });

    lg(LogLevel::Info) << translator->translate("ok.create_surface") << endlog;
}

void App::_vk_create_logical_device() {
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    std::unordered_set<uint32_t> unique_queue_families = {
        *queue_family.graphics,
        *queue_family.present
    };
    float queue_priority = 1.0f;
    for (auto & v : unique_queue_families) {
        auto & info = queue_infos.emplace_back(VkDeviceQueueCreateInfo{});

        info.sType = VST(DEVICE_QUEUE_CREATE_INFO);
        info.queueFamilyIndex = v;
        info.queueCount = 1;
        info.pQueuePriorities = &queue_priority;
    }

    VkDeviceCreateInfo create_info {};
    create_info.sType = VST(DEVICE_CREATE_INFO);
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;
    create_info.enabledExtensionCount = static_cast<uint32_t>(valid_device_extensions.size());
    create_info.ppEnabledExtensionNames = valid_device_extensions.data();

    if (VkResult result = vkCreateDevice(physical_device, &create_info, allocator, &device)) {
        lg(LogLevel::Error) << translator->translate(
            "bad.vk_device",
            static_cast<int>(result)
        ) << endlog;
        throw "Failed to create logical device!";
    }
    defer_mgr.defer([this] {
        vkDestroyDevice(device, allocator);
    });

    lg(LogLevel::Info) << translator->translate(
        "ok.vk_device",
        valid_device_extensions
    ) << endlog;

    /// 获取队列
    vkGetDeviceQueue(device, *queue_family.graphics, 0, &graphics_queue);
    vkGetDeviceQueue(device, *queue_family.present, 0, &present_queue);
}

void App::_vk_pick_physical_device() {
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
    if (physical_device_count == 0) {
        lg(LogLevel::Error) << translator->translate("bad.no_phy_dev") << endlog;
        throw "No Vulkan physical devices found!";
    }
    std::vector<VkPhysicalDevice> devices(physical_device_count);
    std::vector<VkPhysicalDeviceProperties> properties;
    std::vector<VkPhysicalDeviceFeatures> features;
    std::vector<std::string> device_info;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, devices.data());
    
    /// 构建用户扩展需求 (直接取自强类型反射配置)
    std::vector<const char *> user_required;
    for (const auto & s : app_cfg.vulkan.device_extensions) {
        user_required.emplace_back(s.c_str());
    }
    std::vector<std::string> device_infos;
    bool verbose = app_cfg.vulkan.verbose_extensions;

    size_t index = 0;
    double highest_score = 0;
    int highest_index = -1;
    QueueFamilyIndices highest_qf;
    SwapChainSupportDetails highest_swap;

    double mul_api_score = app_cfg.vulkan.score_multiplier.api_version;
    double mul_img2d_score = app_cfg.vulkan.score_multiplier.image_dim2d;
    double mul_discrete = app_cfg.vulkan.score_multiplier.discrete_gpu;
    bool need_geometry = app_cfg.vulkan.score_multiplier.need_geometry;
    bool fail_load = app_cfg.vulkan.score_multiplier.fail_load;
    
    lg << translator->translate("check.begin_scoring") << endlog;

    std::vector<const char *> valid_exts;
    for (auto & dev : devices) {
        double score = 0;
        VkPhysicalDeviceProperties deviceProperties {};
        VkPhysicalDeviceFeatures deviceFeatures {};
        vkGetPhysicalDeviceProperties(dev, &deviceProperties);
        vkGetPhysicalDeviceFeatures(dev, &deviceFeatures);
        properties.emplace_back(deviceProperties);
        features.emplace_back(deviceFeatures);

        // 生成名字
        auto & str = device_info.emplace_back(deviceProperties.deviceName);
        str += "@Vulkan";
        str += vulkan_api_to_string(deviceProperties.apiVersion);

        /// 计算分数
        double api_score = 
            (VK_API_VERSION_MAJOR(deviceProperties.apiVersion) * 1024 + 
            VK_API_VERSION_MINOR(deviceProperties.apiVersion)) * mul_api_score;

        double image_score = deviceProperties.limits.maxImageDimension2D * mul_img2d_score;

        score = api_score + image_score;
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score *= mul_discrete;
        }
        
        auto q_family = find_queue_families(dev, surface);
        bool extension_supports = check_required_extensions(dev, valid_exts, user_required, device_infos, verbose);

        if (need_geometry && deviceFeatures.geometryShader == VK_FALSE) score = -1;
        else if (fail_load) score = -2;
        else if (!q_family.is_complete()) score = -3;
        else if (!extension_supports) {
            valid_device_extensions.clear();
            score = -4;
        }
        
        // swap chain 部分
        SwapChainSupportDetails swap_details;
        if (extension_supports) {
            swap_details = query_swap_chain_details(dev, surface);
            if (swap_details.formats.empty() || swap_details.present_modes.empty()) {
                score = -5;
            }
        }
        
        // 结算
        if (score > highest_score) {
            highest_index = static_cast<int>(index);
            highest_score = score;
            highest_qf = q_family;
            highest_swap = swap_details;
            std::memcpy(&device_properties, &deviceProperties, sizeof(deviceProperties));
            std::memcpy(&device_features, &deviceFeatures, sizeof(deviceFeatures));
        }

        if (verbose) {
            lg << translator->translate("check.gpu_score_with_exts",
                device_info[index],
                score,
                device_infos.size()
            ) << color_cyan() << device_infos << endlog;  
            device_infos.clear();
        } else {
            lg << translator->translate("check.gpu_score",
                device_info[index],
                score
            ) << endlog;  
        }   
        ++index;
    }

    valid_device_extensions = user_required;
    swap_chain_details = highest_swap;

    lg << translator->translate("check.physical_device",
        device_info.size()
    ) << color_cyan() << device_info << endlog;

    if (highest_index == -1) {
        lg(LogLevel::Error) << translator->translate("bad.no_gpu_satisfiable", user_required) << endlog;
        throw "No suitable GPU found!";
    }

    physical_device = devices[highest_index];
    queue_family = highest_qf;

    lg(LogLevel::Info) << translator->translate("ok.selecting_gpu",
        device_info[highest_index],
        highest_score
    ) << endlog;
}

void App::_vk_setup_debug_callback() {
    // 设置 allow post (直接取自强类型反射配置)
    allow_posts[0] = app_cfg.vulkan.debug_allow.error;
    allow_posts[1] = app_cfg.vulkan.debug_allow.warn;
    allow_posts[2] = app_cfg.vulkan.debug_allow.info;
    allow_posts[3] = app_cfg.vulkan.debug_allow.verbose;
    allow_posts[4] = app_cfg.vulkan.debug_allow.others;

    // 设置回调
    VkDebugUtilsMessengerCreateInfoEXT create_info {};
    create_info.sType = VST(DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    create_info.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    create_info.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_call_back;
    create_info.pUserData = reinterpret_cast<void*>(this);

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (func) {
        if (VkResult result = func(instance, &create_info, allocator, &debug_messenger)) {
            lg(LogLevel::Error) << translator->translate("bad.create_debug_messenger",
                static_cast<int>(result)
            ) << endlog;
            throw "Failed to create debug messenger!";
        }
        lg(LogLevel::Info) << translator->translate("ok.create_debug_messenger") << endlog;
    } else {
        lg(LogLevel::Error) << translator->translate("bad.debug_messenger") << endlog;
        return;
    }

    defer_mgr.defer([this] {
        auto destroy_func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );
        if (destroy_func) destroy_func(instance, debug_messenger, allocator);
        else {
            lg(LogLevel::Error) << translator->translate("bad.destroy_debug_messenger") << endlog;
        }
    });
}

void App::_vk_create_instance() {
    VkApplicationInfo app_info {};
    app_info.sType = VST(APPLICATION_INFO);
    app_info.pApplicationName = app_cfg.vulkan.app_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(
        app_cfg.vulkan.app_version.size() > 0 ? app_cfg.vulkan.app_version[0] : 1,
        app_cfg.vulkan.app_version.size() > 1 ? app_cfg.vulkan.app_version[1] : 0,
        app_cfg.vulkan.app_version.size() > 2 ? app_cfg.vulkan.app_version[2] : 0
    );
    app_info.pEngineName = app_cfg.vulkan.engine_name.c_str();
    app_info.engineVersion = VK_MAKE_VERSION(
        app_cfg.vulkan.engine_version.size() > 0 ? app_cfg.vulkan.engine_version[0] : 1,
        app_cfg.vulkan.engine_version.size() > 1 ? app_cfg.vulkan.engine_version[1] : 0,
        app_cfg.vulkan.engine_version.size() > 2 ? app_cfg.vulkan.engine_version[2] : 0
    );
    app_info.apiVersion = VK_MAKE_API_VERSION(
        app_cfg.vulkan.api_version.size() > 0 ? app_cfg.vulkan.api_version[0] : 0,
        app_cfg.vulkan.api_version.size() > 1 ? app_cfg.vulkan.api_version[1] : 1,
        app_cfg.vulkan.api_version.size() > 2 ? app_cfg.vulkan.api_version[2] : 3,
        app_cfg.vulkan.api_version.size() > 3 ? app_cfg.vulkan.api_version[3] : 0
    );

    // 对 Vulkan 扩展进行检测
    uint32_t vk_extc = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &vk_extc, nullptr);
    std::vector<VkExtensionProperties> vk_extensions(vk_extc);
    vkEnumerateInstanceExtensionProperties(nullptr, &vk_extc, vk_extensions.data());
    std::unordered_set<std::string_view> vk_existing_extensions;
    {
        std::vector<std::string> str_extensions;
        for (auto & p : vk_extensions) {
            vk_existing_extensions.emplace(p.extensionName);
            auto & s = str_extensions.emplace_back(p.extensionName);
            s += "(v";
            s += vulkan_api_to_string(p.specVersion);
            s += ")";
        }
        
        lg << translator->translate("check.vk_instance_ext",
            vk_extc
        ) << color_cyan() << str_extensions << endlog;
    }

    // 检查 Layer 扩展
    uint32_t vk_layerc = 0;
    vkEnumerateInstanceLayerProperties(&vk_layerc, nullptr);
    std::vector<VkLayerProperties> available_layers(vk_layerc);
    vkEnumerateInstanceLayerProperties(&vk_layerc, available_layers.data());
    std::unordered_set<std::string_view> vk_existing_layers;
    {
        std::vector<std::string> str_layers;
        for (auto & p : available_layers) {
            vk_existing_layers.emplace(p.layerName);
            auto & str = str_layers.emplace_back(p.layerName);
            str += "@v";
            str += 
                std::to_string(VK_API_VERSION_MAJOR(p.specVersion)) + "." +
                std::to_string(VK_API_VERSION_MINOR(p.specVersion)) + "." +
                std::to_string(VK_API_VERSION_PATCH(p.specVersion));
            str += ":";
            str += p.description;
        }
        lg << translator->translate("check.vk_instance_layers",
            vk_layerc
        ) << color_cyan() << str_layers << endlog;
    }

    VkInstanceCreateInfo create_info {};
    create_info.sType = VST(INSTANCE_CREATE_INFO);
    create_info.pApplicationInfo = &app_info;

    // 检查 GLFW 扩展支持
    uint32_t glfw_extc = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_extc);
    lg << translator->translate("check.glfw_ext", glfw_extc)
       << color_cyan()
       << std::span(glfw_exts, glfw_extc) << endlog;

    std::vector<const char *> required_extensions;
    required_extensions.insert(
        required_extensions.begin(),
        glfw_exts,
        glfw_exts + glfw_extc
    );

    /// 用户自定义扩展 (来自强类型反射配置)
    {
        std::vector<std::string_view> unfound_extensions;
        for (const auto & s : app_cfg.vulkan.instance_extensions) {
            auto it = vk_existing_extensions.find(s);
            if (it != vk_existing_extensions.end()) {
                required_extensions.emplace_back(it->data());
                
                if (*it == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) {
                    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                } else if (*it == VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
                    --enable_validation_layer_steps;
                }
            } else {
                unfound_extensions.emplace_back(s);
            }
        }
        if (!unfound_extensions.empty()) {
            lg(LogLevel::Error) << 
                translator->translate("bad.ext_not_found",
                    unfound_extensions.size(),
                    unfound_extensions
                )
            << endlog;
            throw "Vulkan instance extension not found!";
        }
    }

    create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();

    // 用户选择的中间层 (来自强类型反射配置)
    std::vector<const char *> required_layers;
    {
        std::vector<std::string_view> unfound_layers;
        for (const auto & s : app_cfg.vulkan.layers) {
            auto it = vk_existing_layers.find(s);
            if (it != vk_existing_layers.end()) {
                required_layers.emplace_back(it->data());
                if (*it == "VK_LAYER_KHRONOS_validation") {
                    --enable_validation_layer_steps;
                }
            } else {
                unfound_layers.emplace_back(s);
            }
        }

        if (!unfound_layers.empty()) {
            lg(LogLevel::Error) << 
                translator->translate("bad.layer_not_found",
                    unfound_layers.size(),
                    unfound_layers
                )
            << endlog;
            throw "Vulkan validation layer not found!";
        }
    }

    create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
    create_info.ppEnabledLayerNames = required_layers.data();

    if (VkResult result = vkCreateInstance(&create_info, allocator, &instance)) {
        lg(LogLevel::Error) << translator->translate(
            "bad.vk_instance",
            static_cast<int>(result)
        ) << endlog;
        throw "Failed to create Vulkan instance!";
    }
    defer_mgr.defer([this] {
        vkDestroyInstance(instance, allocator);
    });

    lg(LogLevel::Info) << translator->translate(
        "ok.vk_instance",
        vulkan_api_to_string(app_info.apiVersion),
        required_extensions,
        required_layers
    ) << endlog;
}
