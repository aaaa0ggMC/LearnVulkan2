#include <app.h>
#include <alib5/aperf.h>
#define VST(X) VK_STRUCTURE_TYPE_##X

using namespace alib5;
using enum Severity;

void App::_setup_vulkan(){
    /// 初始化instance
    _vk_create_instance();
    /// 初始化VulkanCallback
    if(!enable_validation_layer_steps)_vk_setup_debug_callback();
    /// 创建surface对象
    _vk_create_window_surface();

    /// 选择物理设备
    _vk_pick_physical_device();
    /// 选择逻辑设备
    _vk_create_logical_device();

    /// 选择swap surface的format
    _vk_choose_swap_surface_format();
    ///  选择呈现模式
    _vk_choose_present_mode();
    /// 选择交换空间范围
    _vk_choose_swap_extent();
    /// 创建交换链
    _vk_create_swap_chain();
    // 创建image views
    _vk_create_image_views();

    /// 创建pipeline
    // renderpass
    _vk_create_render_pass();
    // pipeline
    _vk_create_graphics_pipeline();
    // 我是抖M吗?...累死了
    // framebuffer
    _vk_create_framebuffers();

    // cmd pool
    _vk_create_command_pool();
    // cmd buffer
    _vk_create_command_buffer();
    // sync
    _vk_create_sync_objects();
}

void App::_vk_create_sync_objects(){
    VkSemaphoreCreateInfo semaphore_ci {};
    semaphore_ci.sType = VST(SEMAPHORE_CREATE_INFO);
    
    VkFenceCreateInfo fence_ci {};
    fence_ci.sType = VST(FENCE_CREATE_INFO);
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    auto create_semaphore = [&,this](std::string_view desc,VkSemaphore & s){
        if(VkResult result = vkCreateSemaphore(device,&semaphore_ci,allocator,&s)){
            lg(Error,*translator,"bad.create_semaphore",desc,(int)result) << endlog;
            throw "BG";
        }
        defer_mgr.defer([this,s]{
            vkDestroySemaphore(device,s,allocator);
        });
    };

    auto create_fence = [&,this](std::string_view desc,VkFence & s){
        if(VkResult result = vkCreateFence(device,&fence_ci,allocator,&s)){
            lg(Error,*translator,"bad.create_fence",desc,(int)result) << endlog;
            throw "BG";
        }
        defer_mgr.defer([this,s]{
            vkDestroyFence(device,s,allocator);
        });
    };

    create_semaphore("image_available", sem_img_available);
    create_semaphore("render_finished", sem_render_fin);
    create_fence("in_flight", fen_in_flight);

    lg(Info,*translator,"ok.sync") << endlog;
}

void App::vk_record_command_buffer(VkCommandBuffer buffer,uint32_t image_index){
    VkCommandBufferBeginInfo beg_info {};
    beg_info.sType = VST(COMMAND_BUFFER_BEGIN_INFO);
    beg_info.flags = 0;
    beg_info.pInheritanceInfo = nullptr;

    if(VkResult result = vkBeginCommandBuffer(buffer, &beg_info)){
        lg(Error,*translator,"bad.begin_cmd_buffer",(int)result) << endlog;
        throw "BG";
    }

    VkRenderPassBeginInfo render_bi {};
    render_bi.sType = VST(RENDER_PASS_BEGIN_INFO);
    render_bi.renderPass = render_pass;
    render_bi.framebuffer = framebuffers[image_index];
    render_bi.renderArea.offset = {0,0};
    render_bi.renderArea.extent = swap_extent;

    VkClearValue clear_color = {{{0.f,0.f,0.f,1.f}}};
    render_bi.clearValueCount = 1;
    render_bi.pClearValues = &clear_color;
    
    vkCmdBeginRenderPass(cmd_buffer,&render_bi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buffer,VK_PIPELINE_BIND_POINT_GRAPHICS,graphics_pipeline);

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)(swap_extent.width);
    viewport.height = (float)(swap_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0,0};
    scissor.extent = swap_extent;

    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer,3,1,0,0);
    vkCmdEndRenderPass(cmd_buffer);
    if(VkResult result = vkEndCommandBuffer(cmd_buffer)){
        lg(Error,*translator,"bad.end_cmd_buffer",(int)result) << endlog;
    }
}

