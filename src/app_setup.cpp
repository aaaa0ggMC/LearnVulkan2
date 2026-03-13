#include <app.h>
#include <alib5/aclock.h>
#include <vulkan/vulkan.h>

using namespace alib5;
using enum Severity;

void App::setup(){
    _setup_logger();
    _setup_language();
    _setup_glfw();
}

void App::endup(){
    // 处理GLFW相关
    glfwDestroyWindow(window);
    glfwTerminate();
}

void App::_setup_glfw(){
    glfwInit();

    // 检查vulkan支持
    if(glfwVulkanSupported()){
        lg(Info) << translator->translate("ok.vulkan") << endlog;
    }else{
        lg(Error) << translator->translate("bad.vulkan") << endlog;
        throw "Bad GUY!";
    }

    // 初始化GLFW
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    // 目前处理resize比较复杂
    ///  @todo 未来学习过程中尝试支持
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE );

    // 创建窗口
    auto width = config.jump("/window/width").to<uint32_t>();
    auto height = config.jump("/window/height").to<uint32_t>();
    auto title = config.jump("/window/title").to<std::string_view>();

    window = glfwCreateWindow(
        width,
        height,
        title.data(),
        nullptr,
        nullptr
    );

    if(!window){
        const char * desc;
        int code = glfwGetError(&desc);
        lg(Error) << translator->translate("bad.window",width,height,title,code,desc) << endlog;
        throw "Bad GUY!";
    }

    lg(Info) << translator->translate("ok.window",width,height,title) << endlog;

    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr,&extension_count,nullptr);
    lg << extension_count << " extensions are supported!" << endlog;
}

void App::_setup_language(){
    full_translator.load_from_entry(
        io::load_entry("./data/translations")
    );

    full_translator.switch_language(config["language"].to<std::string_view>());
    auto t = full_translator.flatten_dots();
    if(!t){
        std::vector<std::string> supports;
        for(auto proxy : full_translator.data().object()){
            supports.emplace_back(proxy.first());
        }
        lg << "Translations failed to load!Supported languages:" << supports << alib5::endlog;
        throw "BAD GUY!";
    }
    translator.emplace(*t);
    lg << translator->translate<false>(
        "test",
        translator->translate("title")
    ) << endlog;
}

void App::_setup_logger(){
    logger.append_mod<lot::Console>("console");
    logger.append_mod<lot::RotateFile>("file",lot::RotateFileConfig("latest{1}.log"));

    lg << "Log system has initialized." << alib5::endlog;
}