#ifndef LEARN_VK_APP_H
#define LEARN_VK_APP_H
#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/atranslator.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool is_complete(){
        return (bool)graphics && (bool)present;
    }
};

struct SwapChainSupportDetails{
    VkSurfaceCapabilitiesKHR capabilities {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct App{
    /// 日志
    alib5::Logger logger;
    alib5::LogFactory lg;
    alib5::LogFactory vk_validation_lg;

    /// 配置文件
    alib5::AData & config;

    /// 语言类
    alib5::Translator full_translator;
    std::optional<alib5::FlattenTranslator> translator;

    /// 窗口
    GLFWwindow * window;

    /// VK分配器
    VkAllocationCallbacks * allocator { nullptr };

    App(alib5::AData & iconfig,alib5::LoggerConfig cfg0,alib5::LogFactoryConfig cfg1,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
    :config(iconfig)
    ,logger(cfg0)
    ,lg(logger,cfg1)
    ,vk_validation_lg(logger,"VulkanValidation")
    ,full_translator(__a){}

    ~App(){ endup(); }

    void setup();
    void _setup_logger();
    void _setup_language();
    void _setup_glfw();
    void _setup_vulkan();

    int run();
    void endup();
    void draw();

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
    VkCommandBuffer cmd_buffer { VK_NULL_HANDLE };
    VkSemaphore sem_img_available { VK_NULL_HANDLE };
    VkSemaphore sem_render_fin { VK_NULL_HANDLE };
    VkFence fen_in_flight { VK_NULL_HANDLE };

    /// 指向不会变的adata
    std::vector<const char *> valid_device_extensions;
    int enable_validation_layer_steps { 2 };
    bool allow_posts [5] {true};


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

    void vk_record_command_buffer(VkCommandBuffer buffer,uint32_t image_index);

    // 资源释放,要做到最晚定义这样才能最早析构
    alib5::misc::DeferManager defer_mgr;
};

#endif