void App::_vk_create_command_buffer(){
    VkCommandBufferAllocateInfo allocate_ci {};
    allocate_ci.sType = VST(COMMAND_BUFFER_ALLOCATE_INFO);
    allocate_ci.commandBufferCount = 1;
    allocate_ci.commandPool = cmd_pool;
    allocate_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if(VkResult result = vkAllocateCommandBuffers(device,&allocate_ci, &cmd_buffer)){
        lg(Error,*translator,"bad.allocate_cmd_buffer",(int)result) << endlog;
        throw "BG";
    }

    lg(Info,*translator,"ok.create_cmd_buffer") << endlog;
}

void App::_vk_create_command_pool(){
    VkCommandPoolCreateInfo pool_ci {};
    pool_ci.sType = VST(COMMAND_POOL_CREATE_INFO);
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = *queue_family.graphics;

    if(VkResult result = vkCreateCommandPool(device,&pool_ci,allocator,&cmd_pool)){
        lg(Error,*translator,"bad.create_cmd_pool",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroyCommandPool(device,cmd_pool,allocator);
    });

    lg(Info,*translator,"ok.create_cmd_pool") << endlog;
}


void App::_vk_create_framebuffers(){
    framebuffers.resize(swapchain_views.size(),VK_NULL_HANDLE);
    for(size_t i = 0;i <framebuffers.size();++i){
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

        if(VkResult result = vkCreateFramebuffer(device,&create_info,allocator,&framebuffers[i])){
            lg(Error,*translator,"bad.create_framebuffer",(int)result) << endlog;
            throw "BG";
        }
        defer_mgr.defer([this,i]{
            vkDestroyFramebuffer(device,framebuffers[i],allocator);
        });
    }
    
    lg(Info,*translator,"ok.create_framebuffer",framebuffers.size()) << endlog;
}

void App::_vk_create_render_pass(){
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

    VkRenderPassCreateInfo render_pass_ci {};
    render_pass_ci.sType = VST(RENDER_PASS_CREATE_INFO);
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = &color_attachment;
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pSubpasses = &subpass;

    if(VkResult result = vkCreateRenderPass(device,&render_pass_ci,allocator,&render_pass)){
        lg(Error,*translator,"bad.create_render_pass",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroyRenderPass(device,render_pass,allocator);
    });

    lg(Info,*translator,"ok.create_render_pass") << endlog;
}

