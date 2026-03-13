#include <app.h>
#include <alib5/aperf.h>

#define VST(X) VK_STRUCTURE_TYPE_##X

using namespace alib5;
using enum Severity;

void App::_setup_vulkan(){
    /// 初始化instance
    _vk_create_instance();
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
        if(vk_extc){
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
        }
        
        lg << translator->translate("check.vk_instance_ext",
            vk_extc
        ) << LOG_COLOR3(Cyan,None,Dim) << str_extensions << endlog;
    }

    VkInstanceCreateInfo create_info {};
    create_info.sType = VST(INSTANCE_CREATE_INFO);
    create_info.pApplicationInfo = &app_info;

    // 检查GLFW扩展支持
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

    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    create_info.enabledExtensionCount = required_extensions.size();
    create_info.ppEnabledExtensionNames = required_extensions.data();

    // 中间层
    create_info.enabledLayerCount = 0;

    if(VkResult result = vkCreateInstance(&create_info,allocator,&instance)){
        lg << translator->translate(
            "bad.vk_instance",
            (int)result
        );
        throw "BAD GUY!";
    }
    lg << translator->translate(
        "ok.vk_instance",
        config.jump("/vulkan/api_version").str(data::JSON(
            data::JSONConfig{
                .compact_lines = true,
                .compact_spaces = true
            }
        )),
        required_extensions
    ) << endlog;
}