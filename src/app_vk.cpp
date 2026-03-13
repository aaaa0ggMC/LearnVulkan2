#include <app.h>
#include <alib5/aperf.h>
#define VST(X) VK_STRUCTURE_TYPE_##X

using namespace alib5;
using enum Severity;

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

void App::_setup_vulkan(){
    /// 初始化instance
    _vk_create_instance();
    /// 初始化VulkanCallback
    if(!enable_validation_layer_steps)_vk_setup_debug_callback();
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
            s += 
                std::to_string(VK_API_VERSION_MAJOR(p.specVersion)) + "." +
                std::to_string(VK_API_VERSION_MINOR(p.specVersion)) + "." +
                std::to_string(VK_API_VERSION_PATCH(p.specVersion));
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
        for(auto & s : config.jump("/vulkan/extensions").array()){
            auto it = vk_existing_extensions.find(s.to<std::string_view>());
            if(it != vk_existing_extensions.end()){
                required_extensions.emplace_back(it->data());
                
                if(*it == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME){
                    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                }else if(*it == "VK_EXT_debug_utils"){
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