void App::_vk_create_graphics_pipeline(){
    std::string vertex_shader,fragment_shader;
    std::string vertex_shader_raw,fragment_shader_raw;
    uint32_t detail_count = config.jump("/vulkan/shader_detail_count").to<uint32_t>();

    auto read = [this](std::string_view cfg,std::string_view type,std::string & o){
        std::string_view path = config.jump(cfg).to<std::string_view>();
        if(io::read_all(path,o) == std::variant_npos){
            lg(Error,*translator,"bad.read_shader",
                translator->translate(type),
                path
            ) << endlog;
            throw "BG";
        }
    };

    read("/vulkan/shaders/0/vert","shader_type.vert",vertex_shader);
    read("/vulkan/shaders/0/vert_raw","shader_type.vert_raw",vertex_shader_raw);
    read("/vulkan/shaders/0/frag","shader_type.frag",fragment_shader);
    read("/vulkan/shaders/0/frag_raw","shader_type.frag_raw",fragment_shader_raw);

    lg(Info,*translator,"ok.read_shader") << 
    "\nVertex  :\n" << LOG_COLOR1(Blue) << log_omit(vertex_shader_raw,detail_count) << LOG_COLOR1(None) << 
    "\nFragment:\n" << LOG_COLOR1(Blue) << log_omit(fragment_shader_raw,detail_count) << LOG_COLOR1(None) <<
    endlog;

    // 创建着色器模块
    auto create_shader_module = [this](std::string_view type,std::span<char> code,VkShaderModule& mod){
        VkShaderModuleCreateInfo create_info {};

        create_info.sType = VST(SHADER_MODULE_CREATE_INFO);
        create_info.codeSize = code.size();
        create_info.pCode = (const uint32_t*)code.data();

        if(VkResult result = vkCreateShaderModule(
                device, 
                &create_info, 
                allocator,
                &mod
        )){

            lg(Error,*translator,"bad.create_shader_module",
                translator->translate(type),
                (int)result
            ) << endlog;
            throw "BG";
        }
    };

    misc::DeferManager module_defer;
    VkShaderModule vert_module {VK_NULL_HANDLE};
    VkShaderModule frag_module {VK_NULL_HANDLE};

    create_shader_module("shader_type.vert",vertex_shader,vert_module);
    module_defer.defer([this,vert_module]{
        vkDestroyShaderModule(device,vert_module,allocator);
    });
    create_shader_module("shader_type.frag",fragment_shader,frag_module);
    module_defer.defer([this,frag_module]{
        vkDestroyShaderModule(device,frag_module,allocator);
    });

    VkPipelineShaderStageCreateInfo vert_create_info {};
    vert_create_info.sType = VST(PIPELINE_SHADER_STAGE_CREATE_INFO);
    vert_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_create_info.module = vert_module;
    // 暂时硬编码,因为也只用到了一个shader其实
    vert_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_create_info {};
    frag_create_info.sType = VST(PIPELINE_SHADER_STAGE_CREATE_INFO);
    frag_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_create_info.module = frag_module;
    // 暂时硬编码,因为也只用到了一个shader其实
    frag_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vert_create_info,frag_create_info};

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_ci {};
    dynamic_state_ci.sType = VST(PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dynamic_state_ci.dynamicStateCount = dynamic_states.size();
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
    viewport.width = (float)swap_extent.width;
    viewport.height = (float)swap_extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor {};
    scissor.offset = {0,0};
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

    if(VkResult result = vkCreatePipelineLayout(device,&pipeline_layout_ci, allocator, &pipeline_layout)){
        lg(Error,*translator,"bad.create_pipeline_layout",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroyPipelineLayout(device,pipeline_layout,allocator);
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

    if(VkResult result = vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&graphics_ci,allocator,&graphics_pipeline)){
        lg(Error,*translator,"bad.create_pipeline",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroyPipeline(device,graphics_pipeline,allocator);
    });

    lg(Info,*translator,"ok.create_pipeline") << endlog;
}

void App::_vk_create_image_views(){
    swapchain_views.resize(swapchain_images.size(),VK_NULL_HANDLE);
    for(size_t i = 0;i < swapchain_views.size();++i){
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

        if(VkResult result = vkCreateImageView(device,&create_info,allocator, &swapchain_views[i])){
            lg(Error,*translator,"bad.create_view",i,(int)result) << endlog;
            throw "BG";     
        }
        // 成功创建,针对每一个注册销毁函数
        defer_mgr.defer([this,i]{
            vkDestroyImageView(device, swapchain_views[i], allocator);
        });
    }

    lg(Info,*translator,"ok.create_view",swapchain_views.size()) << endlog;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_call_back(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
){
    App * papp = reinterpret_cast<App*>(pUserData);
    if(papp) [[likely]] {
        App & app = *papp;
        Severity sv;

        if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
            sv = Error;
            if(!app.allow_posts[0])return VK_FALSE;
        }else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
            sv = Warn;
            if(!app.allow_posts[1])return VK_FALSE;
        }else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT){
            sv = Info;
            if(!app.allow_posts[2])return VK_FALSE;
        }else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT){
            sv = Trace;
            if(!app.allow_posts[3])return VK_FALSE;
        }else{
            sv = Debug;
            if(!app.allow_posts[4])return VK_FALSE;
        }

        auto && ctx = app.vk_validation_lg(sv);

        if(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT){
            std::move(ctx) << "[Performance]";
        }else if(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT){
            std::move(ctx) << "[Validation ]";
        }

        // 线程安全的哈哈
        // header为VulkanValidation
        std::move(ctx) << pCallbackData->pMessage << endlog; 
    }else [[unlikely]]{
        std::cerr << "[Early Vulkan Log]: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
    return VK_FALSE;
}

