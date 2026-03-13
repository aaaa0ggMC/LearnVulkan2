#ifndef LEARN_VK_APP_H
#define LEARN_VK_APP_H
#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/atranslator.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices{
    std::optional<uint32_t> graphics;

    bool is_complete(){
        return (bool)graphics;
    }
};

struct App{
    /// 配置文件
    alib5::AData & config;

    /// 日志
    alib5::Logger logger;
    alib5::LogFactory lg;
    alib5::LogFactory vk_validation_lg;

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

    /// Vulkan相关
    VkInstance instance { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT debug_messenger { VK_NULL_HANDLE };
    VkPhysicalDevice physical_device { VK_NULL_HANDLE };
    QueueFamilyIndices queue_family;

    int enable_validation_layer_steps { 2 };
    bool allow_posts [5] {true};


    void _vk_create_instance();
    void _vk_setup_debug_callback();
    void _vk_pick_physical_device();
};

#endif