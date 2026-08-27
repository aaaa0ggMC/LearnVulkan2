#ifndef LEARN_VK_APP_H
#define LEARN_VK_APP_H

#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include <memory_resource>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "schema.h"

import alib6;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool is_complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct App {
    /// 强类型反射配置与动态数据
    ApplicationConfig & app_cfg;
    alib6::AData & config;

    /// 日志
    alib6::log::Logger logger;
    alib6::log::LogFactory lg;
    alib6::log::LogFactory vk_validation_lg;

    /// 语言类
    alib6::Translator full_translator;
    std::optional<alib6::FlattenTranslator> translator;

    /// 窗口
    GLFWwindow * window { nullptr };

    /// VK分配器
    VkAllocationCallbacks * allocator { nullptr };

    App(
        ApplicationConfig & iapp_cfg,
        alib6::AData & iconfig,
        std::pmr::memory_resource * __a = alib6::get_default_resource()
    )
        : app_cfg(iapp_cfg)
        , config(iconfig)
        , logger(app_cfg.actual_logger, __a)
        , lg(logger, app_cfg.actual_factory)
        , vk_validation_lg(logger, "VulkanValidation")
        , full_translator(__a)
        , defer_mgr(__a) {}

    ~App() { endup(); }

    void setup();
    void _setup_logger();
    void _setup_language();
    void _setup_glfw();
    void _setup_vulkan();

    int run();
    void endup();
    void draw(size_t &);

    /// Vulkan相关
    VkInstance instance { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT debug_messenger { VK_NULL_HANDLE };
    VkSurfaceKHR surface { VK_NULL_HANDLE };
    VkPhysicalDevice physical_device { VK_NULL_HANDLE };
    QueueFamilyIndices queue_family {};
    VkPhysicalDeviceProperties device_properties {};
    VkPhysicalDeviceFeatures device_features {};
    VkDevice device { VK_NULL_HANDLE };
    VkQueue graphics_queue { VK_NULL_HANDLE };
    VkQueue present_queue { VK_NULL_HANDLE };
    SwapChainSupportDetails swap_chain_details {};
    VkSurfaceFormatKHR surface_format {};
    VkPresentModeKHR present_mode {};
    VkExtent2D swap_extent {};
    VkSwapchainKHR swapchain { VK_NULL_HANDLE };
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_views; 
    VkRenderPass render_pass { VK_NULL_HANDLE };
    VkPipelineLayout pipeline_layout { VK_NULL_HANDLE };
    VkPipeline graphics_pipeline { VK_NULL_HANDLE };
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool cmd_pool { VK_NULL_HANDLE };
    std::vector<VkCommandBuffer> cmd_buffers;

    uint32_t sync_object_count { 0 };
    std::vector<VkSemaphore> sem_img_available;
    std::vector<VkSemaphore> sem_render_fin;
    std::vector<VkFence> fen_in_flights;

    /// 指向不会变的设备扩展与验证配置
    std::vector<const char *> valid_device_extensions;
    int enable_validation_layer_steps { 2 };
    bool allow_posts [5] {true, true, true, true, true};

    void _vk_create_instance();
    void _vk_setup_debug_callback();
    void _vk_pick_physical_device();
    void _vk_create_logical_device();
    void _vk_create_window_surface();
    void _vk_choose_swap_surface_format();
    void _vk_choose_present_mode();
    void _vk_choose_swap_extent();
    void _vk_create_swap_chain();
    void _vk_create_image_views();
    void _vk_create_graphics_pipeline();
    void _vk_create_render_pass();
    void _vk_create_framebuffers();
    void _vk_create_command_pool();
    void _vk_create_command_buffer();
    void _vk_create_sync_objects();

    void vk_record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);

    // 资源释放管理器 (最晚声明最早析构)
    alib6::DeferManager defer_mgr;
};

#endif // LEARN_VK_APP_H