void App::_vk_create_swap_chain(){
    auto & cap = swap_chain_details.capabilities;
    uint32_t image_count = cap.minImageCount + 1;
    if(cap.maxImageCount > 0 && image_count > cap.maxImageCount){
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
    uint32_t indices[] = {*queue_family.graphics,*queue_family.present};
    if(indices[0] != indices[1]){
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = indices;
    }else{
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }
    create_info.preTransform = cap.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if(VkResult result = vkCreateSwapchainKHR(device,&create_info,allocator,&swapchain)){
        lg(Error,*translator,"bad.create_swapchain",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroySwapchainKHR(device,swapchain,allocator);
    });

    uint32_t swapchain_images_count = 0;
    vkGetSwapchainImagesKHR(device,swapchain,&swapchain_images_count,nullptr);
    swapchain_images.resize(swapchain_images_count);
    vkGetSwapchainImagesKHR(device,swapchain,&swapchain_images_count,swapchain_images.data());

    lg(Info,*translator,"ok.create_swapchain",swapchain_images_count) << endlog;
}

void App::_vk_choose_swap_extent(){
    if(swap_chain_details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
        swap_extent = swap_chain_details.capabilities.currentExtent;
    }else{
        int width = 0,height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actual_extent = {
            (uint32_t)width,
            (uint32_t)height
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

    lg(Info,
        *translator,
        "ok.select_extent",
        swap_extent.width,
        swap_extent.height
    ) << endlog;
}

void App::_vk_choose_present_mode(){
    present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(auto & p : swap_chain_details.present_modes){
        if(p == VK_PRESENT_MODE_MAILBOX_KHR){
            present_mode = p;
        }
    }

    lg(Info,*translator,"ok.select_present",(int)present_mode) << endlog;
}

void App::_vk_choose_swap_surface_format(){
    bool selected = false;
    for(const auto & fmt : swap_chain_details.formats){
        if(fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ){
            surface_format = fmt;
            selected = true;
            break;
        }else if(fmt.format == VK_FORMAT_B8G8R8_SRGB &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ){
            surface_format = fmt;
            selected = true;
            break;
        }
    }
    if(!selected)surface_format = swap_chain_details.formats[0];

    lg(Info,
        *translator,
        "ok.select_format",
        (int)surface_format.format,
        (int)surface_format.colorSpace
    ) << endlog;
}

static SwapChainSupportDetails query_swap_chain_details(
    VkPhysicalDevice dev,
    VkSurfaceKHR surface
){
    SwapChainSupportDetails det;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev,surface, &det.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev,surface,&format_count, nullptr);
    if(format_count){
        det.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev,surface,&format_count, det.formats.data());
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev,surface,&present_mode_count,nullptr);
    if(present_mode_count){
        det.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev,surface,&present_mode_count,det.present_modes.data());
    }

    return det;
} 

static bool check_required_extensions(
    VkPhysicalDevice physical_device,
    std::vector<const char*> & fill_in,
    const std::vector<const char *>& exts,
    std::vector<std::string> & verbose_names,
    bool verbose = false
){
    /// 尝试获取所有支持的extensions
    uint32_t dev_ext_c;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &dev_ext_c, nullptr);
    std::vector<VkExtensionProperties> vk_extensions(dev_ext_c);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &dev_ext_c, vk_extensions.data());
    std::unordered_set<std::string_view> vk_existing_extensions;
    
    for(auto & p : vk_extensions){
        vk_existing_extensions.emplace(p.extensionName);
        if(verbose){
            auto& str = verbose_names.emplace_back(p.extensionName);
            str += "(v";
            str += ext::vulkan_api_to_string<false>(p.specVersion);
            str += ")";
        }
    }

    fill_in.clear();
    for(auto & data : exts){
        auto it = vk_existing_extensions.find(data);
        if(it != vk_existing_extensions.end()){
            // 指向长久缓存
            fill_in.emplace_back(data);
        }else{
            return false;
        }
    }
    return true;
}

static QueueFamilyIndices find_queue_families(VkPhysicalDevice dev,VkSurfaceKHR surface){
    QueueFamilyIndices q;
    uint32_t qf_count;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, properties.data());

    VkBool32 support_present = VK_FALSE;

    size_t i = 0;
    for(auto & p : properties){
        support_present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &support_present);
        bool support_graphics = p.queueFlags & VK_QUEUE_GRAPHICS_BIT;

        if(!q.graphics && support_graphics){
            q.graphics.emplace(i);
        }
        if(!q.present && support_present){
            q.present.emplace(i);
        }

        if(support_graphics && support_present){
            q.graphics.emplace(i);
            q.present.emplace(i);
            break;
        }
        ++i;
    }
    return q;
}

void App::_vk_create_window_surface(){
    if(VkResult result = glfwCreateWindowSurface(instance, window, allocator, &surface)){
        lg(Error,*translator,"bad.create_surface",(int)result) << endlog;
        throw "BG";
    }
    defer_mgr.defer([this]{
        vkDestroySurfaceKHR(instance, surface, allocator);
    });

    lg(Info,*translator,"ok.create_surface") << endlog;
}

void App::_vk_create_logical_device(){
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    std::unordered_set<uint32_t> unique_queue_families = {
        *queue_family.graphics,
        *queue_family.present
    };
    /// @todo 难道这个也可以配置?后面再讲
    float queue_priority = 1.0;
    for(auto & v : unique_queue_families){
        auto & info = queue_infos.emplace_back(VkDeviceQueueCreateInfo{});

        info.sType = VST(DEVICE_QUEUE_CREATE_INFO);
        info.queueFamilyIndex = v;
        info.queueCount = 1;
        info.pQueuePriorities = &queue_priority;
    }

    VkDeviceCreateInfo create_info {};
    create_info.sType = VST(DEVICE_CREATE_INFO);
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.queueCreateInfoCount = queue_infos.size();
    create_info.pEnabledFeatures = &device_features;
    // 据说Vulkan 1.1中这里就不需要了,自动继承instance的处理
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;
    create_info.enabledExtensionCount = valid_device_extensions.size();
    create_info.ppEnabledExtensionNames = valid_device_extensions.data();

    if(VkResult result = vkCreateDevice(physical_device, &create_info,allocator, &device)){
        lg << translator->translate(
            "bad.vk_device",
            (int)result
        );
        throw "BAD GUY!";
    }
    defer_mgr.defer([this]{
        vkDestroyDevice(device,allocator);
    });

    lg(Info) << translator->translate(
        "ok.vk_device",
        valid_device_extensions
    ) << endlog;

    /// 获取的队列
    // 图形
    vkGetDeviceQueue(device,*queue_family.graphics,0,&graphics_queue);
    // 呈现
    vkGetDeviceQueue(device,*queue_family.present,0,&present_queue);
}

void App::_vk_pick_physical_device(){
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance,&physical_device_count,nullptr);
    if(physical_device_count == 0){
        lg(Error) << translator->translate("bad.no_phy_dev") << endlog;
        throw "Bad GUY!";
    }
    std::vector<VkPhysicalDevice> devices(physical_device_count);
    std::vector<VkPhysicalDeviceProperties> properties;
    std::vector<VkPhysicalDeviceFeatures> features;
    std::vector<std::string> device_info;
    vkEnumeratePhysicalDevices(instance,&physical_device_count,devices.data());
    
    /// 构建用户扩展需求
    std::vector<const char *> user_required;
    for(auto & s : config.jump("/vulkan/device_extensions").array()){
        user_required.emplace_back(s.to<std::string_view>().data());
    }
    std::vector<std::string> device_infos;
    bool verbose = config.jump("/vulkan/verbose_extensions").to<bool>();

    size_t index = 0;
    double highest_score = 0;
    int highest_index = -1;
    QueueFamilyIndices highest_qf;
    SwapChainSupportDetails highest_swap;

    double mul_api_score = config.jump("/vulkan/score_multiplier/api_version").to<double>();
    double mul_img2d_score = config.jump("/vulkan/score_multiplier/image_dim2d").to<double>();
    double mul_discrete = config.jump("/vulkan/score_multiplier/discrete_gpu").to<double>();
    bool need_geometry = config.jump("/vulkan/score_multiplier/need_geometry").to<bool>();
    bool fail_load = config.jump("/vulkan/score_multiplier/fail_load").to<bool>();
    
    lg(*translator,"check.begin_scoring") << endlog;

    // 这个留作占位,理论上最终结果就是user_required
    std::vector<const char *> valid_exts;
    for(auto & dev : devices){
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
        str += ext::vulkan_api_to_string<false>(deviceProperties.apiVersion);

        /// 计算分数以及highest
        double api_score = 
            (VK_API_VERSION_MAJOR(deviceProperties.apiVersion) * 1024 + 
            VK_API_VERSION_MINOR(deviceProperties.apiVersion)) * mul_api_score;

        double image_score = deviceProperties.limits.maxImageDimension2D * mul_img2d_score;

        score = api_score + image_score;
        // 如果是独显,那么最终分数乘上multiplier
        if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)score *= mul_discrete;
        
        auto queue_family = find_queue_families(dev,surface);
        bool extension_supports = check_required_extensions(dev,valid_exts,user_required,device_infos,verbose);

        // 这里是必需的成分
        if(need_geometry && deviceFeatures.geometryShader == VK_FALSE)score = -1;
        else if(fail_load)score = -2; // 固定设置
        else if(!queue_family.is_complete()){
            score = -3;
        }else if(!extension_supports){
            valid_device_extensions.clear();
            score = -4;
        }
        // swap chain部分
        SwapChainSupportDetails swap_details;
        if(extension_supports){
            swap_details = query_swap_chain_details(dev, surface);
            if(swap_details.formats.empty() || swap_details.present_modes.empty()){
                score = -5;
            }
        }
        
        // 结算
        if(score > highest_score){
            highest_index = index;
            highest_score = score;
            highest_qf = queue_family;
            highest_swap = swap_details;
            std::memcpy(&device_properties,&deviceProperties,sizeof(deviceProperties));
            std::memcpy(&device_features, &deviceFeatures,sizeof(deviceFeatures));
        }

        if(verbose){
            lg << translator->translate("check.gpu_score_with_exts",
                device_info[index],
                score,
                device_infos.size()
            ) << LOG_COLOR3(Cyan,None,Dim) << device_infos <<  endlog;  
            device_infos.clear();
        }else{
            lg << translator->translate("check.gpu_score",
                device_info[index],
                score
            ) << endlog;  
        }   
        ++index;
    }
    // 目前,理论上就是这样的
    valid_device_extensions = user_required;
    swap_chain_details = highest_swap;

    lg << translator->translate("check.physical_device",
        device_info.size()
    ) << LOG_COLOR3(Cyan, None, Dim) << device_info << endlog;

    if(highest_index == -1){
        lg(Error,*translator,"bad.no_gpu_satisfiable",user_required) << endlog;
        throw "BG";
    }

    physical_device = devices[highest_index];
    queue_family = highest_qf;

    lg(Info) << translator->translate("ok.selecting_gpu",
        device_info[highest_index],
        highest_score
    ) << endlog;
}

void App::_vk_setup_debug_callback(){
    // 设置 allow post
    allow_posts[0] = config.jump("/vulkan/debug_allow/error").to<bool>();
    allow_posts[1] = config.jump("/vulkan/debug_allow/warn").to<bool>();
    allow_posts[2] = config.jump("/vulkan/debug_allow/info").to<bool>();
    allow_posts[3] = config.jump("/vulkan/debug_allow/verbose").to<bool>();
    allow_posts[4] = config.jump("/vulkan/debug_allow/others").to<bool>();

    // 设置回调
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
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
    create_info.pUserData = (void*)this;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
    if(func){
        if(VkResult result = func(instance,&create_info,allocator,&debug_messenger)){
            lg(Error) << translator->translate("bad.create_debug_messenger",
                (int)result
            ) << endlog;
            throw "BAD GUY!";
        }
        lg(Info) << translator->translate("ok.create_debug_messenger") << endlog;
    }else{
        lg(Error) << translator->translate("bad.debug_messenger") << endlog;
        // 可以继续运行
        return;
    }

    defer_mgr.defer([this]{
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
        if(func)func(instance, debug_messenger, allocator);
        else{
            lg(Error) << translator->translate("bad.destroy_debug_messenger") << endlog;
        }
    });
}

void App::_vk_create_instance(){
    VkApplicationInfo app_info {};
    app_info.sType = VST(APPLICATION_INFO);
    app_info.pApplicationName = config.jump("/vulkan/app_name").to<std::string_view>().data();
    app_info.applicationVersion = VK_MAKE_VERSION(
        config.jump("/vulkan/app_version")[0].to<uint32_t>(),
        config.jump("/vulkan/app_version")[1].to<uint32_t>(),
        config.jump("/vulkan/app_version")[2].to<uint32_t>() 
    );
    app_info.pEngineName = config.jump("/vulkan/engine_name").to<std::string_view>().data();
    app_info.engineVersion = VK_MAKE_VERSION(
        config.jump("/vulkan/engine_version")[0].to<uint32_t>(),
        config.jump("/vulkan/engine_version")[1].to<uint32_t>(),
        config.jump("/vulkan/engine_version")[2].to<uint32_t>() 
    );
    app_info.apiVersion = VK_MAKE_API_VERSION(
        config.jump("/vulkan/api_version")[0].to<uint32_t>(),
        config.jump("/vulkan/api_version")[1].to<uint32_t>(),
        config.jump("/vulkan/api_version")[2].to<uint32_t>(),
        config.jump("/vulkan/api_version")[3].to<uint32_t>()
    );

    // 对Vulkan扩展进行检测
    uint32_t vk_extc;
    vkEnumerateInstanceExtensionProperties(nullptr,&vk_extc,nullptr);
    std::vector<VkExtensionProperties> vk_extensions(vk_extc);
    vkEnumerateInstanceExtensionProperties(nullptr,&vk_extc,vk_extensions.data());
    std::unordered_set<std::string_view> vk_existing_extensions;
    {
        std::vector<std::string> str_extensions;
        for(auto & p : vk_extensions){
            vk_existing_extensions.emplace(p.extensionName);
            auto & s = str_extensions.emplace_back(
                p.extensionName
            );
            s += "(v";
            s +=  ext::vulkan_api_to_string<false>(p.specVersion);
            s += ")";
        }
        
        lg << translator->translate("check.vk_instance_ext",
            vk_extc
        ) << LOG_COLOR3(Cyan,None,Dim) << str_extensions << endlog;
    }

    // 检查Layer扩展
    uint32_t vk_layerc = 0;
    vkEnumerateInstanceLayerProperties(&vk_layerc,nullptr);
    std::vector<VkLayerProperties> available_layers(vk_layerc);
    vkEnumerateInstanceLayerProperties(&vk_layerc,available_layers.data());
    std::unordered_set<std::string_view> vk_existing_layers;
    {
        std::vector<std::string> str_layers;
        for(auto & p : available_layers){
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
        ) << LOG_COLOR3(Cyan,None,Dim) << str_layers << endlog;
    }

    VkInstanceCreateInfo create_info {};
    create_info.sType = VST(INSTANCE_CREATE_INFO);
    create_info.pApplicationInfo = &app_info;

    // 检查GLFW扩展支持
    // 来点颜色瞧瞧
    uint32_t glfw_extc = 0;
    const char ** glfw_exts;
    glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_extc);
    lg << translator->translate("check.glfw_ext",glfw_extc)
       << LOG_COLOR3(Cyan,None,Dim) 
       << std::span(glfw_exts,glfw_extc)<< endlog;

    /// 加入新的支持
    std::vector<const char *> required_extensions;
    required_extensions.insert(
        required_extensions.begin(),
        glfw_exts,
        glfw_exts + glfw_extc
    );
    /// 用户自定义的extension
    {
        std::vector<std::string_view> unfound_extensions;
        for(auto & s : config.jump("/vulkan/instance_extensions").array()){
            auto it = vk_existing_extensions.find(s.to<std::string_view>());
            if(it != vk_existing_extensions.end()){
                required_extensions.emplace_back(it->data());
                
                if(*it == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME){
                    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                }else if(*it == VK_EXT_DEBUG_UTILS_EXTENSION_NAME){
                    --enable_validation_layer_steps;
                }
            }else{
                unfound_extensions.emplace_back(s.to<std::string_view>());
            }
        }
        if(unfound_extensions.size()){
            lg(Error) << 
                translator->translate("bad.ext_not_found",
                    unfound_extensions.size(),
                    unfound_extensions
                )
            << endlog;
            throw "Bad Guy";
        }
    }

    create_info.enabledExtensionCount = required_extensions.size();
    create_info.ppEnabledExtensionNames = required_extensions.data();

    // 用户选择的中间层
    std::vector<const char *> required_layers;
    {
        std::vector<std::string_view> unfound_layers;
        for(auto & s : config.jump("/vulkan/layers").array()){
            auto it = vk_existing_layers.find(s.to<std::string_view>());
            if(it != vk_existing_layers.end()){
                required_layers.emplace_back(it->data());
                if(*it == "VK_LAYER_KHRONOS_validation"){
                    --enable_validation_layer_steps;
                }
            }else{
                unfound_layers.emplace_back(s.to<std::string_view>());
            }
        }

        if(unfound_layers.size()){
            lg(Error) << 
                translator->translate("bad.layer_not_found",
                    unfound_layers.size(),
                    unfound_layers
                )
            << endlog;
            throw "Bad Guy";
        }
    }

    create_info.enabledLayerCount = required_layers.size();
    create_info.ppEnabledLayerNames = required_layers.data();

    if(VkResult result = vkCreateInstance(&create_info,allocator,&instance)){
        lg << translator->translate(
            "bad.vk_instance",
            (int)result
        );
        throw "BAD GUY!";
    }
    defer_mgr.defer([this]{
        vkDestroyInstance(instance,allocator);
    });

    lg(Info) << translator->translate(
        "ok.vk_instance",
        config.jump("/vulkan/api_version").str(data::JSON(
            data::JSONConfig{
                .compact_lines = true,
                .compact_spaces = true
            }
        )),
        required_extensions,
        required_layers
    ) << endlog;
}